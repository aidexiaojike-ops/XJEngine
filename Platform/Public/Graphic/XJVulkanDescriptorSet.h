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
            // 构造函数：创建一个描述集池
            // device：关联的设备
            // maxSets：最大描述集数量
            // poolSizes：池中每种类型的描述符大小
            XJVulkanDescriptorPool(XJVulkanDevice *device, uint32_t maxSets, const std::vector<VkDescriptorPoolSize> &poolSizes);
            ~XJVulkanDescriptorPool();

            
            VkDescriptorPool XJGetDescriptorPool() {return mDescriptorPool;}
              // 分配描述集
            // setLayout：描述集布局
            // count：分配的描述集数量
            std::vector<VkDescriptorSet> AllocateDescriptorSet(XJVulkanDescriptorSetLayout *setLayout, uint32_t count);
        private:
            VkDescriptorPool mDescriptorPool;

            XJVulkanDevice *mDevice;
    };

    class DescriptorSetWriter
    {
        public:
            // 创建一个缓冲区描述符信息
            // buffer：缓冲区对象
            // offset：偏移量
            // range：范围
            static VkDescriptorBufferInfo BuildBufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
            {
                VkDescriptorBufferInfo bufferInfo
                {
                    .buffer = buffer,
                    .offset = offset,
                    .range = range
                };

                return bufferInfo;
            }
            // 创建一个纹理描述符信息
            // sampler：采样器
            // imageView：图像视图
            static VkDescriptorImageInfo BuildImageInfo(VkSampler sampler, VkImageView imagerView)
            {
                VkDescriptorImageInfo imageInfo
                {
                    .sampler = sampler,
                    .imageView = imagerView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
                return imageInfo;
            }
             // 写入缓冲区描述符
            static VkWriteDescriptorSet WriteBuffer(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descriptorType, const VkDescriptorBufferInfo *pBufferInfo) {
                VkWriteDescriptorSet writeDescriptorSet
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = dstSet,  // 目标描述集
                    .dstBinding = dstBinding,  // 绑定位置
                    .dstArrayElement = 0,
                    .descriptorCount = 1,  // 描述符数量
                    .descriptorType = descriptorType,  // 描述符类型
                    .pBufferInfo = pBufferInfo  // 缓冲区信息
                };
                return writeDescriptorSet;
            }
            // 写入图像描述符
            static VkWriteDescriptorSet WriteImage(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descriptorType, const VkDescriptorImageInfo *pImageInfo) {
                VkWriteDescriptorSet writeDescriptorSet
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = dstSet,  // 目标描述集
                    .dstBinding = dstBinding,  // 绑定位置
                    .dstArrayElement = 0,
                    .descriptorCount = 1,  // 描述符数量
                    .descriptorType = descriptorType,  // 描述符类型
                    .pImageInfo = pImageInfo  // 图像信息
                };
                return writeDescriptorSet;
            }

            // 更新描述集
            static void UpdateDescriptorSets(VkDevice device, const std::vector<VkWriteDescriptorSet> &writes) 
            {
                // 调用 Vulkan API 更新描述集
                vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
            }

    };

 
   
}


#endif