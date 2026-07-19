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
        
            spdlog::info(
                "Material runtime build ok: ubo='{}', size={}, params={}, textures={}",
                layout.GetUboName(),
                layout.GetUboSize(),
                layout.GetParameterBindings().size(),
                layout.GetTextureBindings().size());
            
            return true;
        }

        bool BuildDefaultUnlitRuntimeMaterialData(XJMaterial& material)
        {
           XJMaterialAsset asset;
           asset.Version = 2;
           asset.ShaderPath = "Resource/Shader/Unlit.xjshader";
           asset.Parameters["BaseColor"] = glm::vec4(0.8f, 0.6f, 0.2f, 1.0f);
           asset.Parameters["AlbedoTexture"] = static_cast<XJAssetHandle>(0);
        
           return BuildRuntimeMaterialData(asset, material);
        }


        void ApplyFallbackUnlitTextureViews(XJUnlitMaterial& material, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
        {
            material.SetTextureView(UNLIT_MAT_BASE_COLOR_A, defaultTexture, defaultSampler);
            material.UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, false);
        
            material.SetTextureView(UNLIT_MAT_BASE_COLOR_B, defaultTexture, defaultSampler);
            material.UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_B, false);
        }

        void ApplyFallbackSamplerTextureViews(XJUnlitMaterial& material, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
        {
            for (const auto& binding : material.GetTextureBindings())
            {
                if (binding.SamplerName.empty())
                    continue;

                material.SetSamplerTextureView(binding.SamplerName, defaultTexture, defaultSampler);
                material.UpdateSamplerTextureViewEnable(binding.SamplerName, false);
            }
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
        ApplyFallbackSamplerTextureViews(material, defaultTexture, defaultSampler);
        ApplyFallbackUnlitTextureViews(material, defaultTexture, defaultSampler);

        
        for (const auto& binding : material.GetTextureBindings())
        {
            if (!binding.ExposedBySchema || binding.ParameterName.empty())
                continue;       

            const XJAssetHandle textureHandle =
                ReadTextureParameter(asset, binding.ParameterName, static_cast<XJAssetHandle>(0));      

            const uint32_t slot = ResolveUnlitTextureSlot(binding);     

            if (textureHandle == 0)
            {
                material.SetSamplerTextureView(binding.SamplerName, defaultTexture, defaultSampler);
                material.UpdateSamplerTextureViewEnable(binding.SamplerName, false);        

                material.SetTextureView(slot, defaultTexture, defaultSampler);
                material.UpdateTextureViewEnable(slot, false);
                continue;
            }       

            std::shared_ptr<XJTexture> texture = GetOrLoadTexture(textureHandle, defaultTexture);
            const bool loadedRealTexture = texture != nullptr && texture != defaultTexture;     

            material.SetSamplerTextureView(binding.SamplerName, texture, defaultSampler);
            material.UpdateSamplerTextureViewEnable(binding.SamplerName, loadedRealTexture && defaultSampler != nullptr);       

            material.SetTextureView(slot, texture, defaultSampler);
            material.UpdateTextureViewEnable(slot, loadedRealTexture && defaultSampler != nullptr);
        }

    }


    std::shared_ptr<XJUnlitMaterial> XJMaterialFactory::CreateFromAsset(const XJMaterialAsset& asset,
                                    const std::shared_ptr<XJTexture>& defaultTex,
                                    const std::shared_ptr<XJSampler>& defaultSampler)
    {
        auto kMat = CreateMaterial<XJUnlitMaterial>();

        const bool runtimeReady = BuildRuntimeMaterialData(asset, *kMat);

        // PBR BaseColor → A/B（一期：A/B 同色）
        glm::vec4 baseColor = ReadVec4Parameter(asset, "BaseColor", asset.BaseColorFactor);
        // XJAssetHandle albedoTexture = ReadTextureParameter(asset, "AlbedoTexture", asset.AlbedoTexture);
        spdlog::info(
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

            // Unlit compatibility: current shader still blends A/B.
            kMat->SetBaseColorB(baseColor);
            kMat->SetMixValue(0.0f);
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

        const bool runtimeReady = BuildDefaultUnlitRuntimeMaterialData(*mat);
        if (runtimeReady)
        {
            mat->SetBaseColorA(glm::vec4(0.8f, 0.6f, 0.2f, 1.0f));
            mat->SetBaseColorB(glm::vec4(0.8f, 0.6f, 0.2f, 1.0f));
            mat->SetMixValue(0.0f);
        }
    
        if (defaultTexture && defaultSampler)
        {
            ApplyFallbackSamplerTextureViews(*mat, defaultTexture, defaultSampler);
            ApplyFallbackUnlitTextureViews(*mat, defaultTexture, defaultSampler);
        }
    
        return mat;
    }
}
