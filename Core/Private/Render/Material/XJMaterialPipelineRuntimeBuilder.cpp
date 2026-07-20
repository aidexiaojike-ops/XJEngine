#include "Render/Material/XJMaterialPipelineRuntimeBuilder.h"

#include "Graphic/XJVulkanDescriptorSet.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Render/Material/XJMaterialShaderRuntimeLayoutBuilder.h"
#include "Render/Shader/XJShaderAsset.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"

namespace XJ
{
    std::string XJMaterialPipelineRuntimeBuilder::ToPipelineShaderSourcePath(//获取位置.spv
        const std::filesystem::path& shaderPath)
    {
        std::filesystem::path path = shaderPath;
        if(path.extension() == ".spv")
            path.replace_extension();


        return path.generic_string();
    }

    bool XJMaterialPipelineRuntimeBuilder::Build(
        const XJShaderAsset& shaderAsset,
        const XJMaterialPipelineRuntimeBuildContext& context,
        XJMaterialPipelineRuntime& outRuntime)
    {
        if (!context.Device || !context.RenderPass)
        {
            spdlog::error("Material pipeline runtime build failed: invalid build context.");
            return false;
        }

        XJMaterialPipelineRuntime runtime{};

        if (!XJMaterialShaderRuntimeLayoutBuilder::BuildFromShaderAsset(shaderAsset, runtime.ShaderLayout))
        {
            spdlog::error("Material pipeline runtime build failed: shader runtime layout build failed.");
            return false;
        }

        if (!runtime.ShaderLayout.HasPrimaryFrameUbo())
        {
            spdlog::error("Material pipeline runtime build failed: no primary frame UBO.");
            return false;
        }

        if (!runtime.ShaderLayout.HasFrameSet())
        {
            spdlog::error("Material pipeline runtime build failed: no frame descriptor set.");
            return false;
        }

        if (!runtime.ShaderLayout.HasPrimaryMaterialUbo())
        {
            spdlog::error("Material pipeline runtime build failed: no primary material UBO.");
            return false;
        }

        if (!runtime.ShaderLayout.HasMaterialParameterSet())
        {
            spdlog::error("Material pipeline runtime build failed: no material parameter descriptor set.");
            return false;
        }

        if (!runtime.ShaderLayout.HasMaterialResourceSet())
        {
            spdlog::error("Material pipeline runtime build failed: no material resource descriptor set.");
            return false;
        }

        XJVulkanDevice* device = context.Device;
        //frame ubo
        runtime.FrameUboDescSetLayout =
            std::make_shared<XJVulkanDescriptorSetLayout>(
                device,
                runtime.ShaderLayout.FrameBindings);
        //材质参数
        runtime.MaterialParamDescSetLayout =
            std::make_shared<XJVulkanDescriptorSetLayout>(
                device,
                runtime.ShaderLayout.MaterialParameterBindings);

        runtime.MaterialResourceDescSetLayout =
            std::make_shared<XJVulkanDescriptorSetLayout>(
                device,
                runtime.ShaderLayout.MaterialResourceBindings);
        //描述符集
        std::vector<VkDescriptorPoolSize> framePoolSizes =
        {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1
            },
        };

        runtime.FrameDescriptorPool =
            std::make_shared<XJ::XJVulkanDescriptorPool>(
                device,
                1,
                framePoolSizes);

        runtime.FrameUboDescSet =
            runtime.FrameDescriptorPool->AllocateDescriptorSet(
                runtime.FrameUboDescSetLayout.get(),
                1)[0];

        runtime.FrameUboBuffer =
            std::make_shared<XJ::XJVulkanBuffer>(
                device,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                sizeof(FrameUbo),
                nullptr,
                true);
        //常量
        VkPushConstantRange modelPC =
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(ModelPC)
        };
        //shader
        ShaderLayout shaderLayout =
        {
            .descriptorSetLayouts = {
                runtime.FrameUboDescSetLayout->XJGetDescriptorSet(),
                runtime.MaterialParamDescSetLayout->XJGetDescriptorSet(),
                runtime.MaterialResourceDescSetLayout->XJGetDescriptorSet()
            },
            .pushConstantRanges = { modelPC }
        };
        //资源
        runtime.PipelineLayout =
            std::make_shared<XJVulkanPipelineLayout>(
                device,
                ToPipelineShaderSourcePath(runtime.ShaderLayout.VertexPath),
                ToPipelineShaderSourcePath(runtime.ShaderLayout.FragmentPath),
                shaderLayout);

        std::vector<VkVertexInputBindingDescription> vertexBindings{};
        vertexBindings.resize(1);
        vertexBindings[0].binding = 0;//绑定顶点
        vertexBindings[0].stride = sizeof(XJ::XJVulkanVertex);//步幅为顶点的结构体大小
        vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//每一个顶点数据
            //顶点输入
        std::vector<VkVertexInputAttributeDescription> vertexAttributes =
        {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(XJVulkanVertex, position)
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(XJVulkanVertex, texcoord0)
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(XJVulkanVertex, normal)
            }
        };

        runtime.Pipeline = std::make_shared<XJVulkanPipeline>(
                device,
                context.RenderPass,
                runtime.PipelineLayout.get());
        // // 仅当 render pass 有深度附件时才启用深度测试
        {
            bool hasDepth = false;
            const auto& attachments = context.RenderPass->XJGetAttachments();

            for (const auto& attachment : attachments)
            {
                if (IsDepthStencilFormat(attachment.format))
                {
                    hasDepth = true;
                    break;
                }
            }

            if (hasDepth)
                runtime.Pipeline->EnableDepthTest(VK_TRUE);
        }

        runtime.Pipeline->SetVertexInputState(vertexBindings, vertexAttributes);//设置顶点输入状态
        runtime.Pipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);//设置输入装配状态 三角形列表
        runtime.Pipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        runtime.Pipeline->SetMultisampleState(context.SampleCount, VK_FALSE, 0.2f);//多重采样
        runtime.Pipeline->Create();

        if (!runtime.IsValid())
        {
            spdlog::error(
                "Material pipeline runtime build failed: created runtime is invalid, shader='{}'.",
                runtime.ShaderLayout.ShaderPath.generic_string());
            return false;
        }
        //成功创建是正常路径
        spdlog::debug(
            "Material pipeline runtime created: shader='{}', frameUbo='{}', frameSet={}, frameBinding={}, materialUbo='{}', materialSet={}, materialBinding={}, materialSize={}, samplers={}",
            runtime.ShaderLayout.ShaderPath.generic_string(),
            runtime.ShaderLayout.PrimaryFrameUboName,
            runtime.ShaderLayout.PrimaryFrameUboSet,
            runtime.ShaderLayout.PrimaryFrameUboBinding,
            runtime.ShaderLayout.PrimaryMaterialUboName,
            runtime.ShaderLayout.PrimaryMaterialUboSet,
            runtime.ShaderLayout.PrimaryMaterialUboBinding,
            runtime.ShaderLayout.PrimaryMaterialUboSize,
            runtime.ShaderLayout.MaterialSamplerBindings.size());

        outRuntime = std::move(runtime);
        return true;
    
    }
}