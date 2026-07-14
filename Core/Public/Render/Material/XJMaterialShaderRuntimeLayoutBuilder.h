#ifndef XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_BUILDER_H
#define XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_BUILDER_H

#include "Render/Material/XJMaterialShaderRuntimeLayout.h"
#include "Render/Shader/XJShaderAsset.h"
#include "Render/Shader/XJShaderDescriptorLayoutBuilder.h"

namespace XJ
{
    class XJMaterialShaderRuntimeLayoutBuilder
    {
        public:
            static bool BuildFromShaderAsset(
                const XJShaderAsset& shaderAsset,
                XJMaterialShaderRuntimeLayout& outLayout)
            {
                if (!shaderAsset.Reflection.Valid)
                    return false;

                outLayout.ShaderPath = shaderAsset.mPath;
                outLayout.VertexPath = shaderAsset.VertexPath;
                outLayout.FragmentPath = shaderAsset.FragmentPath;
                outLayout.Reflection = shaderAsset.Reflection;

                outLayout.MaterialParameterBindings = BuildDescriptorSetLayoutBindings(
                    shaderAsset.Reflection,
                    outLayout.MaterialParameterSet);

                outLayout.MaterialResourceBindings = BuildDescriptorSetLayoutBindings(
                    shaderAsset.Reflection,
                    outLayout.MaterialResourceSet);

                return !outLayout.MaterialParameterBindings.empty() &&
                       !outLayout.MaterialResourceBindings.empty();
            }
    };
}

#endif
