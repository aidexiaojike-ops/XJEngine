#include "Asset/Importer/XJModelImporter.h"
// #include <tiny_gltf.h>
#include "spdlog/spdlog.h"


namespace XJ
{
    std::shared_ptr<XJMeshAsset> XJGltfImporter::ExtractMesh(const tinygltf::Model& model, int meshIndex)
    {   
        auto kMeshAsset = std::make_shared<XJMeshAsset>();
        kMeshAsset->mHandle = XJAsset::GenerateHandle();
        kMeshAsset->mType = XJAssetType::Mesh;
         // 遍历所有 mesh → primitive → 收集顶点/索引
        for(const auto& kMesh : model.meshes)
        {
            for(const auto& kPrimitive : kMesh.primitives)
            {
                uint32_t kFirstVertex = static_cast<uint32_t>(kMeshAsset->mVertices.size());// 记录当前顶点数量，后续索引需要基于此偏移
                // 处理属性
                if(kPrimitive.attributes.count("POSITION"))
                {
                    const auto& kAccessor = model.accessors[kPrimitive.attributes.at("POSITION")];
                    const auto& kBufferView = model.bufferViews[kAccessor.bufferView];
                    const auto& kBuffer = model.buffers[kBufferView.buffer];
                    const float* kDataPtr = reinterpret_cast<const float*>(kBuffer.data.data() + kBufferView.byteOffset + kAccessor.byteOffset);
                    
                    size_t kVertexCount = kAccessor.count;
                    for(size_t i=0; i<kVertexCount; ++i)
                    {
                        Vertex kVertex{};
                        kVertex.Position[0] = kDataPtr[i*3 + 0];
                        kVertex.Position[1] = kDataPtr[i*3 + 1];
                        kVertex.Position[2] = kDataPtr[i*3 + 2];
                        kMeshAsset->mVertices.push_back(kVertex);// 先只处理位置，后续可以扩展法线、UV等属性
                    } 
                }
                if (kPrimitive.attributes.count("NORMAL"))
                {
                    const auto& kAccessor = model.accessors[kPrimitive.attributes.at("NORMAL")];
                    const auto& kBufferView = model.bufferViews[kAccessor.bufferView];
                    const auto& kBuffer = model.buffers[kBufferView.buffer];
                    const float* kDataPtr = reinterpret_cast<const float*>(kBuffer.data.data() + kBufferView.byteOffset + kAccessor.byteOffset);
                    size_t kVertexCount = kAccessor.count;
                    for(size_t i=0; i<kVertexCount; ++i)                    {
                        kMeshAsset->mVertices[kFirstVertex + i].Normal[0] = kDataPtr[i*3 + 0];
                        kMeshAsset->mVertices[kFirstVertex + i].Normal[1] = kDataPtr[i*3 + 1];
                        kMeshAsset->mVertices[kFirstVertex + i].Normal[2] = kDataPtr[i*3 + 2];
                    }
                }
                if (kPrimitive.attributes.count("TEXCOORD_0"))
                {
                    const auto& kAccessor = model.accessors[kPrimitive.attributes.at("TEXCOORD_0")];
                    const auto& kBufferView = model.bufferViews[kAccessor.bufferView];
                    const auto& kBuffer = model.buffers[kBufferView.buffer];
                    const float* kDataPtr = reinterpret_cast<const float*>(kBuffer.data.data() + kBufferView.byteOffset + kAccessor.byteOffset);
                    size_t kVertexCount = kAccessor.count;
                    for(size_t i=0; i<kVertexCount; ++i)                    {
                        kMeshAsset->mVertices[kFirstVertex + i].UV[0] = kDataPtr[i*2 + 0];
                        kMeshAsset->mVertices[kFirstVertex + i].UV[1] = kDataPtr[i*2 + 1];
                    }
                }

                // index（支持 UNSIGNED_BYTE / SHORT / INT 三种 glTF 索引类型）
                if (kPrimitive.indices >= 0)
                {
                    const auto& kAccessor = model.accessors[kPrimitive.indices];
                    const auto& kBufferView = model.bufferViews[kAccessor.bufferView];
                    const auto& kBuffer = model.buffers[kBufferView.buffer];
                    const uint8_t* kDataPtr = kBuffer.data.data()
                        + kBufferView.byteOffset + kAccessor.byteOffset;

                    switch (kAccessor.componentType)
                    {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            for (size_t i = 0; i < kAccessor.count; ++i)
                                kMeshAsset->mIndices.push_back(kFirstVertex + kDataPtr[i]);
                            break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                            for (size_t i = 0; i < kAccessor.count; ++i)
                                kMeshAsset->mIndices.push_back(kFirstVertex +
                                    reinterpret_cast<const uint16_t*>(kDataPtr)[i]);
                            break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                            for (size_t i = 0; i < kAccessor.count; ++i)
                                kMeshAsset->mIndices.push_back(kFirstVertex +
                                    reinterpret_cast<const uint32_t*>(kDataPtr)[i]);
                            break;
                        default:
                            spdlog::error("glTF: unsupported index component type {}", kAccessor.componentType);
                            break;
                    }
                }
            }
        }

        // kReshAsset->mHandle = XJAsset::GenerateHandle();
        return kMeshAsset;
    }

    bool XJGltfImporter::LoadMeshAsset(const std::string& path)
    {
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        if (!loader.LoadBinaryFromFile(&mModel, &err, &warn, path))
        {
            spdlog::error("XJGltfImporter::Load failed: {}", path);
            return false;
        }
        mFilePath = path;
        return true;
    }
   
}