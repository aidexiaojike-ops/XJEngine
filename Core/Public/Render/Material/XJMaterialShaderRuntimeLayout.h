#ifndef XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_H
#define XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_H

#include "Graphic/VulkanCommon.h"
#include "Render/Shader/XJShaderReflection.h"

#include <filesystem>
#include <vector>

namespace XJ
{
    struct XJMaterialShaderRuntimeLayout
    {
        std::filesystem::path ShaderPath;
        std::filesystem::path VertexPath;
        std::filesystem::path FragmentPath;

        XJShaderReflectionResult Reflection;

        uint32_t FrameSet = 0;
        uint32_t MaterialParameterSet = 1;
        uint32_t MaterialResourceSet = 2;

        std::vector<VkDescriptorSetLayoutBinding> MaterialParameterBindings;
        std::vector<VkDescriptorSetLayoutBinding> MaterialResourceBindings;
    };
}

#endif
