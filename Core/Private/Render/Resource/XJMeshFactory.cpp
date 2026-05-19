#include "Render/Resource/XJMeshFactory.h"

namespace XJ
{
    std::shared_ptr<XJMesh>XJMeshFactory::CreateFromAsset(const XJMeshAsset& asset)
    {
        std::vector<XJVulkanVertex> kVertices;//转换 Vertex（Asset 格式）→ XJVulkanVertex（GPU 格式）
        kVertices.reserve(asset.mVertices.size());//预留空间，避免重复分配
        for(const auto& vertex : asset.mVertices)
        {
            XJVulkanVertex kVulkanVertex{};
            kVulkanVertex.position = vertex.Position;
            kVulkanVertex.normal = vertex.Normal;
            kVulkanVertex.tangent = vertex.Tangent;
            kVulkanVertex.texcoord0 = vertex.UV;

            kVertices.push_back(kVulkanVertex);
         }
        return std::make_shared<XJMesh>(kVertices, asset.mIndices);//创建 XJMesh 实例，传入转换后的顶点数据和索引数据
    }
}