#include "Asset/Importer/XJModelImporter.h"
// #include <tiny_gltf.h>
#include "spdlog/spdlog.h"
#include <cstring>
#include <limits>


namespace XJ
{
    std::shared_ptr<XJMeshAsset> XJGltfImporter::ExtractMesh(const tinygltf::Model& model, int meshIndex)
    {
        auto meshAsset = std::make_shared<XJMeshAsset>();
        meshAsset->mHandle = XJAsset::GenerateHandle();
        meshAsset->mType = XJAssetType::Mesh;
        
        // 统一校验 glTF accessor，避免 bufferView=-1、格式不匹配、stride 错误或越界读。
        // 返回的 data 指向 accessor 第一个元素，stride 是相邻元素之间的字节距离。
        auto getAccessorData = [&model](
            int accessorIndex,
            int expectedType,
            int expectedComponentType,
            const char* name,
            const tinygltf::Accessor*& accessor,
            const uint8_t*& data,
            size_t& stride,
            size_t& elementSize) -> bool
        {
            if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
            {
                spdlog::error("glTF: invalid {} accessor index {}", name, accessorIndex);
                return false;
            }

            accessor = &model.accessors[accessorIndex];

            if (accessor->sparse.isSparse)
            {
                spdlog::error("glTF: sparse accessor is not supported for {}", name);
                return false;
            }

            if (accessor->bufferView < 0 || accessor->bufferView >= static_cast<int>(model.bufferViews.size()))
            {
                spdlog::error("glTF: invalid {} bufferView {}", name, accessor->bufferView);
                return false;
            }

            if (accessor->type != expectedType || accessor->componentType != expectedComponentType)
            {
                spdlog::error("glTF: invalid {} accessor format", name);
                return false;
            }

            const tinygltf::BufferView& bufferView = model.bufferViews[accessor->bufferView];
            if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
            {
                spdlog::error("glTF: invalid {} buffer index {}", name, bufferView.buffer);
                return false;
            }

            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            elementSize =
                static_cast<size_t>(tinygltf::GetNumComponentsInType(accessor->type)) *
                static_cast<size_t>(tinygltf::GetComponentSizeInBytes(accessor->componentType));
            // glTF accessor 允许通过 byteStride 存储交错顶点数据。
            // 如果 byteStride 为 0，表示数据紧密排列，步长就是单个元素大小。
            const int byteStride = accessor->ByteStride(bufferView);
            if (byteStride < 0)
            {
                spdlog::error("glTF: invalid {} byteStride {}", name, byteStride);
                return false;
            }

            stride = byteStride > 0 ? static_cast<size_t>(byteStride) : elementSize;
            if (stride < elementSize)
            {
                spdlog::error("glTF: {} byteStride is smaller than element size", name);
                return false;
            }

            const size_t start = bufferView.byteOffset + accessor->byteOffset;
            const size_t requiredSize = accessor->count == 0 ? 0 : (accessor->count - 1) * stride + elementSize;

            if (start > buffer.data.size() || requiredSize > buffer.data.size() - start)
            {
                spdlog::error("glTF: {} accessor range exceeds buffer size", name);
                return false;
            }

            data = buffer.data.data() + start;
            return true;
        };

        auto readVec3 = [](const uint8_t* data, size_t stride, size_t index)
        {
            glm::vec3 value{};
            std::memcpy(&value, data + index * stride, sizeof(float) * 3);
            return value;
        };

        auto readVec2 = [](const uint8_t* data, size_t stride, size_t index)
        {
            glm::vec2 value{};
            std::memcpy(&value, data + index * stride, sizeof(float) * 2);
            return value;
        };
        // 只导入调用者指定的 mesh。旧代码遍历所有 mesh，会把多个 mesh 合并成一个资产。
        if (meshIndex < 0 || meshIndex >= static_cast<int>(model.meshes.size()))
        {
            spdlog::error("glTF: invalid mesh index {}", meshIndex);
            return meshAsset;
        }

        const tinygltf::Mesh& mesh = model.meshes[meshIndex];

        for (const tinygltf::Primitive& primitive : mesh.primitives)
        {
            // 第一版渲染管线只支持 triangle list。glTF mode=-1 也按规范默认 TRIANGLES。
            if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES)
            {
                spdlog::warn("glTF: primitive skipped because mode {} is not TRIANGLES", primitive.mode);
                continue;
            }

            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end())
            {
                spdlog::warn("glTF: primitive skipped because POSITION is missing");
                continue;
            }

            const tinygltf::Accessor* positionAccessor = nullptr;
            const uint8_t* positionData = nullptr;
            size_t positionStride = 0;
            size_t elementSize = 0;

            if (!getAccessorData(
                    posIt->second,
                    TINYGLTF_TYPE_VEC3,
                    TINYGLTF_COMPONENT_TYPE_FLOAT,
                    "POSITION",
                    positionAccessor,
                    positionData,
                    positionStride,
                    elementSize))
            {
                continue;
            }

            if (meshAsset->mVertices.size() > std::numeric_limits<uint32_t>::max() ||
                meshAsset->mIndices.size() > std::numeric_limits<uint32_t>::max())
            {
                spdlog::error("glTF: merged mesh exceeds uint32_t draw range");
                break;
            }

            const size_t vertexStart = meshAsset->mVertices.size();
            const size_t indexStart = meshAsset->mIndices.size();
            const uint32_t firstVertex = static_cast<uint32_t>(vertexStart);
            const uint32_t firstIndex = static_cast<uint32_t>(indexStart);

            auto rollbackPrimitive = [&]()
            {
                meshAsset->mVertices.resize(vertexStart);
                meshAsset->mIndices.resize(indexStart);
            };

            // POSITION 是创建顶点数组的基准，NORMAL/UV 必须和它 count 一致，不能用自己的 count 直接写 mVertices。
            const size_t vertexCount = positionAccessor->count;

            if (vertexCount == 0 ||
                vertexCount > std::numeric_limits<uint32_t>::max() - firstVertex)
            {
                spdlog::error("glTF: primitive vertex count is invalid or overflows uint32_t");
                continue;
            }

            for (size_t i = 0; i < vertexCount; ++i)
            {
                Vertex vertex{};
                vertex.Position = readVec3(positionData, positionStride, i);
                meshAsset->mVertices.push_back(vertex);
            }
            // NORMAL 是可选属性；如果存在但数量不匹配，跳过写入，避免越界覆盖顶点数组。
            auto normalIt = primitive.attributes.find("NORMAL");
            if (normalIt != primitive.attributes.end())
            {
                const tinygltf::Accessor* normalAccessor = nullptr;
                const uint8_t* normalData = nullptr;
                size_t normalStride = 0;

                if (getAccessorData(
                        normalIt->second,
                        TINYGLTF_TYPE_VEC3,
                        TINYGLTF_COMPONENT_TYPE_FLOAT,
                        "NORMAL",
                        normalAccessor,
                        normalData,
                        normalStride,
                        elementSize))
                {
                    if (normalAccessor->count != vertexCount)
                    {
                        spdlog::error("glTF: NORMAL count does not match POSITION count");
                    }
                    else
                    {
                        for (size_t i = 0; i < vertexCount; ++i)
                            meshAsset->mVertices[firstVertex + i].Normal = readVec3(normalData, normalStride, i);
                    }
                }
            }
            // TEXCOORD_0 是可选属性；同样必须和 POSITION 数量一致。
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end())
            {
                const tinygltf::Accessor* uvAccessor = nullptr;
                const uint8_t* uvData = nullptr;
                size_t uvStride = 0;

                if (getAccessorData(
                        uvIt->second,
                        TINYGLTF_TYPE_VEC2,
                        TINYGLTF_COMPONENT_TYPE_FLOAT,
                        "TEXCOORD_0",
                        uvAccessor,
                        uvData,
                        uvStride,
                        elementSize))
                {
                    if (uvAccessor->count != vertexCount)
                    {
                        spdlog::error("glTF: TEXCOORD_0 count does not match POSITION count");
                    }
                    else
                    {
                        for (size_t i = 0; i < vertexCount; ++i)
                            meshAsset->mVertices[firstVertex + i].UV = readVec2(uvData, uvStride, i);
                    }
                }
            }

            if (primitive.indices >= 0)
            {// getAccessorData() 校验前   primitive.indices 越界
                if (primitive.indices >= static_cast<int>(model.accessors.size()))
                {
                    spdlog::error(
                        "glTF: primitive has invalid "
                        "index accessor {}",
                        primitive.indices);
                    
                    rollbackPrimitive();
                    continue;
                }

                const tinygltf::Accessor* indexAccessor = nullptr;
                const uint8_t* indexData = nullptr;
                size_t indexStride = 0;

                if (!getAccessorData(
                        primitive.indices,
                        TINYGLTF_TYPE_SCALAR,
                        model.accessors[primitive.indices].componentType,
                        "INDICES",
                        indexAccessor,
                        indexData,
                        indexStride,
                        elementSize))
                {
                    rollbackPrimitive();
                    continue;
                }

                if (indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
                    indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
                    indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    spdlog::error("glTF: unsupported index component type {}", indexAccessor->componentType);
                    rollbackPrimitive();
                    continue;
                }

                if(indexAccessor->count == 0|| indexAccessor->count>std::numeric_limits<uint32_t>::max() - firstIndex)
                {
                    spdlog::error("glTF: primitive index count is " "invalid or overflows uint32_t");

                    rollbackPrimitive();
                    continue;
                }
                // 先构造局部索引。全部验证成功后再提交到 MeshAsset，
                // 避免单个坏索引留下不完整的 primitive。
                std::vector<uint32_t> primitiveIndices;
                primitiveIndices.reserve(indexAccessor->count);
                bool indicesValid = true;

                for (size_t indexPosition = 0; indexPosition < indexAccessor->count; ++indexPosition)
                {
                    uint32_t localIndex = 0;

                    if (indexAccessor->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        uint8_t value = 0;
                        std::memcpy(&value, indexData + indexPosition * indexStride, sizeof(value));
                        localIndex = value;
                    }
                    else if (indexAccessor->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        uint16_t value = 0;
                        std::memcpy(&value, indexData + indexPosition * indexStride, sizeof(value));
                        localIndex = value;
                    }
                    else
                    {
                        std::memcpy(&localIndex, indexData + indexPosition * indexStride, sizeof(localIndex));
                    }

                    if (localIndex >= vertexCount)
                    {
                        spdlog::error("glTF: index {} out of vertex range {}", localIndex, vertexCount);
                        indicesValid = false;
                        break;
                    }

                    if (firstVertex > std::numeric_limits<uint32_t>::max() - localIndex)
                    {
                        spdlog::error("glTF: index overflow");
                        indicesValid = false;
                        break;
                    }
                    // 索引是相对当前 primitive 顶点起点的，需要加 firstVertex 变成合并后 meshAsset 的全局索引。
                    primitiveIndices.push_back(firstVertex + localIndex);
                }

                if (!indicesValid)
                {
                    rollbackPrimitive();
                    continue;
                }

                meshAsset->mIndices.insert(
                    meshAsset->mIndices.end(),
                    primitiveIndices.begin(),
                    primitiveIndices.end());
            }
            else
            {
                // 无索引 primitive 自动生成连续索引，
                // 让运行时统一使用 vkCmdDrawIndexed。
                if(vertexCount > std::numeric_limits<uint32_t>::max() -  firstIndex)
                {
                    spdlog::error("glTF: generated index count " "overflows uint32_t");

                    rollbackPrimitive();
                    continue;
                }
                for (uint32_t localIndex = 0; localIndex < static_cast<uint32_t>(vertexCount); ++localIndex)
                    meshAsset->mIndices.push_back(firstVertex + localIndex);
            }

            const size_t appendedIndexCount = meshAsset->mIndices.size() - indexStart;
            if(appendedIndexCount == 0 || appendedIndexCount > std::numeric_limits<uint32_t>::max())
            {
                spdlog::error("glTF: generated primitive index count " "is invalid");

                rollbackPrimitive();
                continue;
            }

            const uint32_t indexCount = static_cast<uint32_t>(appendedIndexCount);
            // TRIANGLES 必须每三个索引组成一个三角形。
            if (indexCount == 0 || indexCount % 3 != 0)
            {
                spdlog::error("glTF: TRIANGLES primitive has invalid index count {}", indexCount);
                rollbackPrimitive();
                continue;
            }

            int32_t sourceMaterialIndex = primitive.material;
            if(sourceMaterialIndex >= static_cast<int32_t>(model.materials.size()))
            {
                spdlog::warn("glTF: primitive material index {} ""is out of range",sourceMaterialIndex);
                sourceMaterialIndex = -1;
            }

            XJMeshPrimitive importedPrimitive;
            importedPrimitive.FirstIndex = firstIndex;
            importedPrimitive.IndexCount = indexCount;
            // 第一版让每个有效 primitive 对应一个独立材质槽。
            importedPrimitive.MaterialSlot = static_cast<uint32_t>(meshAsset->mPrimitives.size());
            importedPrimitive.SourceMaterialIndex = sourceMaterialIndex;

            if (!importedPrimitive.IsValid(static_cast<uint32_t>(meshAsset->mIndices.size())))
            {
                spdlog::error("glTF: generated primitive draw range is invalid");
                rollbackPrimitive();
                continue;
            }

            meshAsset->mPrimitives.push_back(importedPrimitive);
        }

        return meshAsset;
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
