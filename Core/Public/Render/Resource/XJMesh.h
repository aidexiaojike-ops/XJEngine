#ifndef XJ_MESH_H
#define XJ_MESH_H

#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanVertex.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace XJ
{

    // GPU 层的可绘制索引范围。
    // 所有 submesh 共享 XJMesh 的 VertexBuffer 和 IndexBuffer。
    struct XJSubmesh
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        uint32_t MaterialSlot = 0;

        bool IsValid(uint32_t totalIndexCount) const
        {
            if (IndexCount == 0)
                return false;

            if (FirstIndex > totalIndexCount)
                return false;

            return IndexCount <=
                totalIndexCount - FirstIndex;
        }
    };

    // XJMesh 是 GPU Resource。构造成功后至少有一个有效 vertex buffer。
    class XJMesh
    {
        public:
            XJMesh(const std::vector<XJVulkanVertex>& vertices, const std::vector<uint32_t>& indices = {}, std::vector<XJSubmesh> submeshes = {});
            ~XJMesh();

            XJMesh(const XJMesh&) = delete;
            XJMesh& operator=(const XJMesh&) = delete;

            bool IsValid() const
            {
                if (!mVertexBuffer || mVertexBuffer->XJGetBuffer() == VK_NULL_HANDLE || mVertexCount == 0)
                {
                    return false;
                }

                if (mIndexCount > 0)
                {
                    return mIndexBuffer &&
                           mIndexBuffer->XJGetBuffer() !=
                               VK_NULL_HANDLE &&
                           !mSubmeshes.empty();
                }

                return true;
            }

            // 绑定共享的 VertexBuffer 和 IndexBuffer。
            // 返回 false 表示资源无效，调用方不能继续 draw。
            bool Bind(VkCommandBuffer commandBuffer) const;           

            // 绘制整个 Mesh，保留给旧渲染路径。
            void Draw(VkCommandBuffer commandBuffer) const;           

            // 只绘制指定的 submesh。
            void DrawSubmesh(VkCommandBuffer commandBuffer, uint32_t submeshIndex) const;


             uint32_t GetVertexCount() const
            {
                return mVertexCount;
            }

            uint32_t GetIndexCount() const
            {
                return mIndexCount;
            }

            uint32_t GetSubmeshCount() const
            {
                return static_cast<uint32_t>(mSubmeshes.size());
            }

            const std::vector<XJSubmesh>& GetSubmeshes() const
            {
                return mSubmeshes;
            }

            const XJSubmesh* GetSubmesh(uint32_t index) const
            {
                if (index >= mSubmeshes.size())
                    return nullptr;

                return &mSubmeshes[index];
            }

            bool HasIndexedGeometry() const
            {
               return mIndexCount > 0 &&
                      mIndexBuffer &&
                      mIndexBuffer->XJGetBuffer() !=
                          VK_NULL_HANDLE;
            }

            uint32_t GetMaterialSlotCount() const
            {
                uint32_t count = 0;
            
                for (const XJSubmesh& submesh :
                     mSubmeshes)
                {
                    if (submesh.MaterialSlot >= count)
                        count = submesh.MaterialSlot + 1;
                }
            
                return count;
            }

        private:
            std::shared_ptr<XJVulkanBuffer> mVertexBuffer;
            std::shared_ptr<XJVulkanBuffer> mIndexBuffer;

            // 默认初始化，避免构造失败路径留下未定义值。
            uint32_t mVertexCount = 0;
            uint32_t mIndexCount = 0;

            std::vector<XJSubmesh> mSubmeshes;
    };
}


#endif