#ifndef XJ_VULKAN_PIPELINE_H
#define XJ_VULKAN_PIPELINE_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanDevice;
    class XJVulkanRenderPass;

    // Define a structure to hold shader layout information 定义一个结构来保存着色器布局
    struct ShaderLayout
    {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
        std::vector<VkPushConstantRange> pushConstantRanges;
    };

    // VkPipelineRasterizationStateCreateInfo XJGetPipelineRasterizationStateCreateInfo;
    struct PipelineVertexInputStare//顶点输入状态
    {
        std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
    };
    struct PipelineInputAssemblyState//输入装配状态
    {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//图元拓扑结构
        VkBool32 primitiveRestartEnable = VK_FALSE;//是否启用图元重启
    };
    struct PipelineRasterizationState//光栅化状态
    {
        VkBool32 depthClampEnable = VK_FALSE;//是否启用深度裁剪
        VkBool32 rasterizerDiscardEnable = VK_FALSE;//是否启用光栅化丢弃
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;//多边形模式
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;//剔除模式
        VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;//前面朝向
        //VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;// 改为逆时针为正面
        VkBool32 depthBiasEnable = VK_FALSE;//是否启用深度偏移
        float depthBiasConstantFactor = 0.0f;//深度偏移常数因子
        float depthBiasClamp = 0.0f;//深度偏移夹紧值
        float depthBiasSlopeFactor = 0.0f;//深度偏移斜率因子
        float lineWidth = 1.0f;//线宽
    };
    struct PipelineMultisampleState//多重采样状态
    {
        VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;//光栅化采样数
        VkBool32 sampleShadingEnable = VK_FALSE;//是否启用采样着色
        float minSampleShading = 0.2f;//最小采样着色
        const VkSampleMask* pSampleMask = nullptr;//采样掩码
        VkBool32 alphaToCoverageEnable = VK_FALSE;//是否启用Alpha到覆盖
        VkBool32 alphaToOneEnable = VK_FALSE;//是否启用Alpha到单色
    };

    struct PipelineDepthStencilState//深度模板状态
    {
        VkBool32 depthTestEnable = VK_TRUE;//是否启用深度测试
        VkBool32 depthWriteEnable = VK_TRUE;//是否启用深度写入
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;//深度比较操作
        VkBool32 depthBoundsTestEnable = VK_FALSE;//是否启用深度边界测试
        VkBool32 stencilTestEnable = VK_FALSE;//是否启用模板测试
        VkStencilOpState front = {};//前面模板操作状态
        VkStencilOpState back = {};//后面模板操作状态
        float minDepthBounds = 0.0f;//最小深度边界
        float maxDepthBounds = 1.0f;//最大深度边界
         
    };
    
    struct PipelineColorBlendState//颜色混合状态
    {
        VkBool32 logicOpEnable = VK_FALSE;//是否启用逻辑操作
        VkLogicOp logicOp = VK_LOGIC_OP_COPY;//逻辑操作
        std::vector<VkPipelineColorBlendAttachmentState> attachments;//颜色混合附件状态
        float blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};//混合常数
    };
    struct PipelineDynamicState//动态状态
    {
        std::vector<VkDynamicState> dynamicStates;//动态状态列表
    };

    struct PipelineConfig
    {
        PipelineVertexInputStare vertexInputState;//顶点输入状态
        PipelineInputAssemblyState inputAssemblyState;//输入装配状态
        PipelineRasterizationState rasterizationState;//光栅化状态
        PipelineMultisampleState multisampleState;//多重采样状态
        PipelineDepthStencilState depthStencilState;//深度模板状态
        PipelineColorBlendState colorBlendState;//颜色混合状态
        PipelineDynamicState dynamicState;//动态状态
                           
    };


    class XJVulkanPipelineLayout
    {
        public:
            XJVulkanPipelineLayout(XJVulkanDevice* device, const std::string &vertexShaderFilePath, 
                const std::string &fragmentShaderFilePath, const ShaderLayout &shaderLayout = {});
            ~XJVulkanPipelineLayout();

            VkPipelineLayout XJGetPipelineLayout() const { return mPipelineLayoutLayout; }
            //两个ShaderModule
            VkShaderModule XJGetVertexShaderModule() const { return mVertexShaderModule; }
            VkShaderModule XJGetFragmentShaderModule() const { return mFragmentShaderModule; }
        private:
            VkResult CreateShaderModule(const std::string &filePath, VkShaderModule* outShaderModule);
            VkPipelineLayout mPipelineLayoutLayout = VK_NULL_HANDLE;
            
            XJVulkanDevice* mDevice;
            //顶点作色 和 片源作色  
            VkShaderModule mVertexShaderModule = VK_NULL_HANDLE;
            VkShaderModule mFragmentShaderModule = VK_NULL_HANDLE;
        
    };


    class XJVulkanPipeline
    {
        private:
            VkPipeline mPipeline = VK_NULL_HANDLE;
            XJVulkanDevice* mDevice;
            XJVulkanRenderPass* mRenderPass;
            XJVulkanPipelineLayout* mPipelineLayout;

            PipelineConfig mPipelineConfig;

            uint32_t mSubpassIndex = 0; // 默认 0
            /* data */
        public:
            XJVulkanPipeline(XJVulkanDevice* device, XJVulkanRenderPass* renderPass, XJVulkanPipelineLayout* pipelineLayout);
            ~XJVulkanPipeline();
            
            void Create();

            void BindPipeline(VkCommandBuffer commandBuffer) const;    

            XJVulkanPipeline *SetVertexInputState(const std::vector<VkVertexInputBindingDescription> &vertexBindings,
                const std::vector<VkVertexInputAttributeDescription> &vertexAttrs);
            XJVulkanPipeline *SetInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);
            XJVulkanPipeline *SetRasterizationState(const PipelineRasterizationState &rasterizationState);
            XJVulkanPipeline *SetMultisampleState(VkSampleCountFlagBits samples, VkBool32 sampleShadingEnable, float minSampleShading = 0.f);
            XJVulkanPipeline *SetDepthStencilState(const PipelineDepthStencilState &depthStencilState);
            XJVulkanPipeline *SetColorBlendAttachmentState(VkBool32 blendEnable, VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ONE, VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,VkBlendOp colorBlendOp = VK_BLEND_OP_ADD,
                VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD);
            XJVulkanPipeline *SetDynamicState(const std::vector<VkDynamicState> &dynamicStates);
            XJVulkanPipeline *EnableAlphaBlend();
            XJVulkanPipeline *EnableDepthTest(VkBool32 enable);//启用深度测试


            VkPipeline XJGetPipeline() const { return mPipeline; }

bool isValid() const {
    return !mPipelineConfig.colorBlendState.attachments.empty();
}


    };
    
}

#endif