#ifndef XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_BUILDER_H
#define XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_BUILDER_H

#include "Render/Material/XJMaterialShaderRuntimeLayout.h"
#include "Render/Shader/XJShaderAsset.h"
#include "Render/Shader/XJShaderDescriptorLayoutBuilder.h"

namespace XJ
{
    class XJMaterialShaderRuntimeLayoutBuilder
    {
        /**
     * @brief 材质着色器运行时布局构建器
     *
     * 从已编译/反射的着色器资产（XJShaderAsset）中提取绑定信息，
     * 填充到 XJMaterialShaderRuntimeLayout 中，
     * 用于后续创建 Vulkan 描述符集布局。
     */
        public:
            static bool BuildFromShaderAsset(const XJShaderAsset& shaderAsset, XJMaterialShaderRuntimeLayout& outLayout)
            {
                if (!shaderAsset.Reflection.Valid)
                    return false;
                // 复制基本路径和反射数据
                outLayout.ShaderPath = shaderAsset.mPath;
                outLayout.VertexPath = shaderAsset.VertexPath;
                outLayout.FragmentPath = shaderAsset.FragmentPath;
                outLayout.Reflection = shaderAsset.Reflection;
                // 为三个描述符集分别构建绑定列表
                outLayout.FrameBindings = BuildDescriptorSetLayoutBindings(
                    shaderAsset.Reflection,
                    outLayout.FrameSet);

                outLayout.MaterialParameterBindings = BuildDescriptorSetLayoutBindings(
                    shaderAsset.Reflection,
                    outLayout.MaterialParameterSet);

                outLayout.MaterialResourceBindings = BuildDescriptorSetLayoutBindings(
                    shaderAsset.Reflection,
                    outLayout.MaterialResourceSet);

                for (const auto& ubo : shaderAsset.Reflection.Ubos)
                {
                    if (ubo.Set != outLayout.MaterialParameterSet)
                        continue;

                    XJMaterialUboLayout uboLayout;
                    uboLayout.UboName = ubo.Name;
                    uboLayout.Set = ubo.Set;
                    uboLayout.Binding = ubo.Binding;
                    uboLayout.Size = ubo.Size;
                    outLayout.MaterialUboLayouts.push_back(uboLayout);
                }

                auto selectPrimaryUbo = [&shaderAsset](uint32_t set, const char* preferredName) -> const XJShaderReflectedUbo*
                {
                    const XJShaderReflectedUbo* firstInSet = nullptr;
                    uint32_t candidateCount = 0;

                    for (const auto& ubo : shaderAsset.Reflection.Ubos)
                    {
                        if (ubo.Set != set)
                            continue;

                        if (ubo.Name == preferredName)
                            return &ubo;

                        if (!firstInSet)
                            firstInSet = &ubo;

                        ++candidateCount;
                    }

                    // Single-UBO shaders remain supported. Multi-UBO shaders must use the
                    // convention name so the primary block is not selected by reflection order.
                    return candidateCount == 1 ? firstInSet : nullptr;
                };

                // 查找主帧 UBO：优先使用约定名，只有一个候选时才回退。
                const XJShaderReflectedUbo* frameUbo = selectPrimaryUbo(outLayout.FrameSet, "FrameUbo");
                if (frameUbo)
                {
                    outLayout.PrimaryFrameUboName = frameUbo->Name;
                    outLayout.PrimaryFrameUboSet = frameUbo->Set;
                    outLayout.PrimaryFrameUboBinding = frameUbo->Binding;
                    outLayout.PrimaryFrameUboSize = frameUbo->Size;
                }

                // 查找主材质 UBO：优先使用约定名，避免多 UBO 时按反射顺序任意选择。
                const XJShaderReflectedUbo* materialUbo = selectPrimaryUbo(outLayout.MaterialParameterSet, "MaterialUbo");
                if (materialUbo)
                {
                    outLayout.PrimaryMaterialUboName = materialUbo->Name;
                    outLayout.PrimaryMaterialUboSet = materialUbo->Set;
                    outLayout.PrimaryMaterialUboBinding = materialUbo->Binding;
                    outLayout.PrimaryMaterialUboSize = materialUbo->Size;
                }

                for(const auto& sampler : shaderAsset.Reflection.Samplers)
                {
                    if(sampler.Set != outLayout.MaterialResourceSet)
                        continue;

                    XJMaterialTextureBinding binding{};
                    binding.SamplerName = sampler.Name;
                    binding.Set = sampler.Set;
                    binding.Binding = sampler.Binding;
                    binding.ExposedBySchema = false;

                    outLayout.MaterialSamplerBindings.push_back(binding);
                }
                
                // 验证布局完整性
                return outLayout.HasPrimaryFrameUbo() &&
                    outLayout.HasFrameSet() &&
                    outLayout.HasPrimaryMaterialUbo() &&
                    outLayout.HasMaterialParameterSet() &&
                    outLayout.HasMaterialResourceSet();

            }
    };
}

#endif
