#include "Render/Resource/XJMaterialFactory.h"

#include "Render/Material/XJMaterialParameterBlockBuilder.h"
#include "Render/Material/XJUnlitMaterialBindingUtils.h"

#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/Importer/XJTextureImporter.h"
#include "Render/Resource/XJTextureFactory.h"

#include <spdlog/spdlog.h>
namespace XJ
{
    namespace
    {
        glm::vec4 ReadVec4Parameter(const XJMaterialAsset& asset, const std::string& name, const glm::vec4& fallback)
        {
            const XJMaterialParameterValue* value = asset.FindParameter(name);
            if (!value)
                return fallback;

            if (std::holds_alternative<glm::vec4>(*value))
                return std::get<glm::vec4>(*value);

            if (std::holds_alternative<glm::vec3>(*value))
            {
                const glm::vec3& v = std::get<glm::vec3>(*value);
                return glm::vec4(v, fallback.w);
            }

            return fallback;
        }

        float ReadFloatParameter(const XJMaterialAsset& asset, const std::string& name, float fallback)
        {
            const XJMaterialParameterValue* value = asset.FindParameter(name);
            if (!value)
                return fallback;

            if (std::holds_alternative<float>(*value))
                return std::get<float>(*value);

            if (std::holds_alternative<int>(*value))
                return static_cast<float>(std::get<int>(*value));

            return fallback;
        }

        XJAssetHandle ReadTextureParameter(const XJMaterialAsset& asset, const std::string& name, XJAssetHandle fallback)
        {
            const XJMaterialParameterValue* value = asset.FindParameter(name);
            if (!value)
                return fallback;

            if (std::holds_alternative<XJAssetHandle>(*value))
                return std::get<XJAssetHandle>(*value);

            return fallback;
        }

        bool BuildRuntimeMaterialData(const XJMaterialAsset& asset, XJMaterial& material)
        {
            if (asset.ShaderPath.empty())
            {
                spdlog::warn("Material runtime build failed: shader path is empty.");
                return false;
            }
        
            auto shaderAsset = XJShaderAssetSerializer::LoadFromFile(asset.ShaderPath);
            if (!shaderAsset)
            {
                spdlog::warn("Material runtime build failed: shader asset load failed: {}", asset.ShaderPath.string());
                return false;
            }
        
            if (!shaderAsset->Reflection.Valid)
            {
                spdlog::warn("Material runtime build failed: shader reflection invalid: {}", asset.ShaderPath.string());
                for (const auto& error : shaderAsset->Reflection.Errors)
                    spdlog::warn("  reflection: {}", error);
                return false;
            }
        
            XJMaterialParameterLayout layout;
            XJMaterialParameterLayoutBuildResult layoutResult = layout.Build(shaderAsset->Schema, shaderAsset->Reflection);
            if (!layoutResult.Valid)
            {
                spdlog::warn("Material runtime build failed: layout invalid: {}", asset.ShaderPath.string());
                for (const auto& error : layoutResult.Errors)
                    spdlog::warn("  layout error: {}", error);
                for (const auto& warning : layoutResult.Warnings)
                    spdlog::warn("  layout warning: {}", warning);
                return false;
            }
        
            XJMaterialParameterBlock block;
            XJMaterialParameterBlockBuildResult blockResult =
                XJMaterialParameterBlockBuilder::Build(asset, layout, block);
        
            if (!blockResult.Valid)
            {
                spdlog::warn("Material runtime build failed: block invalid: {}", asset.ShaderPath.string());
                for (const auto& error : blockResult.Errors)
                    spdlog::warn("  block error: {}", error);
                for (const auto& warning : blockResult.Warnings)
                    spdlog::warn("  block warning: {}", warning);
                return false;
            }
        
            material.SetParameterLayout(layout);
            material.SetParameterBlock(block);
            material.SetTextureBindings(layout.GetTextureBindings());
            material.SetShaderPath(asset.ShaderPath);
        
            spdlog::debug(
                "Material runtime build ok: ubo='{}', size={}, params={}, textures={}",
                layout.GetUboName(),
                layout.GetUboSize(),
                layout.GetParameterBindings().size(),
                layout.GetTextureBindings().size());
            
            return true;
        }

        void SetSamplerTextureFallback(XJUnlitMaterial& material,
            const XJMaterialTextureBinding& binding,
            const std::shared_ptr<XJTexture>& defaultTexture,
            const std::shared_ptr<XJSampler>& defaultSampler)
        {
            if (binding.SamplerName.empty())
                return;
        
            material.SetSamplerTextureView(binding.SamplerName, defaultTexture, defaultSampler);
            material.UpdateSamplerTextureViewEnable(binding.SamplerName, false);
        }

        void ApplyFallbackUnlitTextureViews(XJUnlitMaterial& material, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
        {
            material.SetTextureView(UNLIT_MAT_BASE_COLOR, defaultTexture, defaultSampler);
            material.UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR, false);
        }

        void ApplyFallbackSamplerTextureViews(XJUnlitMaterial& material, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
        {
            if (!defaultTexture || !defaultSampler)
                return;

            for (const auto& binding : material.GetTextureBindings())
            {
                SetSamplerTextureFallback(material, binding, defaultTexture, defaultSampler);
            }
        }
        
        bool IsRealTextureLoaded(const std::shared_ptr<XJTexture>& texture,
            const std::shared_ptr<XJTexture>& fallback)
        {
            return texture != nullptr && texture != fallback;
        }

        void SetUnlitSlotTextureFallback(
            XJUnlitMaterial& material,
            const XJMaterialTextureBinding& binding,
            const std::shared_ptr<XJTexture>& defaultTexture,
            const std::shared_ptr<XJSampler>& defaultSampler)
        {
            const uint32_t slot = ResolveUnlitTextureSlot(binding);
        
            material.SetTextureView(slot, defaultTexture, defaultSampler);
            material.UpdateTextureViewEnable(slot, false);
        }

        void SetTextureFallbackForBinding(XJUnlitMaterial& material,
            const XJMaterialTextureBinding& binding,
            const std::shared_ptr<XJTexture>& defaultTexture,
            const std::shared_ptr<XJSampler>& defaultSampler)
        {
            SetSamplerTextureFallback(material, binding, defaultTexture, defaultSampler);
            SetUnlitSlotTextureFallback(material, binding, defaultTexture, defaultSampler);
        }

        void SetTextureForBinding(
            XJUnlitMaterial& material,
            const XJMaterialTextureBinding& binding,
            const std::shared_ptr<XJTexture>& texture,
            const std::shared_ptr<XJSampler>& sampler,
            bool enable)
        {
            if (!binding.SamplerName.empty())
            {
                material.SetSamplerTextureView(binding.SamplerName, texture, sampler);
                material.UpdateSamplerTextureViewEnable(binding.SamplerName, enable);
            }
        
            const uint32_t slot = ResolveUnlitTextureSlot(binding);
            material.SetTextureView(slot, texture, sampler);
            material.UpdateTextureViewEnable(slot, enable);
        }
    }

    XJMaterialFactory XJMaterialFactory::mMaterialFactory{};

    std::shared_ptr<XJTexture> XJMaterialFactory::GetOrLoadTexture(XJAssetHandle handle, const std::shared_ptr<XJTexture>& fallback)
    {
        if (handle == 0)
            return fallback;

        auto cacheIt = mTextureCache.find(handle);
        if (cacheIt != mTextureCache.end())
        {
            if (auto texture = cacheIt->second.lock())
                return texture;

            mTextureCache.erase(cacheIt);
        }

        if (!mAssetRegistry)
        {
            spdlog::warn("Texture load fallback: material factory has no asset registry, handle={}", handle);
            return fallback;
        }

        auto meta = mAssetRegistry->GetMeta(handle);
        if (!meta || meta->Type != XJAssetType::Texture)
        {
            spdlog::warn("Texture load fallback: invalid texture handle={}", handle);
            return fallback;
        }

        auto textureAsset = XJTextureImporter::ImportTexture(meta->SourcePath.string());
        if (!textureAsset)
        {
            spdlog::warn("Texture load fallback: failed to import texture: {}", meta->SourcePath.string());
            return fallback;
        }

        auto texture = XJTextureFactory::CreateTextureFromAsset(*textureAsset);
        if (!texture)
        {
            spdlog::warn("Texture load fallback: failed to create runtime texture: {}", meta->SourcePath.string());
            return fallback;
        }

        mTextureCache[handle] = texture;
        return texture;
    }



    void XJMaterialFactory::ApplyTextureBindings(XJUnlitMaterial& material, const XJMaterialAsset& asset, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
    {
        if (!defaultTexture || !defaultSampler)
        {
            spdlog::warn("ApplyTextureBindings skipped fallback setup: default texture or sampler is null.");
            return;
        }

        // First make every reflected sampler descriptor valid. Some samplers are not schema-exposed
        // but still declared in shader and must receive a descriptor write.
        ApplyFallbackSamplerTextureViews(material, defaultTexture, defaultSampler);
        ApplyFallbackUnlitTextureViews(material, defaultTexture, defaultSampler);

        
        for (const auto& binding : material.GetTextureBindings())
        {
            // Non-exposed reflected samplers still keep fallback descriptor, but are not controlled by .xjmat.
            if (!binding.ExposedBySchema || binding.ParameterName.empty())
                continue;       

            const XJAssetHandle textureHandle =
                ReadTextureParameter(asset, binding.ParameterName, static_cast<XJAssetHandle>(0));      

            if (textureHandle == 0)
            {
                SetTextureFallbackForBinding(material, binding, defaultTexture, defaultSampler);

                spdlog::debug(
                    "Material texture cleared: material='{}', parameter='{}', sampler='{}', binding={}",
                    asset.mPath.string(),
                    binding.ParameterName,
                    binding.SamplerName,
                    binding.Binding);

                continue;
            }       

            std::shared_ptr<XJTexture> texture = GetOrLoadTexture(textureHandle, defaultTexture);
            const bool loadedRealTexture = texture != nullptr && texture != defaultTexture;     

            if (!loadedRealTexture)
            {
                SetTextureFallbackForBinding(material, binding, defaultTexture, defaultSampler);

                spdlog::warn(
                    "Material texture fallback: material='{}', parameter='{}', sampler='{}', handle={} failed to load real texture.",
                    asset.mPath.string(),
                    binding.ParameterName,
                    binding.SamplerName,
                    textureHandle);

                continue;
            }

            SetTextureForBinding(
                material,
                binding,
                texture,
                defaultSampler,
                true);

            spdlog::debug(
                "Material texture bound: material='{}', parameter='{}', sampler='{}', binding={}, handle={}",
                asset.mPath.string(),
                binding.ParameterName,
                binding.SamplerName,
                binding.Binding,
                textureHandle);
        }

    }


    std::shared_ptr<XJUnlitMaterial> XJMaterialFactory::CreateFromAsset(const XJMaterialAsset& asset,
                                    const std::shared_ptr<XJTexture>& defaultTex,
                                    const std::shared_ptr<XJSampler>& defaultSampler)
    {
        auto kMat = CreateMaterial<XJUnlitMaterial>();

        const bool runtimeReady = BuildRuntimeMaterialData(asset, *kMat);

        // PBR BaseColor -> Unlit base color
        glm::vec4 baseColor = ReadVec4Parameter(asset, "BaseColor", asset.BaseColorFactor);
        // XJAssetHandle albedoTexture = ReadTextureParameter(asset, "AlbedoTexture", asset.AlbedoTexture);
        spdlog::debug(
                    "CreateFromAsset BaseColor: material='{}', rgba=({}, {}, {}, {}), index={}",
                    asset.mPath.string(),
                    baseColor.r,
                    baseColor.g,
                    baseColor.b,
                    baseColor.a,
                    kMat->GetIndex());
        if (runtimeReady)
        {
            kMat->SetParameterValue("BaseColor", baseColor);
        }
        else
        {
            spdlog::warn("CreateFromAsset produced material without runtime parameter block: {}", asset.mPath.string());
        }
         // Albedo 贴图 → TextureViewA
        ApplyTextureBindings(*kMat, asset, defaultTex, defaultSampler);
    
        return kMat;
        
    }

    std::shared_ptr<XJUnlitMaterial> XJMaterialFactory::CreateDefaultMaterial(
                const std::shared_ptr<XJTexture>& defaultTexture,
                const std::shared_ptr<XJSampler>& defaultSampler)
    {
        auto mat = CreateMaterial<XJUnlitMaterial>();

        XJMaterialAsset asset;
        asset.Version = 2;
        asset.ShaderPath = "Resource/Shader/Unlit.xjshader";
        asset.Parameters["BaseColor"] = glm::vec4(0.8f, 0.6f, 0.2f, 1.0f);
        asset.Parameters["AlbedoTexture"] = static_cast<XJAssetHandle>(0);
        
        const bool runtimeReady = BuildRuntimeMaterialData(asset, *mat);
        if (runtimeReady)
        {
            mat->SetParameterValue("BaseColor", glm::vec4(0.8f, 0.6f, 0.2f, 1.0f));
        }
        else
        {
            spdlog::warn("CreateDefaultMaterial produced material without runtime parameter block.");
        }
    
        ApplyTextureBindings(*mat, asset, defaultTexture, defaultSampler);
    
        return mat;
    }
}
