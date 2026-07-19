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
                // 查找主帧 UBO：遍历所有 UBO，取第一个属于 FrameSet 的
                for (const auto& ubo : shaderAsset.Reflection.Ubos)
                {
                    if (ubo.Set != outLayout.FrameSet)
                        continue;
                
                    outLayout.PrimaryFrameUboName = ubo.Name;
                    outLayout.PrimaryFrameUboSet = ubo.Set;
                    outLayout.PrimaryFrameUboBinding = ubo.Binding;
                    outLayout.PrimaryFrameUboSize = ubo.Size;
                    break;
                }
                //  查找主材质 UBO：遍历所有 UBO，取第一个属于 MaterialParameterSet 的
                for (const auto& ubo : shaderAsset.Reflection.Ubos)
                {
                    if(ubo.Set != outLayout.MaterialParameterSet)
                        continue;

                    outLayout.PrimaryMaterialUboName = ubo.Name;
                    outLayout.PrimaryMaterialUboSet = ubo.Set;
                    outLayout.PrimaryMaterialUboBinding = ubo.Binding;
                    outLayout.PrimaryMaterialUboSize = ubo.Size;
                    break;
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
