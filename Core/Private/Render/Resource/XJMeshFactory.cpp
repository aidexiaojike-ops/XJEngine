#include "Render/Resource/XJMeshFactory.h"
#include "Graphic/XJVulkanGeometryUtil.h"

#include <spdlog/spdlog.h>
#include <exception>


namespace XJ
{
    std::shared_ptr<XJMesh>XJMeshFactory::CreateFromAsset(const XJMeshAsset& asset)
    {
         if (asset.mVertices.empty())
        {
            spdlog::error("XJMeshFactory::CreateFromAsset failed: asset has no vertices.");
            return nullptr;
        }

        std::vector<XJVulkanVertex> vertices;//转换 Vertex（Asset 格式）→ XJVulkanVertex（GPU 格式）
        vertices.reserve(asset.mVertices.size());//预留空间，避免重复分配
        for(const auto& vertex : asset.mVertices)
        {
            XJVulkanVertex kVulkanVertex{};
            kVulkanVertex.position = vertex.Position;
            kVulkanVertex.normal = vertex.Normal;
            kVulkanVertex.tangent = vertex.Tangent;
            kVulkanVertex.texcoord0 = vertex.UV;

            vertices.push_back(kVulkanVertex);
        }
        try
        {
            return std::make_shared<XJMesh>(vertices, asset.mIndices);
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
            return std::make_shared<XJMesh>(vertices, indices);
        }
        catch (const std::exception& e)
        {
            spdlog::error("XJMeshFactory::CreateCubeMesh failed: {}", e.what());
            return nullptr;
        }
    }
}