#ifndef XJ_MESH_H
#define XJ_MESH_H

#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanGeometryUtil.h"

namespace XJ
{
    class XJMesh
    {
        public:
            XJMesh(const std::vector<XJ::XJVulkanVertex>& vertices, const std::vector<uint32_t>& indices = {});
            ~XJMesh();

            void Draw(VkCommandBuffer commandBuffer);

        private:
            /* data */
            std::shared_ptr<XJVulkanBuffer> mVertexBuffer;//顶点缓冲区
            std::shared_ptr<XJVulkanBuffer> mIndexBuffer;//索引缓冲区

            uint32_t mVertexCount;//顶点数量
            uint32_t mIndexCount;//索引数量
            std::vector<XJVulkanVertex> mVertices;//顶点数据
            std::vector<uint32_t> mIndices;//索引数据
    };
}


#endif