#include "Render/XJMesh.h"
#include "Render/XJRenderContext.h"
#include "XJApplication.h"

namespace XJ
{
    XJMesh::XJMesh(const std::vector<XJ::XJVulkanVertex>& vertices, const std::vector<uint32_t>& indices)
    {

        if(vertices.empty())
        {
            spdlog::error("XJMesh::XJMesh 参数错误，vertices 不能为空");
            return;
        }
        XJ::XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* device = renderContext->XJGetDevice();

        mVertexCount = vertices.size();
        mIndexCount = indices.size();

        //Mesh的创建
        mVertexBuffer = std::make_shared<XJVulkanBuffer>(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(vertices[0]) * vertices.size(), vertices.data());//创建顶点缓冲区
        if(mIndexCount > 0)
        {
            mIndexBuffer = std::make_shared<XJVulkanBuffer>(device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                sizeof(indices[0]) * indices.size(), indices.data());//创建索引缓冲区
        }
       
    }
    XJMesh::~XJMesh()
    {
         
    }   
    void XJMesh::Draw(VkCommandBuffer commandBuffer)
    {
        VkBuffer vertexBuffers[] = { mVertexBuffer->XJGetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        //绑定顶点缓冲区
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);//绑定顶点缓冲区
        if(mIndexCount > 0)
        {
             //绑定索引缓冲区
            vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->XJGetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            //绘制
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mIndexCount), 1, 0, 0, 0);//绘制索引图元
           
        }else
        {
             //绘制
            vkCmdDraw(commandBuffer, mVertexCount, 1, 0, 0); // 绘制一个三角形
            return;

        }


    }
}