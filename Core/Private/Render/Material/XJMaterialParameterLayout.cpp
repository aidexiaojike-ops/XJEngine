#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Shader/XJShaderReflectionUtils.h"
#include "Render/Material/XJMaterialBuildResultUtils.h"
#include "Render/Shader/XJShaderSchemaBindingResolver.h"

namespace XJ
{
    namespace
    {
        const XJShaderReflectedUbo* FindUboByBinding(
            const XJShaderReflectionResult& reflection,
            uint32_t set,
            uint32_t binding)
        {
            for (const auto& ubo : reflection.Ubos)
            {
                if (ubo.Set == set && ubo.Binding == binding)
                    return &ubo;
            }

            return nullptr;
        }
    }

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
        for (const auto& sampler : reflection.Samplers)
        {
            XJMaterialTextureBinding textureBinding;
            textureBinding.SamplerName = sampler.Name;
            textureBinding.Set = sampler.Set;
            textureBinding.Binding = sampler.Binding;
            textureBinding.ExposedBySchema = false;
        
            mTextureBindings.push_back(textureBinding);
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
                bool updatedExistingBinding = false;
                for (auto& textureBinding : mTextureBindings)
                {
                    if (textureBinding.SamplerName == resolvedBinding.SamplerName)
                    {
                        textureBinding.Type = resolvedBinding.Type;
                        textureBinding.ParameterName = resolvedBinding.ParameterName;
                        textureBinding.Set = resolvedBinding.Set;
                        textureBinding.Binding = resolvedBinding.Binding;
                        textureBinding.ExposedBySchema = true;
                        updatedExistingBinding = true;
                        break;
                    }
                }

                if (!updatedExistingBinding)
                {
                    XJMaterialTextureBinding textureBinding;
                    textureBinding.Type = resolvedBinding.Type;
                    textureBinding.ParameterName = resolvedBinding.ParameterName;
                    textureBinding.SamplerName = resolvedBinding.SamplerName;
                    textureBinding.Set = resolvedBinding.Set;
                    textureBinding.Binding = resolvedBinding.Binding;
                    textureBinding.ExposedBySchema = true;
                
                    mTextureBindings.push_back(textureBinding);
                }

                continue;
            }

            if (resolvedBinding.Kind != XJShaderResolvedBindingKind::UboMember)
                continue;

            const XJShaderReflectedUbo* reflectedUbo = FindUboByBinding(
                reflection,
                resolvedBinding.Set,
                resolvedBinding.Binding);

            if (!reflectedUbo)
                reflectedUbo = FindUbo(reflection, resolvedBinding.UboName);

            if (!FindUboLayout(resolvedBinding.Set, resolvedBinding.Binding))
            {
                XJMaterialUboLayout uboLayout;
                uboLayout.UboName = resolvedBinding.UboName;
                uboLayout.Set = resolvedBinding.Set;
                uboLayout.Binding = resolvedBinding.Binding;
                uboLayout.Size = reflectedUbo ? reflectedUbo->Size : 0;
                mUboLayouts.push_back(uboLayout);
            }

            if (!mHasPrimaryUbo)
            {
                mHasPrimaryUbo = true;
                mUboName = resolvedBinding.UboName;
                mUboSet = resolvedBinding.Set;
                mUboBinding = resolvedBinding.Binding;
                mUboSize = reflectedUbo ? reflectedUbo->Size : 0;
            }
            else if (mUboSet != resolvedBinding.Set || mUboBinding != resolvedBinding.Binding)
            {
                // 当前 runtime 仍然只有一个材质参数 block/buffer/descriptor。
                // 这里先按 (set,binding) 记录 UBO 元信息，给未来每 UBO 一块 block 留扩展入口；
                // 但在 runtime 真正支持多 UBO 前，必须报错并跳过该参数，避免写入同一块内存后静默丢参。
                AddMaterialBuildError(
                    buildResult,
                    "Multiple material UBO bindings are not supported. Primary UBO is '" +
                        mUboName +
                        "' at set=" +
                        std::to_string(mUboSet) +
                        ", binding=" +
                        std::to_string(mUboBinding) +
                        ", but parameter '" +
                        resolvedBinding.ParameterName +
                        "' uses UBO '" +
                        resolvedBinding.UboName +
                        "' at set=" +
                        std::to_string(resolvedBinding.Set) +
                        ", binding=" +
                        std::to_string(resolvedBinding.Binding) +
                        ".");

                continue;
            }
            else if (mUboName != resolvedBinding.UboName)
            {
                AddMaterialBuildWarning(
                    buildResult,
                    "Material UBO name differs for the same binding. Primary UBO is '" +
                        mUboName +
                        "', parameter '" +
                        resolvedBinding.ParameterName +
                        "' resolved to '" +
                        resolvedBinding.UboName +
                        "'.");
            }

            if (mUboSize == 0)
            {
                AddMaterialBuildError(buildResult, "Material UBO size is zero: " + resolvedBinding.UboName);
                continue;
            }

            if (resolvedBinding.Offset + resolvedBinding.Size > mUboSize)
            {
                AddMaterialBuildError(
                    buildResult,
                    "Material parameter exceeds primary UBO size: " +
                        resolvedBinding.ParameterName +
                        ", offset=" +
                        std::to_string(resolvedBinding.Offset) +
                        ", size=" +
                        std::to_string(resolvedBinding.Size) +
                        ", uboSize=" +
                        std::to_string(mUboSize));

                continue;
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
        mHasPrimaryUbo = false;
        mUboSize = 0;
        mUboName.clear();
        mUboSet = 0;
        mUboBinding = 0;
        mParameterBindings.clear();
        mTextureBindings.clear();
        mUboMemberBindings.clear();
        mUboLayouts.clear();
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

    const XJMaterialTextureBinding* XJMaterialParameterLayout::FindTextureBindingBySampler(const std::string& samplerName) const
    {
        for(const auto& binding : mTextureBindings)
        {
            if(binding.SamplerName == samplerName)
                return &binding;
        }

        return nullptr;
    }
    
    const XJMaterialUboMemberBinding* XJMaterialParameterLayout::FindFirstUboBinding(const std::string& uboName) const
    {
        for (const auto& binding : mUboMemberBindings)
        {
            if (binding.UboName == uboName)
                return &binding;
        }

        return nullptr;
    }

    const XJMaterialUboLayout* XJMaterialParameterLayout::FindUboLayout(uint32_t set, uint32_t binding) const
    {
        for (const auto& layout : mUboLayouts)
        {
            if (layout.Set == set && layout.Binding == binding)
                return &layout;
        }

        return nullptr;
    }

}
