#ifndef XJ_VULKAN_DESCRIPTOR_SET_H
#define XJ_VULKAN_DESCRIPTOR_SET_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanDevice;
    // 
     /**
     * @brief 构造函数：创建 Vulkan 描述符集布局对象。
     * @param device    XJVulkanDevice 指针，用于获取 VkDevice。
     * @param bindings  描述符集布局绑定列表，指定每个绑定的类型、阶段等。
     *
     * 该函数会调用 vkCreateDescriptorSetLayout 创建描述符集布局，
     * 结果存储在成员变量 mDescriptorSet 中。
     */
   
    class XJVulkanDescriptorSetLayout
    {
        public:
            XJVulkanDescriptorSetLayout(XJVulkanDevice *device, const std::vector<VkDescriptorSetLayoutBinding> &bindings);
            ~XJVulkanDescriptorSetLayout();
            VkDescriptorSetLayout XJGetDescriptorSet() {return mDescriptorSet;}
        private:
            VkDescriptorSetLayout mDescriptorSet = VK_NULL_HANDLE;

            XJVulkanDevice *mDevice;
    };


    struct DescriptorSetAllocateInfo
    {
        XJVulkanDescriptorSetLayout *setLayout;
        uint32_t count;
    };

    
    class XJVulkanDescriptorPool
    {
        public:
            XJVulkanDescriptorPool(XJVulkanDevice *device, uint32_t maxSets, const std::vector<VkDescriptorPoolSize> &poolSizes);
            ~XJVulkanDescriptorPool();

            
            VkDescriptorPool XJGetDescriptorPool() {return mDescriptorPool;}
            std::vector<VkDescriptorSet> AllocateDescriptorSet(XJVulkanDescriptorSetLayout *setLayout, uint32_t count);
        private:
            VkDescriptorPool mDescriptorPool;

            XJVulkanDevice *mDevice;
    };

 
   
}


#endif