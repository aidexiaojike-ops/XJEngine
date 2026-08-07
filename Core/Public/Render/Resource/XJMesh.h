#ifndef XJ_MESH_H
#define XJ_MESH_H

#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanVertex.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace XJ
{
    // XJMesh 是 GPU Resource。构造成功后至少有一个有效 vertex buffer。
    class XJMesh
    {
        public:
            XJMesh(const std::vector<XJVulkanVertex>& vertices, const std::vector<uint32_t>& indices = {});
            ~XJMesh();

            bool IsValid() const
            {
                return mVertexBuffer != nullptr &&
                       mVertexBuffer->XJGetBuffer() != VK_NULL_HANDLE &&
                       mVertexCount > 0;
            }

            void Draw(VkCommandBuffer commandBuffer);

        private:
            std::shared_ptr<XJVulkanBuffer> mVertexBuffer;
            std::shared_ptr<XJVulkanBuffer> mIndexBuffer;

            // 默认初始化，避免构造失败路径留下未定义值。
            uint32_t mVertexCount = 0;
            uint32_t mIndexCount = 0;
    };
}


#endif