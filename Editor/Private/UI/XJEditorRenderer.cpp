#include "UI/XJEditorRenderer.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"


namespace XJ
{
    bool XJEditorRenderer::Init(const XJEditorRendererInitInfo& info)
    {
         mDevice = info.device; 

        //创建描述符池
        VkDescriptorPoolSize kPoolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo kPoolCI = {};
        kPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        kPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        kPoolCI.maxSets = 1000;
        kPoolCI.poolSizeCount = std::size(kPoolSizes);
        kPoolCI.pPoolSizes = kPoolSizes;
        vkCreateDescriptorPool(info.device, &kPoolCI, nullptr, &mDescriptorPool);

        //初始化 ImGui Vulkan
        ImGui_ImplVulkan_InitInfo kVi = {};
        kVi.Instance = info.instance;
        kVi.PhysicalDevice = info.physicalDevice;
        kVi.Device = info.device;
        kVi.QueueFamily = info.queueFamily;
        kVi.Queue = info.queue;
        kVi.PipelineCache = VK_NULL_HANDLE;
        kVi.DescriptorPool = mDescriptorPool;
        kVi.RenderPass = info.renderPass;
        kVi.MinImageCount = info.imageCount;
        kVi.ImageCount = info.imageCount;
        kVi.MSAASamples = info.msaaSamples;
        kVi.Subpass = info.subpass;

        ImGui_ImplVulkan_Init(&kVi);

        //上传字体纹理   把所有用的的文字上传到GPU
        //VkCommandBuffer kCmd;
        //VkCommandBufferAllocateInfo kCmdAllocInfo = {};
        //kCmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        //kCmdAllocInfo.commandPool = info.commandPool;
        //kCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        //kCmdAllocInfo.commandBufferCount = 1;
        //vkAllocateCommandBuffers(info.device, &kCmdAllocInfo, &kCmd);//分配命令缓冲区
//
        //VkCommandBufferBeginInfo kCmdBeginInfo = {};
        //kCmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //kCmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        //vkBeginCommandBuffer(kCmd, &kCmdBeginInfo);//开始命令缓冲区

        ImGui_ImplVulkan_CreateFontsTexture();//上传字体纹理   无参版本
        //ImGui_ImplVulkan_CreateFontsTexture(kCmd);//上传字体纹理   无参版本
        //vkEndCommandBuffer(kCmd);//结束命令缓冲区

        //VkSubmitInfo kSubmitInfo = {};
        //kSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        //kSubmitInfo.commandBufferCount = 1;
        //kSubmitInfo.pCommandBuffers = &kCmd;
        //vkQueueSubmit(info.queue,  1, &kSubmitInfo, VK_NULL_HANDLE);//提交命令缓冲区
        //vkQueueWaitIdle(info.queue);//等待队列空闲

        //vkFreeCommandBuffers(info.device, info.commandPool, 1, &kCmd);//释放命令缓冲区
        // ImGui_ImplVulkan_DestroyFontsTexture();//销毁上传用的，绘制时才重建
        //
        ////ImGui_ImplVulkan_DestroyFontUploadObjects();//新版本才有

        return true;

    }
    void XJEditorRenderer::RenderDrawData(VkCommandBuffer cmd, ImDrawData* drawData)
    {
        if(drawData) ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
    }
    void XJEditorRenderer::Shutdown()
    {
        ImGui_ImplVulkan_Shutdown();
        if(mDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
            mDescriptorPool = VK_NULL_HANDLE;
        }
        
    }
}