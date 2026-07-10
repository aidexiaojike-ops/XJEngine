#include "Render/Resource/XJMaterialFactory.h"

#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Render/Material/XJMaterialParameterBlockBuilder.h"

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
    }

    XJMaterialFactory XJMaterialFactory::mMaterialFactory{};

    std::shared_ptr<XJTexture> XJMaterialFactory::GetOrLoadTexture(XJAssetHandle handle, const std::shared_ptr<XJTexture>& fallback)
    {
        // 检查 cache
        auto kTextureCache = mTextureCache.find(handle);
        if(kTextureCache != mTextureCache.end())
        {
            if (auto kTex = kTextureCache->second.lock())
            return kTex;
            // weak_ptr 过期 → 移除
            mTextureCache.erase(kTextureCache);
        }
         // 暂时返回 fallback（实现 AssetManager 路径查询后改）
        if (handle != 0)
            mTextureCache[handle] = fallback; // 占位
        return fallback;
    }
    std::shared_ptr<XJUnlitMaterial> XJMaterialFactory::CreateFromAsset(const XJMaterialAsset& asset,
                                    const std::shared_ptr<XJTexture>& defaultTex,
                                    const std::shared_ptr<XJSampler>& defaultSampler)
    {
        auto kMat = CreateMaterial<XJUnlitMaterial>();

        const bool runtimeReady = BuildRuntimeMaterialData(asset, *kMat);

        // PBR BaseColor → A/B（一期：A/B 同色）
        glm::vec4 baseColor = ReadVec4Parameter(asset, "BaseColor", asset.BaseColorFactor);
        XJAssetHandle albedoTexture = ReadTextureParameter(asset, "AlbedoTexture", asset.AlbedoTexture);
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
            kMat->SetBaseColorA(baseColor);
            kMat->SetBaseColorB(baseColor);
            kMat->SetMixValue(0.0f);
        }
        else
        {
            spdlog::warn("CreateFromAsset produced material without runtime parameter block: {}", asset.mPath.string());
        }
         // Albedo 贴图 → TextureViewA
        if (albedoTexture != 0)
        {
            auto kTex = GetOrLoadTexture(albedoTexture, defaultTex);
            kMat->SetTextureView(UNLIT_MAT_BASE_COLOR_A, kTex, defaultSampler);
            kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, true);
        }
        else
        {
            kMat->SetTextureView(UNLIT_MAT_BASE_COLOR_A, defaultTex, defaultSampler);
            kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, false);
        }

        kMat->SetTextureView(UNLIT_MAT_BASE_COLOR_B, defaultTex, defaultSampler);
        kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_B, false);
    
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
            mat->SetTextureView(UNLIT_MAT_BASE_COLOR_A, defaultTexture, defaultSampler);
            mat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, false);
        
            mat->SetTextureView(UNLIT_MAT_BASE_COLOR_B, defaultTexture, defaultSampler);
            mat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_B, false);
        }
    
        return mat;
    }
}