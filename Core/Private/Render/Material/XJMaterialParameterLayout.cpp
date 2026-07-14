#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Shader/XJShaderReflectionUtils.h"
#include "Render/Material/XJMaterialBuildResultUtils.h"
#include "Render/Shader/XJShaderSchemaBindingResolver.h"

namespace XJ
{

    XJMaterialParameterLayoutBuildResult XJMaterialParameterLayout::Build(const XJShaderSchema& schema, const XJShaderReflectionResult& reflection)
    {
        Clear();

        XJMaterialParameterLayoutBuildResult buildResult;

        if(!reflection.Valid)
        {
            AddMaterialBuildError(buildResult, "Cannot build material parameter layout: shader reflection is invalid.");

            for (const auto& error : reflection.Errors)
                AddMaterialBuildError(buildResult, error);

            return buildResult;
        }   
        //遍历 reflection 的所有 UBO members，全部存进去
        for(const auto& ubo : reflection.Ubos)
        {
            for(const auto& member : ubo.Members)
            {
                XJMaterialUboMemberBinding uboMemberBinding;
                uboMemberBinding.UboName = ubo.Name;
                uboMemberBinding.MemberName = member.Name;
                uboMemberBinding.Set = ubo.Set;
                uboMemberBinding.Binding = ubo.Binding;
                uboMemberBinding.Offset = member.Offset;
                uboMemberBinding.Size = member.Size;

                mUboMemberBindings.push_back(uboMemberBinding);
            }
        }

        //遍历 schema 的所有参数，检查是否在 reflection 中有对应的 UBO member 或 sampler

        const XJShaderSchemaBindingResolveResult resolveResult = ResolveShaderSchemaBindings(schema, reflection);

        for (const auto& error : resolveResult.Errors)
            AddMaterialBuildError(buildResult, error);

        for (const auto& warning : resolveResult.Warnings)
            AddMaterialBuildWarning(buildResult, warning);

        for(const auto& resolvedBinding  : resolveResult.Bindings)
        {
            if(resolvedBinding.Kind == XJShaderResolvedBindingKind::Texture)
            {
               
                XJMaterialTextureBinding textureBinding;
                textureBinding.Type = resolvedBinding.Type;
                textureBinding.ParameterName = resolvedBinding.ParameterName;
                textureBinding.SamplerName = resolvedBinding.SamplerName;
                textureBinding.Set = resolvedBinding.Set;
                textureBinding.Binding = resolvedBinding.Binding;

                mTextureBindings.push_back(textureBinding);
                continue;
            }

            if (resolvedBinding.Kind != XJShaderResolvedBindingKind::UboMember)
                continue;

            if (mUboName.empty())
            {
                mUboName = resolvedBinding.UboName;

                if (const XJShaderReflectedUbo* ubo = FindUbo(reflection, resolvedBinding.UboName))
                    mUboSize = ubo->Size;
            }
            else if (mUboName != resolvedBinding.UboName)
            {
                AddMaterialBuildWarning(buildResult, "Multiple material UBOs are not fully supported yet. Parameter uses UBO: " + resolvedBinding.UboName);
            }


            XJMaterialParameterBinding parameterBinding;
            parameterBinding.ParameterName = resolvedBinding.ParameterName;
            parameterBinding.Type = resolvedBinding.Type;
            parameterBinding.UboName = resolvedBinding.UboName;
            parameterBinding.MemberName = resolvedBinding.MemberName;
            parameterBinding.Set = resolvedBinding.Set;
            parameterBinding.Binding = resolvedBinding.Binding;
            parameterBinding.Offset = resolvedBinding.Offset;
            parameterBinding.Size = resolvedBinding.Size;

            mParameterBindings.push_back(parameterBinding);
        }

        mValid = buildResult.Errors.empty();
        buildResult.Valid = mValid;

        return buildResult;

        
    }

    void XJMaterialParameterLayout::Clear()
    {
        mValid = false;
        mUboSize = 0;
        mUboName.clear();
        mParameterBindings.clear();
        mTextureBindings.clear();
        mUboMemberBindings.clear();
    }

    const XJMaterialParameterBinding* XJMaterialParameterLayout::FindParameterBinding(const std::string& parameterName) const
    {
        for (const auto& binding : mParameterBindings)
        {
            if (binding.ParameterName == parameterName)
                return &binding;
        }
        return nullptr;
    }

    const XJMaterialTextureBinding* XJMaterialParameterLayout::FindTextureBinding(const std::string& parameterName) const
    {
        for (const auto& binding : mTextureBindings)
        {
            if (binding.ParameterName == parameterName)
                return &binding;
        }

        return nullptr;
    }

    const XJMaterialUboMemberBinding* XJMaterialParameterLayout::FindUboMemberBinding(const std::string& uboName, const std::string& memberName) const
    {
        for (const auto& binding : mUboMemberBindings)
        {
            if (binding.UboName == uboName && binding.MemberName == memberName)
                return &binding;
        }

        return nullptr;
    }

}
