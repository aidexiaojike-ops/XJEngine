#include "Graphic/XJVulkanDescriptorSet.h"
#include "Graphic/XJVulkanDevice.h"


namespace XJ
{
        XJVulkanDescriptorSetLayout::XJVulkanDescriptorSetLayout(XJVulkanDevice *device, const std::vector<VkDescriptorSetLayoutBinding> &bindings)
            : mDevice(device) // 初始化成员
        {
            VkDescriptorSetLayoutCreateInfo kDescriptorSetLayoutCreateInfo{};
            kDescriptorSetLayoutCreateInfo.sType =VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            kDescriptorSetLayoutCreateInfo.pNext = nullptr;
            kDescriptorSetLayoutCreateInfo.flags = 0;
            kDescriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());// 绑定数量
            kDescriptorSetLayoutCreateInfo.pBindings = bindings.data();// 绑定数组

             // 创建描述符集布局，结果存入 mDescriptorSet
            VkResult result = vkCreateDescriptorSetLayout(mDevice->XJGetDevice(), &kDescriptorSetLayoutCreateInfo, nullptr, &mDescriptorSet);
            if (result != VK_SUCCESS) {
                spdlog::error("vkCreateDescriptorSetLayout failed: {}", static_cast<int>(result));
                // 确保句柄为NULL
                mDescriptorSet = VK_NULL_HANDLE;
            } else {
                spdlog::info("vkCreateDescriptorSetLayout succeeded");
            }

        }
        XJVulkanDescriptorSetLayout::~XJVulkanDescriptorSetLayout()
        {
            if (mDescriptorSet != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(mDevice->XJGetDevice(), mDescriptorSet, nullptr);
            }
        }


        XJVulkanDescriptorPool::XJVulkanDescriptorPool(XJVulkanDevice *device,uint32_t maxSets, const std::vector<VkDescriptorPoolSize> &poolSizes)
            : mDevice(device) // 初始化成员
        {
            VkDescriptorPoolCreateInfo kDescriptorPoolCreateInfo{};
            kDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            kDescriptorPoolCreateInfo.pNext = nullptr;
            kDescriptorPoolCreateInfo.flags = 0; // 可选标志（如 FREE_DESCRIPTOR_SET_BIT）
            kDescriptorPoolCreateInfo.maxSets = maxSets; // 最大描述符集数量
            kDescriptorPoolCreateInfo.poolSizeCount = poolSizes.size(); // 池大小条目数
            kDescriptorPoolCreateInfo.pPoolSizes = poolSizes.data(); // 池大小数组
            // 创建描述符池，结果存入 mDescriptorPool
            XJDebug_Log(vkCreateDescriptorPool(mDevice->XJGetDevice(), &kDescriptorPoolCreateInfo, nullptr, &mDescriptorPool));
        }
        XJVulkanDescriptorPool::~XJVulkanDescriptorPool()
        {
            if(mDescriptorPool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(mDevice->XJGetDevice(), mDescriptorPool, nullptr);
            }
        }
        std::vector<VkDescriptorSet> XJVulkanDescriptorPool::AllocateDescriptorSet(XJVulkanDescriptorSetLayout *setLayout, uint32_t count)
        {
             // 准备一个向量存储分配得到的描述符集句柄
            std::vector<VkDescriptorSet> descriptorSets(count);
              // 准备与 count 相同数量的描述符集布局引用（所有引用相同布局）
            std::vector<VkDescriptorSetLayout> setLayouts(count);
            for(int i = 0; i < count; i++)
            {
                setLayouts[i] = setLayout->XJGetDescriptorSet(); // 从布局对象获取 VkDescriptorSetLayout
            }

            VkDescriptorSetAllocateInfo allocateInfo{};
            allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocateInfo.pNext = nullptr;
            allocateInfo.descriptorPool = mDescriptorPool; // 使用当前池
            allocateInfo.descriptorSetCount = count;// 要分配的数量
            allocateInfo.pSetLayouts = setLayouts.data();  // 布局数组
            // 执行分配
            VkResult ret = vkAllocateDescriptorSets(mDevice->XJGetDevice(),&allocateInfo, descriptorSets.data());
            XJDebug_Log(ret);
            if(ret != VK_SUCCESS) // 如果分配失败，清空返回向量（表示无有效句柄）
            {
                spdlog::error("Failed to create descriptor set layout: {}", ret);
                descriptorSets.clear();
            }


            return descriptorSets;
        }

}