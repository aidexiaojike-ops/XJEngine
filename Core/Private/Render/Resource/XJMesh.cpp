#include "Render/Resource/XJMesh.h"

#include "Graphic/XJVulkanDevice.h"
#include "Render/XJRenderContext.h"
#include "XJApplication.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace XJ
{
    XJMesh::XJMesh(const std::vector<XJVulkanVertex>& vertices, const std::vector<uint32_t>& indices)
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

    void XJMesh::Draw(VkCommandBuffer commandBuffer)
    {
        if (commandBuffer == VK_NULL_HANDLE)
        {
            spdlog::warn("XJMesh::Draw skipped: command buffer is null.");
            return;
        }

        if (!IsValid())
        {
            spdlog::warn("XJMesh::Draw skipped: mesh is invalid.");
            return;
        }

        VkBuffer vertexBuffers[] = { mVertexBuffer->XJGetBuffer() };
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        if (mIndexCount > 0)
        {
            if (!mIndexBuffer || mIndexBuffer->XJGetBuffer() == VK_NULL_HANDLE)
            {
                spdlog::warn("XJMesh::Draw skipped indexed draw: index buffer is invalid.");
                return;
            }

            vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->XJGetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, mIndexCount, 1, 0, 0, 0);
            return;
        }

        vkCmdDraw(commandBuffer, mVertexCount, 1, 0, 0);
    }
}