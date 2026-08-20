#include "Render/Resource/XJMeshFactory.h"
#include "Graphic/XJVulkanGeometryUtil.h"

#include <spdlog/spdlog.h>
#include <exception>
#include <utility>
#include <limits>

namespace XJ
{
    std::shared_ptr<XJMesh>XJMeshFactory::CreateFromAsset(const XJMeshAsset& asset)
    {
        if (asset.mVertices.empty())
        {
            spdlog::error("XJMeshFactory::CreateFromAsset failed: asset has no vertices.");
            return nullptr;
        }

        if(asset.mIndices.empty())
        {
            spdlog::error("XJMeshFactory::CreateFromAsset " "failed: asset has no indices.");

            return nullptr;
        }
        if(asset.mVertices.size() > std::numeric_limits<uint32_t>::max() || asset.mIndices.size()> std::numeric_limits<uint32_t>::max())
        {
            spdlog::error("XJMeshFactory::CreateFromAsset " "failed: mesh exceeds uint32_t range.");

            return nullptr;
        }

        std::vector<XJVulkanVertex> vertices;//转换 Vertex（Asset 格式）→ XJVulkanVertex（GPU 格式）
        vertices.reserve(asset.mVertices.size());//预留空间，避免重复分配
        for(const auto& vertex : asset.mVertices)
        {
            XJVulkanVertex gpuVertex{};
            gpuVertex.position = vertex.Position;
            gpuVertex.normal = vertex.Normal;
            gpuVertex.tangent = vertex.Tangent;
            gpuVertex.texcoord0 = vertex.UV;

            vertices.push_back(gpuVertex);
        }

        std::vector<XJSubmesh> submeshes;
        if(asset.mPrimitives.empty())
        {
            // 兼容旧资产：没有 primitive metadata 时，
             // 整个 index buffer 作为 material slot 0。
            XJSubmesh fallbackSubmesh;
            fallbackSubmesh.FirstIndex = 0;
            fallbackSubmesh.IndexCount = static_cast<uint32_t>(asset.mIndices.size());
            fallbackSubmesh.MaterialSlot = 0;

            submeshes.push_back(fallbackSubmesh);
        }
        else
        {
            submeshes.reserve(asset.mPrimitives.size());
            const uint32_t totalIndexCount = static_cast<uint32_t>(asset.mIndices.size());

            for(const XJMeshPrimitive& primitive : asset.mPrimitives)
            {
                if(!primitive.IsValid(totalIndexCount))
                {
                     spdlog::error(
                        "XJMeshFactory::CreateFromAsset "
                        "failed: invalid primitive range, "
                        "firstIndex={}, indexCount={}, "
                        "totalIndices={}.",
                        primitive.FirstIndex,
                        primitive.IndexCount,
                        totalIndexCount);

                    return nullptr;
                }

                XJSubmesh submesh;
                submesh.FirstIndex = primitive.FirstIndex;
                submesh.IndexCount = primitive.IndexCount;
                submesh.MaterialSlot = primitive.MaterialSlot;

                submeshes.push_back(submesh);
            }
        }

        try
        {
            return std::make_shared<XJMesh>(vertices, asset.mIndices, std::move(submeshes));
        }
        catch (const std::exception& e)
        {
            spdlog::error("XJMeshFactory::CreateFromAsset failed: {}", e.what());
            return nullptr;
        }//创建 XJMesh 实例，传入转换后的顶点数据和索引数据
    }

    std::shared_ptr<XJMesh> XJMeshFactory::CreateCubeMesh()
    {
        std::vector<XJVulkanVertex> vertices;
        std::vector<uint32_t> indices;

        XJVulkanGeometryUtil::CreateCube(//自带cube的设置
            -0.5f,
             0.5f,
            -0.5f,
             0.5f,
            -0.5f,
             0.5f,
            vertices,
            indices,
            true,
            true,
            glm::mat4(1.0f));

        if (vertices.empty())
        {
            spdlog::error("XJMeshFactory::CreateCubeMesh failed: generated cube has no vertices.");
            return nullptr;
        }

        try
        {
            std::vector<XJSubmesh> submeshes;

            XJSubmesh cubeSubmesh;
            cubeSubmesh.FirstIndex = 0;
            cubeSubmesh.IndexCount = static_cast<uint32_t>(indices.size());
            cubeSubmesh.MaterialSlot = 0;

            submeshes.push_back(cubeSubmesh);

            return std::make_shared<XJMesh>(
                vertices,
                indices,
                std::move(submeshes));
        }
        catch (const std::exception& e)
        {
            spdlog::error("XJMeshFactory::CreateCubeMesh failed: {}", e.what());
            return nullptr;
        }
    }
}