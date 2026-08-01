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

            const uint32_t firstVertex = static_cast<uint32_t>(meshAsset->mVertices.size());
            // POSITION 是创建顶点数组的基准，NORMAL/UV 必须和它 count 一致，不能用自己的 count 直接写 mVertices。
            const size_t vertexCount = positionAccessor->count;

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
            {
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
                    continue;
                }

                if (indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
                    indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
                    indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    spdlog::error("glTF: unsupported index component type {}", indexAccessor->componentType);
                    continue;
                }

                for (size_t i = 0; i < indexAccessor->count; ++i)
                {
                    uint32_t index = 0;

                    if (indexAccessor->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        uint8_t value = 0;
                        std::memcpy(&value, indexData + i * indexStride, sizeof(value));
                        index = value;
                    }
                    else if (indexAccessor->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        uint16_t value = 0;
                        std::memcpy(&value, indexData + i * indexStride, sizeof(value));
                        index = value;
                    }
                    else
                    {
                        std::memcpy(&index, indexData + i * indexStride, sizeof(index));
                    }

                    if (index >= vertexCount)
                    {
                        spdlog::error("glTF: index {} out of vertex range {}", index, vertexCount);
                        continue;
                    }

                    if (firstVertex > std::numeric_limits<uint32_t>::max() - index)
                    {
                        spdlog::error("glTF: index overflow");
                        continue;
                    }
                    // 索引是相对当前 primitive 顶点起点的，需要加 firstVertex 变成合并后 meshAsset 的全局索引。
                    meshAsset->mIndices.push_back(firstVertex + index);
                }
            }
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