#ifndef XJ_VULKAN_ADDMODELASSETS_H
#define XJ_VULKAN_ADDMODELASSETS_H


namespace XJ
{
    class XJVulkanDevice;
    class VulkanCommandPool;
    class VulkanBuffer;
    class XJVulkanAddModelAssets
    {
        public:
            //XJVulkanAddModelAssets(const std::string& MODEL_PATH, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
            ~XJVulkanAddModelAssets();
        private:
            XJVulkanDevice* mDevice;
    };

}
#endif 