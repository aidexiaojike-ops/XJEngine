#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Edit/FileUtil.h"

namespace XJ
{


    XJVulkanPipelineLayout::XJVulkanPipelineLayout(XJVulkanDevice* device, const std::string &vertexShaderFilePath, 
               const std::string &fragmentShaderFilePath, const ShaderLayout &shaderLayout) : mDevice(device) 
    {
        spdlog::trace("{} : 开始创建管线布局", __FUNCTION__);
        spdlog::debug("顶点着色器路径: {}", vertexShaderFilePath + ".spv");
        spdlog::debug("片元着色器路径: {}", fragmentShaderFilePath + ".spv");
        
        //编译两个ShaderModule  
        spdlog::trace("创建顶点着色器模块...");
        //编译两个ShaderModule  
        XJDebug_Log(CreateShaderModule(vertexShaderFilePath + ".spv", &mVertexShaderModule));//添加位置
        spdlog::trace("顶点着色器模块创建成功: {}", (void*)mVertexShaderModule);
        
        spdlog::trace("创建片元着色器模块...");
        XJDebug_Log(CreateShaderModule(fragmentShaderFilePath + ".spv", &mFragmentShaderModule));
        spdlog::trace("片元着色器模块创建成功: {}", (void*)mFragmentShaderModule);
        //pipelineLayout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pNext = nullptr;
        pipelineLayoutInfo.flags = 0;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(shaderLayout.descriptorSetLayouts.size());//描述符集布局数量
        pipelineLayoutInfo.pSetLayouts = shaderLayout.descriptorSetLayouts.data();//描述符集布局数组
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(shaderLayout.pushConstantRanges.size());//推送常量范围数量
        pipelineLayoutInfo.pPushConstantRanges = shaderLayout.pushConstantRanges.data();//推送常量范围数组

        //spdlog::debug("描述符集布局数量: {}", pipelineLayoutInfo.setLayoutCount);
        //spdlog::debug("推送常量范围数量: {}", pipelineLayoutInfo.pushConstantRangeCount);
        XJDebug_Log(vkCreatePipelineLayout(mDevice->XJGetDevice(), &pipelineLayoutInfo, nullptr, &mPipelineLayoutLayout));
        spdlog::trace("{} : 管线布局创建成功 : {}", __FUNCTION__, (void*)mPipelineLayoutLayout);
    }
    XJVulkanPipelineLayout::~XJVulkanPipelineLayout()
    {
        if (mPipelineLayoutLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(mDevice->XJGetDevice(), mPipelineLayoutLayout, nullptr);
            spdlog::trace("{} : 销毁管线布局 : {}", __FUNCTION__, (void*)mPipelineLayoutLayout);
            mPipelineLayoutLayout = VK_NULL_HANDLE;
        }
        if (mVertexShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(mDevice->XJGetDevice(), mVertexShaderModule, nullptr);
            spdlog::trace("{} : 销毁顶点着色器模块 : {}", __FUNCTION__, (void*)mVertexShaderModule);
            mVertexShaderModule = VK_NULL_HANDLE;
        }
        if (mFragmentShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(mDevice->XJGetDevice(), mFragmentShaderModule, nullptr);
            spdlog::trace("{} : 销毁片元着色器模块 : {}", __FUNCTION__, (void*)mFragmentShaderModule);
            mFragmentShaderModule = VK_NULL_HANDLE;
        }

        spdlog::trace("{0} : 销毁 管线布局 实例 : {1}", __FUNCTION__, (void*)mPipelineLayoutLayout);
    }
    VkResult XJVulkanPipelineLayout::CreateShaderModule(const std::string &filePath, VkShaderModule* outShaderModule)
    {
        spdlog::trace("读取着色器文件: {}", filePath);
        std::vector<char> content =  ReadCharArrayFromFile(filePath);
        spdlog::debug("着色器文件大小: {} 字节", content.size());

        VkShaderModuleCreateInfo ShaderModuleCreateInfo{};
        ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ShaderModuleCreateInfo.pNext = nullptr;
        ShaderModuleCreateInfo.flags = 0;
        ShaderModuleCreateInfo.codeSize = content.size();
        ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(content.data());

       
        
        return vkCreateShaderModule(mDevice->XJGetDevice(), &ShaderModuleCreateInfo, nullptr, outShaderModule);
    }

    // Implementation of XJVulkanPipeline class
    XJVulkanPipeline::XJVulkanPipeline(XJVulkanDevice* device, XJVulkanRenderPass* renderPass, XJVulkanPipelineLayout* pipelineLayout): mDevice(device), mRenderPass(renderPass), mPipelineLayout(pipelineLayout)
    {
        spdlog::trace("{} : 创建管线对象", __FUNCTION__);
        spdlog::debug("Device: {}, RenderPass: {}, PipelineLayout: {}", 
                     (void*)device, (void*)renderPass, (void*)pipelineLayout);
    }
    
    XJVulkanPipeline::~XJVulkanPipeline()
    {
        if (mPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(mDevice->XJGetDevice(), mPipeline, nullptr);
            mPipeline = VK_NULL_HANDLE;
        }
        spdlog::trace("{0} : 销毁 管线 实例 : {1}", __FUNCTION__, (void*)mPipeline);
    }
    void XJVulkanPipeline::Create()
    {   
        spdlog::trace("{} : 开始创建图形管线", __FUNCTION__);
        
        // 验证必要的对象
        if (!mDevice)
        {
            spdlog::error("[VulkanPipeline] Create failed: device is null"); 
            return;
        }
        if (!mRenderPass)
        {
            spdlog::error("[VulkanPipeline] Create failed: render pass is null");
            return;
        }
        if (!mPipelineLayout)
        {
            spdlog::error("[VulkanPipeline] Create failed: pipeline layout is null");
            return;
        }

        spdlog::debug("子通道索引: {}", mSubpassIndex);
        
         // 1. Pipeline Layout

        uint32_t colorAttachmentCount =
        mRenderPass->XJGetColorAttachmentCount(mSubpassIndex);
        spdlog::debug("颜色附件数量: {}", colorAttachmentCount);

        if (mPipelineConfig.colorBlendState.attachments.empty())
        {
            spdlog::trace("初始化默认颜色混合附件状态");
            mPipelineConfig.colorBlendState.attachments.resize(colorAttachmentCount);
        
            for (uint32_t i = 0; i < colorAttachmentCount; ++i)
            {
                auto& att = mPipelineConfig.colorBlendState.attachments[i];
                att.blendEnable = VK_FALSE;
                att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                att.colorBlendOp = VK_BLEND_OP_ADD;
                att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                att.alphaBlendOp = VK_BLEND_OP_ADD;
                att.colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
            }
            //spdlog::trace("颜色混合附件初始化完成，数量: {}", colorAttachmentCount);
        }
        //着色器阶段
        spdlog::trace("配置着色器阶段...");
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.pNext = nullptr;
        vertShaderStageInfo.flags = 0;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = mPipelineLayout->XJGetVertexShaderModule();//顶点着色器模块
        vertShaderStageInfo.pName = "main";
        vertShaderStageInfo.pSpecializationInfo = nullptr;
        //spdlog::trace("顶点着色器阶段: module={}", (void*)vertShaderStageInfo.module);
        //片元着色器阶段
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.pNext = nullptr;
        fragShaderStageInfo.flags = 0;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = mPipelineLayout->XJGetFragmentShaderModule();//片元着色器模块
        fragShaderStageInfo.pName = "main";
        fragShaderStageInfo.pSpecializationInfo = nullptr;
        //spdlog::trace("片元着色器阶段: module={}", (void*)fragShaderStageInfo.module);

        VkPipelineShaderStageCreateInfo shaderStagesInfo[] =
        {
            vertShaderStageInfo,
            fragShaderStageInfo
        };
        //顶点输入状态
        spdlog::trace("配置顶点输入状态...");
        VkPipelineVertexInputStateCreateInfo vertexInputStateInfo{};
        vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateInfo.pNext = nullptr;
        vertexInputStateInfo.flags = 0;
        vertexInputStateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(mPipelineConfig.vertexInputState.vertexBindingDescriptions.size());// 顶点绑定描述数量
        vertexInputStateInfo.pVertexBindingDescriptions = mPipelineConfig.vertexInputState.vertexBindingDescriptions.data();// 顶点绑定描述数组
        vertexInputStateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(mPipelineConfig.vertexInputState.vertexAttributeDescriptions.size()); // 顶点属性描述数量 
        vertexInputStateInfo.pVertexAttributeDescriptions = mPipelineConfig.vertexInputState.vertexAttributeDescriptions.data();// 顶点属性描述数组
        //spdlog::debug("顶点绑定描述数量: {}, 顶点属性描述数量: {}", 
        //             vertexInputStateInfo.vertexBindingDescriptionCount, 
        //             vertexInputStateInfo.vertexAttributeDescriptionCount);
        //输入装配状态
        spdlog::trace("配置输入装配状态...");
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo{};
        inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateInfo.pNext = nullptr;
        inputAssemblyStateInfo.flags = 0;
        inputAssemblyStateInfo.topology = mPipelineConfig.inputAssemblyState.topology;//图元拓扑结构  几何体类型
        inputAssemblyStateInfo.primitiveRestartEnable = mPipelineConfig.inputAssemblyState.primitiveRestartEnable;//是否启用图元重启
        //spdlog::debug("图元拓扑: {}, 图元重启: {}", 
        //            static_cast<VkPrimitiveTopology>(inputAssemblyStateInfo.topology),
        //            static_cast<uint32_t>(inputAssemblyStateInfo.primitiveRestartEnable));
        //默认视口
        VkViewport defaultViewport{};
        defaultViewport.x = 0.0f;
        defaultViewport.y = 0.0f;
        defaultViewport.width = 800.0f;
        defaultViewport.height = 600.0f;
        defaultViewport.minDepth = 0.0f;
        defaultViewport.maxDepth = 1.0f;
        //默认裁剪矩形
        VkRect2D defaultScissor{};
        defaultScissor.offset = {0, 0};
        defaultScissor.extent = {800, 600};
        spdlog::trace("视口设置: {}x{}", defaultViewport.width, defaultViewport.height);
        //视口状态
        VkPipelineViewportStateCreateInfo viewportStateInfo{};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.pNext = nullptr;  
        viewportStateInfo.flags = 0;
        viewportStateInfo.viewportCount = 1; //视口数量
        viewportStateInfo.pViewports = &defaultViewport;
        viewportStateInfo.scissorCount = 1; //裁剪矩形数量
        viewportStateInfo.pScissors = &defaultScissor;
        //光栅化状态
        spdlog::trace("配置光栅化状态...");
        VkPipelineRasterizationStateCreateInfo rasterizationStateInfo{};
        rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateInfo.pNext = nullptr;
        rasterizationStateInfo.flags = 0;
        rasterizationStateInfo.depthClampEnable = mPipelineConfig.rasterizationState.depthClampEnable;//是否启用深度裁剪
        rasterizationStateInfo.rasterizerDiscardEnable = mPipelineConfig.rasterizationState.rasterizerDiscardEnable;//是否丢弃光栅化阶段
        rasterizationStateInfo.polygonMode = mPipelineConfig.rasterizationState.polygonMode;//多边形填充模式
        rasterizationStateInfo.cullMode = mPipelineConfig.rasterizationState.cullMode;//剔除模式
        rasterizationStateInfo.frontFace = mPipelineConfig.rasterizationState.frontFace;//正面朝向
        rasterizationStateInfo.depthBiasEnable = mPipelineConfig.rasterizationState.depthBiasEnable;//是否启用深度偏移
        rasterizationStateInfo.depthBiasConstantFactor = mPipelineConfig.rasterizationState.depthBiasConstantFactor;//深度偏移常量因子
        rasterizationStateInfo.depthBiasClamp = mPipelineConfig.rasterizationState.depthBiasClamp;//深度偏移夹紧值
        rasterizationStateInfo.depthBiasSlopeFactor = mPipelineConfig.rasterizationState.depthBiasSlopeFactor;//深度偏移斜率因子
        rasterizationStateInfo.lineWidth = mPipelineConfig.rasterizationState.lineWidth;//线宽
        //spdlog::debug("多边形模式: {}, 剔除模式: {}, 线宽: {}", 
        //     rasterizationStateInfo.polygonMode, 
        //     rasterizationStateInfo.cullMode, 
        //     rasterizationStateInfo.lineWidth);
        spdlog::debug("线宽: {}",rasterizationStateInfo.lineWidth);

        //多重采样状态
        spdlog::trace("配置多重采样状态...");
        VkPipelineMultisampleStateCreateInfo multisampleStateInfo{};
        multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateInfo.pNext = nullptr;
        multisampleStateInfo.flags = 0;
        multisampleStateInfo.rasterizationSamples = mPipelineConfig.multisampleState.rasterizationSamples;//光栅化采样数
        multisampleStateInfo.sampleShadingEnable = mPipelineConfig.multisampleState.sampleShadingEnable;//是否启用采样着色
        multisampleStateInfo.minSampleShading = mPipelineConfig.multisampleState.minSampleShading;//最小采样着色值
        multisampleStateInfo.pSampleMask = nullptr;//采样掩码
        multisampleStateInfo.alphaToCoverageEnable = VK_FALSE;//是否启用Alpha到覆盖
        multisampleStateInfo.alphaToOneEnable = VK_FALSE;//是否启用Alpha到1
        //spdlog::debug("光栅化采样数: {}, 采样着色: {}",
        //            static_cast<uint32_t>(multisampleStateInfo.rasterizationSamples),
        //            multisampleStateInfo.sampleShadingEnable);

        //深度模板状态
        spdlog::trace("配置深度模板状态...");
        VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo{};
        depthStencilStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateInfo.pNext = nullptr;
        depthStencilStateInfo.flags = 0;
        depthStencilStateInfo.depthTestEnable = mPipelineConfig.depthStencilState.depthTestEnable;//是否启用深度测试
        depthStencilStateInfo.depthWriteEnable = mPipelineConfig.depthStencilState.depthWriteEnable;//是否启用深度写入
        depthStencilStateInfo.depthCompareOp = mPipelineConfig.depthStencilState.depthCompareOp;//深度比较操作
        depthStencilStateInfo.depthBoundsTestEnable = mPipelineConfig.depthStencilState.depthBoundsTestEnable;//是否启用深度边界测试
        depthStencilStateInfo.stencilTestEnable = mPipelineConfig.depthStencilState.stencilTestEnable;//是否启用模板测试
        depthStencilStateInfo.front = mPipelineConfig.depthStencilState.front;//正面模板操作
        depthStencilStateInfo.back = mPipelineConfig.depthStencilState.back;//背面模板操作
        depthStencilStateInfo.minDepthBounds = mPipelineConfig.depthStencilState.minDepthBounds;//最小深度边界值
        depthStencilStateInfo.maxDepthBounds = mPipelineConfig.depthStencilState.maxDepthBounds;//最大深度边界值
        //spdlog::debug("深度测试: {}, 深度写入: {}, 深度比较操作: {}", 
        //             depthStencilStateInfo.depthTestEnable, 
        //             depthStencilStateInfo.depthWriteEnable, 
        //             depthStencilStateInfo.depthCompareOp);
        //spdlog::debug("深度测试: {}, 深度写入: {}", 
        //             depthStencilStateInfo.depthTestEnable, 
        //             depthStencilStateInfo.depthWriteEnable);
        //颜色混合状态
        spdlog::trace("配置颜色混合状态...");
        VkPipelineColorBlendStateCreateInfo colorBlendStateInfo{};
        colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateInfo.pNext = nullptr;
        colorBlendStateInfo.flags = 0;
        colorBlendStateInfo.logicOpEnable = mPipelineConfig.colorBlendState.logicOpEnable;//是否启用逻辑操作
        colorBlendStateInfo.logicOp = mPipelineConfig.colorBlendState.logicOp;//逻辑操作
        colorBlendStateInfo.attachmentCount = static_cast<uint32_t>(mPipelineConfig.colorBlendState.attachments.size());//颜色混合附件数量
        colorBlendStateInfo.pAttachments = mPipelineConfig.colorBlendState.attachments.data();//颜色混合附件数组
        colorBlendStateInfo.blendConstants[0] = 0.0f;//混合常量R
        colorBlendStateInfo.blendConstants[1] = 0.0f;//混合常量G
        colorBlendStateInfo.blendConstants[2] = 0.0f;//混 合常量B
        colorBlendStateInfo.blendConstants[3] = 0.0f;//混合常量A
        //spdlog::debug("逻辑操作: {}, 颜色混合附件数量: {}", 
        //             static_cast<int>(colorBlendStateInfo.logicOp), 
        //             colorBlendStateInfo.attachmentCount);
        //动态状态
        spdlog::trace("配置动态状态...");
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.pNext = nullptr;
        dynamicStateInfo.flags = 0;
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(mPipelineConfig.dynamicState.dynamicStates.size());//动态状态数量
        dynamicStateInfo.pDynamicStates = mPipelineConfig.dynamicState.dynamicStates.data();//动态状态数组   
        //spdlog::debug("动态状态数量: {}", 
        //             dynamicStateInfo.dynamicStateCount);
        //管线创建信息
        spdlog::trace("组装管线创建信息...");        
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = nullptr;
        pipelineInfo.flags = 0;
        pipelineInfo.stageCount = ARRAY_SIZE(shaderStagesInfo);//着色器阶段数量
        pipelineInfo.pStages = shaderStagesInfo;
        pipelineInfo.pVertexInputState = &vertexInputStateInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
        pipelineInfo.pTessellationState = nullptr;
        pipelineInfo.pViewportState = &viewportStateInfo;
        pipelineInfo.pRasterizationState = &rasterizationStateInfo;
        pipelineInfo.pMultisampleState = &multisampleStateInfo;
        pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
        pipelineInfo.pColorBlendState = &colorBlendStateInfo;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = mPipelineLayout->XJGetPipelineLayout();//管线布局
        pipelineInfo.renderPass = mRenderPass->XJGetRenderPass();//渲染
        pipelineInfo.subpass = mSubpassIndex;//子通道索引
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = 0;
        //spdlog::debug("管线布局: {}, 渲染通道: {}, 子通道索引: {}", 
        //             (void*)pipelineInfo.layout, 
        //             (void*)pipelineInfo.renderPass, 
        //             pipelineInfo.subpass);

        spdlog::trace("调用 vkCreateGraphicsPipelines...");
        XJDebug_Log(vkCreateGraphicsPipelines(mDevice->XJGetDevice(), mDevice->XJGetPipelineCache(), 1, &pipelineInfo, nullptr, &mPipeline));

        spdlog::trace("{} : 图形管线创建成功 : {}", __FUNCTION__, (void*)mPipeline);     
    }

    XJVulkanPipeline *XJVulkanPipeline::SetVertexInputState(const std::vector<VkVertexInputBindingDescription> &vertexBindings,
            const std::vector<VkVertexInputAttributeDescription> &vertexAttrs)//顶点输入状态
    {
        //spdlog::trace("设置顶点输入状态: {} 个绑定, {} 个属性", vertexBindings.size(), vertexAttrs.size());
        mPipelineConfig.vertexInputState.vertexBindingDescriptions = vertexBindings;
        mPipelineConfig.vertexInputState.vertexAttributeDescriptions = vertexAttrs;
        return this;
    }
    XJVulkanPipeline *XJVulkanPipeline::SetInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)//输入装配状态
    {
        //spdlog::trace("设置输入装配状态: topology={}, primitiveRestart={}", topology, primitiveRestartEnable);
        mPipelineConfig.inputAssemblyState.topology = topology;
        mPipelineConfig.inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
        return this;
    }
    XJVulkanPipeline *XJVulkanPipeline::SetRasterizationState(const PipelineRasterizationState &rasterizationState)//光栅化状态
    {
        spdlog::trace("设置光栅化状态: polygonMode={}, cullMode={}, lineWidth={}", 
                     rasterizationState.polygonMode, rasterizationState.cullMode, rasterizationState.lineWidth);
        mPipelineConfig.rasterizationState.depthClampEnable = rasterizationState.depthClampEnable;
        mPipelineConfig.rasterizationState.rasterizerDiscardEnable = rasterizationState.rasterizerDiscardEnable;
        mPipelineConfig.rasterizationState.polygonMode = rasterizationState.polygonMode;
        mPipelineConfig.rasterizationState.cullMode = rasterizationState.cullMode;
        mPipelineConfig.rasterizationState.frontFace = rasterizationState.frontFace;    
        
        mPipelineConfig.rasterizationState.depthBiasEnable = rasterizationState.depthBiasEnable;
        mPipelineConfig.rasterizationState.depthBiasConstantFactor = rasterizationState.depthBiasConstantFactor;
        mPipelineConfig.rasterizationState.depthBiasClamp = rasterizationState.depthBiasClamp;
        mPipelineConfig.rasterizationState.depthBiasSlopeFactor = rasterizationState.depthBiasSlopeFactor;
        mPipelineConfig.rasterizationState.lineWidth = rasterizationState.lineWidth;
        
        //mPipelineConfig.rasterizationState = rasterizationState;
        return this;
    }
    XJVulkanPipeline *XJVulkanPipeline::SetMultisampleState(VkSampleCountFlagBits samples, VkBool32 sampleShadingEnable, float minSampleShading)
    {
        //spdlog::trace("设置多重采样状态: samples={}, sampleShading={}, minShading={}", 
        //             samples, sampleShadingEnable, minSampleShading);
        mPipelineConfig.multisampleState.rasterizationSamples = samples;
        mPipelineConfig.multisampleState.sampleShadingEnable = sampleShadingEnable;
        mPipelineConfig.multisampleState.minSampleShading = minSampleShading;
        return this;
    }
    XJVulkanPipeline *XJVulkanPipeline::SetDepthStencilState(const PipelineDepthStencilState &depthStencilState)
    {
        spdlog::trace("设置深度模板状态: depthTest={}, depthWrite={}, stencilTest={}", 
                     depthStencilState.depthTestEnable, depthStencilState.depthWriteEnable, depthStencilState.stencilTestEnable);
        mPipelineConfig.depthStencilState.depthTestEnable = depthStencilState.depthTestEnable;
        mPipelineConfig.depthStencilState.depthWriteEnable = depthStencilState.depthWriteEnable;
        mPipelineConfig.depthStencilState.depthCompareOp = depthStencilState.depthCompareOp;
        mPipelineConfig.depthStencilState.depthBoundsTestEnable = depthStencilState.depthBoundsTestEnable;
        mPipelineConfig.depthStencilState.stencilTestEnable = depthStencilState.stencilTestEnable;
        mPipelineConfig.depthStencilState.front = depthStencilState.front;;
        mPipelineConfig.depthStencilState.back = depthStencilState.back;
        mPipelineConfig.depthStencilState.minDepthBounds = depthStencilState.minDepthBounds;
        mPipelineConfig.depthStencilState.maxDepthBounds = depthStencilState.maxDepthBounds;
        // mPipelineConfig.depthStencilState.cullMode = VK_CULL_MODE_NONE; //深度测试时关闭剔除，确保所有面都参与深度测试
        //mPipelineConfig.depthStencilState = depthStencilState;
        return this;
    }
    XJVulkanPipeline *XJVulkanPipeline::SetColorBlendAttachmentState(VkBool32 blendEnable, VkBlendFactor srcColorBlendFactor, VkBlendFactor dstColorBlendFactor ,VkBlendOp colorBlendOp,
        VkBlendFactor srcAlphaBlendFactor , VkBlendFactor dstAlphaBlendFactor ,VkBlendOp alphaBlendOp)
    {
        uint32_t colorAttachmentCount = mRenderPass->XJGetColorAttachmentCount(/*subpassIndex=*/mSubpassIndex);
        //spdlog::trace("设置颜色混合附件状态: attachmentCount={}, blendEnable={}", colorAttachmentCount, blendEnable);
        if (mPipelineConfig.colorBlendState.attachments.size() < colorAttachmentCount)
        {
              mPipelineConfig.colorBlendState.attachments.resize(colorAttachmentCount);
        }
        for (uint32_t i = 0; i < colorAttachmentCount ; ++i)
        {
        mPipelineConfig.colorBlendState.attachments[i].blendEnable = blendEnable;
        mPipelineConfig.colorBlendState.attachments[i].srcColorBlendFactor = srcColorBlendFactor;
        mPipelineConfig.colorBlendState.attachments[i].dstColorBlendFactor = dstColorBlendFactor;
        mPipelineConfig.colorBlendState.attachments[i].colorBlendOp = colorBlendOp;
        mPipelineConfig.colorBlendState.attachments[i].srcAlphaBlendFactor = srcAlphaBlendFactor;
        mPipelineConfig.colorBlendState.attachments[i].dstAlphaBlendFactor = dstAlphaBlendFactor;
        mPipelineConfig.colorBlendState.attachments[i].alphaBlendOp = alphaBlendOp;
        mPipelineConfig.colorBlendState.attachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                                      VK_COLOR_COMPONENT_G_BIT |
                                                                                      VK_COLOR_COMPONENT_B_BIT |
                                                                                      VK_COLOR_COMPONENT_A_BIT;
        }
        mPipelineConfig.colorBlendState.logicOpEnable = VK_FALSE;
        mPipelineConfig.colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    
        return this;
    }
    XJVulkanPipeline *XJVulkanPipeline::SetDynamicState(const std::vector<VkDynamicState> &dynamicStates)
    {
        //spdlog::trace("设置动态状态: {} 个状态", dynamicStates.size());
        mPipelineConfig.dynamicState.dynamicStates = dynamicStates;
        return this;

    }
    XJVulkanPipeline *XJVulkanPipeline::EnableAlphaBlend()//启用Alpha混合
    {
        uint32_t colorAttachmentCount  = mRenderPass->XJGetColorAttachmentCount(mSubpassIndex);
        //spdlog::trace("启用Alpha混合: {} 个附件", colorAttachmentCount);
        // 🔒 确保 attachments 数量足够
        if (mPipelineConfig.colorBlendState.attachments.size() < colorAttachmentCount)
        {
            mPipelineConfig.colorBlendState.attachments.resize(colorAttachmentCount);
        }

        for (uint32_t i = 0; i < colorAttachmentCount ; ++i)
        {
            mPipelineConfig.colorBlendState.attachments[i].blendEnable = VK_TRUE;
            mPipelineConfig.colorBlendState.attachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            mPipelineConfig.colorBlendState.attachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            mPipelineConfig.colorBlendState.attachments[i].colorBlendOp = VK_BLEND_OP_ADD;
            mPipelineConfig.colorBlendState.attachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            mPipelineConfig.colorBlendState.attachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            mPipelineConfig.colorBlendState.attachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
            //mPipelineConfig.colorBlendState.attachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            //                                                                      VK_COLOR_COMPONENT_G_BIT |
            //                                                                      VK_COLOR_COMPONENT_B_BIT |
            //                                                                      VK_COLOR_COMPONENT_A_BIT;
        }
      
        return this;
    }
    XJVulkanPipeline *XJVulkanPipeline::EnableDepthTest(VkBool32 enable)
    {
        spdlog::trace("启用深度测试: compareOp=LESS");
        //mPipelineConfig.depthStencilState.depthTestEnable = VK_TRUE;
        //mPipelineConfig.depthStencilState.depthWriteEnable = VK_TRUE;
        mPipelineConfig.depthStencilState.depthTestEnable = enable;
        mPipelineConfig.depthStencilState.depthWriteEnable = enable;
        mPipelineConfig.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
        return this;
    }
    void XJVulkanPipeline::BindPipeline(VkCommandBuffer commandBuffer) const
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    }

    
}