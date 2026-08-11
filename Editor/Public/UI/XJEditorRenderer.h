#ifndef XJ_EDITOR_RENDERER_H
#define XJ_EDITOR_RENDERER_H

#include "Graphic/VulkanCommon.h"
#include <imgui.h>         // ImDrawData

namespace XJ
{
   

    struct XJEditorRendererInitInfo
    {
        VkInstance       instance;
        uint32_t         apiVersion = VK_API_VERSION_1_0;
        VkPhysicalDevice physicalDevice;
        VkDevice         device;

        VkRenderPass     renderPass;   // ✔ 核心
        VkCommandPool    commandPool;

        uint32_t         queueFamily;
        VkQueue          queue;

        uint32_t         imageCount;

        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT; // ✔ 可保留
        uint32_t         subpass = 0;                              // ✔ 可保留

        VkFormat         colorFormat;
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
            void Shutdown();
    };
    
   
}

#endif
