#ifndef XJ_EDITOR_RENDERER_H
#define XJ_EDITOR_RENDERER_H

#include "Graphic/VulkanCommon.h"
#include <imgui.h>         // ImDrawData

namespace XJ
{
   

    struct XJEditorRendererInitInfo
    {
        VkInstance       instance = VK_NULL_HANDLE;
        uint32_t         apiVersion = VK_API_VERSION_1_0;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice         device = VK_NULL_HANDLE;

        VkRenderPass     renderPass = VK_NULL_HANDLE;   // ✔ 核心
        VkCommandPool    commandPool = VK_NULL_HANDLE;

        uint32_t         queueFamily = 0;
        VkQueue          queue = VK_NULL_HANDLE;

        uint32_t         imageCount = 0;

        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT; // ✔ 可保留
        uint32_t         subpass = 0;                              // ✔ 可保留

        VkFormat         colorFormat = VK_FORMAT_UNDEFINED;
    };

    class XJEditorRenderer
    {
        private:

            VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
            VkDevice         mDevice         = VK_NULL_HANDLE;
            bool             mInitialized    = false;
            /* data */
        public:
            ~XJEditorRenderer();

            bool Init(const XJEditorRendererInitInfo& info);
            void RenderDrawData(VkCommandBuffer cmd, ImDrawData* drawData);
            void UpdateSwapchainImageCount(uint32_t imageCount);
            void Shutdown();
    };
    
   
}

#endif
