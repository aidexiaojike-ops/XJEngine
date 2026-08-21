#include "Render/Resource/XJMesh.h"

#include "Graphic/XJVulkanDevice.h"
#include "Render/XJRenderContext.h"
#include "XJApplication.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

namespace XJ
{
    XJMesh::XJMesh(const std::vector<XJVulkanVertex>& vertices, const std::vector<uint32_t>& indices, std::vector<XJSubmesh> submeshes)
        : mSubmeshes(std::move(submeshes))
    {
        if (vertices.empty())
        {
            spdlog::error("XJMesh create failed: vertices is empty.");
            throw std::invalid_argument("XJMesh requires at least one vertex");
        }

        XJAppContext* appContext = XJApplication::XJGetAppContext();
        XJRenderContext* renderContext = appContext ? appContext->renderContext : nullptr;
        XJVulkanDevice* device = renderContext ? renderContext->XJGetDevice() : nullptr;

        if (!device || !device->IsValid())
        {
            spdlog::error("XJMesh create failed: device is invalid.");
            throw std::runtime_error("XJMesh requires a valid Vulkan device");
        }

        mVertexCount = static_cast<uint32_t>(vertices.size());
        mIndexCount = static_cast<uint32_t>(indices.size());

        if(mIndexCount == 0)
        {
            // 当前 submesh 结构只描述索引范围。
            // importer 下一阶段会为无索引 primitive 自动生成索引。
            if (!mSubmeshes.empty())
            {
                spdlog::error( "XJMesh create failed: indexed submeshes " "were supplied without an index buffer.");
                throw std::invalid_argument("XJMesh submeshes require indices");
            }
        }
        else
        {
            // 兼容旧调用方：未传 submesh 时生成覆盖整个 Mesh 的默认范围。
            if(mSubmeshes.empty())
            {
                XJSubmesh defaultSubmesh;
                defaultSubmesh.FirstIndex = 0;
                defaultSubmesh.IndexCount = mIndexCount;
                defaultSubmesh.MaterialSlot = 0;

                mSubmeshes.push_back(defaultSubmesh);
            }
        }

        mBounds.Reset();

        for(XJSubmesh& submesh : mSubmeshes)
        {
            // 构造阶段先只检查索引范围。Builtin/旧资产可能没有预计算 Bounds，
            // 下方会根据最终 GPU 顶点和索引重新计算并写回。
            if (submesh.IndexCount == 0 ||
                submesh.FirstIndex > mIndexCount ||
                submesh.IndexCount > mIndexCount - submesh.FirstIndex)
            {
                spdlog::error(
                    "XJMesh create failed: invalid "
                    "submesh index range.");
                
                throw std::invalid_argument( "XJMesh contains invalid submesh range");
            }

            XJBoundingBox calculatedBounds;
            const uint32_t endIndex = submesh.FirstIndex + submesh.IndexCount;

            for(uint32_t indexPosition = submesh.FirstIndex; indexPosition < endIndex; ++indexPosition)
            {
                const uint32_t vertexIndex = indices[indexPosition];

                if(vertexIndex >= vertices.size())
                {
                    spdlog::error(
                        "XJMesh create failed: "
                        "index {} references vertex {}, "
                        "but vertex count is {}.",
                        indexPosition,
                        vertexIndex,
                        vertices.size());

                        throw std::invalid_argument( "XJMesh index is outside vertex range");

                }

                calculatedBounds.Expand(vertices[vertexIndex].position);
            }
            if (!calculatedBounds.IsValid())
            {
                spdlog::error(
                    "XJMesh create failed: "
                    "submesh bounds are invalid.");
                
                throw std::invalid_argument("XJMesh submesh bounds are invalid");
            }

            // GPU 顶点数据是最终权威来源。
            // 即使 Asset 已提供 Bounds，也重新计算以防导入数据损坏。
            submesh.Bounds = calculatedBounds;

            mBounds.Merge(submesh.Bounds);
        }

        if (mIndexCount == 0)
        {
            for (const XJVulkanVertex& vertex :
                 vertices)
            {
                mBounds.Expand(vertex.position);
            }
        }


        mVertexBuffer = std::make_shared<XJVulkanBuffer>(
            device,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(vertices[0]) * vertices.size(),
            vertices.data());

        if (!mVertexBuffer || mVertexBuffer->XJGetBuffer() == VK_NULL_HANDLE)
        {
            spdlog::error("XJMesh create failed: vertex buffer is invalid.");
            throw std::runtime_error("XJMesh failed to create vertex buffer");
        }

        if (mIndexCount > 0)
        {
            mIndexBuffer = std::make_shared<XJVulkanBuffer>(
                device,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                sizeof(indices[0]) * indices.size(),
                indices.data());

            if (!mIndexBuffer || mIndexBuffer->XJGetBuffer() == VK_NULL_HANDLE)
            {
                spdlog::error("XJMesh create failed: index buffer is invalid.");
                throw std::runtime_error("XJMesh failed to create index buffer");
            }
        }
    }

    XJMesh::~XJMesh() = default;

    void XJMesh::Draw(VkCommandBuffer commandBuffer) const
    {
      
        if (!Bind(commandBuffer))
            return;
    
        if (mIndexCount > 0)
        {
            vkCmdDrawIndexed(commandBuffer, mIndexCount, 1, 0, 0, 0);
            return;
        }


        vkCmdDraw(commandBuffer, mVertexCount, 1, 0, 0);
    }

    bool XJMesh::Bind(VkCommandBuffer commandBuffer) const
    {
        if (commandBuffer == VK_NULL_HANDLE)
        {
            spdlog::warn("XJMesh::Bind skipped: command buffer is null.");
            return false;
        }

        if (!IsValid())
        {
            spdlog::warn("XJMesh::Draw skipped: mesh is invalid.");
            return false; 
        }

        VkBuffer vertexBuffers[] = { mVertexBuffer->XJGetBuffer() };
        VkDeviceSize offsets[] = { 0 };

         vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        if (mIndexCount > 0)
        {
            if (!mIndexBuffer || mIndexBuffer->XJGetBuffer() == VK_NULL_HANDLE)
            {
                spdlog::warn("XJMesh::Draw skipped indexed draw: index buffer is invalid.");
                return false; 
            }

            vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->XJGetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        
        }
        
        return true;
    }

    void XJMesh::DrawSubmesh(VkCommandBuffer commandBuffer, uint32_t submeshIndex) const
    {
        const XJSubmesh* submesh = GetSubmesh(submeshIndex);

        if(!submesh)
        {
            spdlog::warn("XJMesh::DrawSubmesh skipped: " "submesh index {} is out of range {}.", submeshIndex, mSubmeshes.size());

            return;
        }


        if(!submesh->IsValid(mIndexCount))
        {
            spdlog::warn( "XJMesh::DrawSubmesh skipped: " "submesh range is invalid.");

            return;
        }

        if (!HasIndexedGeometry())
        {
            spdlog::warn("XJMesh::DrawSubmesh skipped: ""indexed geometry is unavailable.");

            return;
        }

        if (!Bind(commandBuffer))
            return;

        vkCmdDrawIndexed(commandBuffer, submesh->IndexCount, 1, submesh->FirstIndex, 0, 0);
    }
}
