#include "Render/Resource/XJMaterialFactory.h"

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
        // PBR BaseColor → A/B（一期：A/B 同色）
        glm::vec4 baseColor = ReadVec4Parameter(asset, "BaseColor", asset.BaseColorFactor);
        float metallic = ReadFloatParameter(asset, "Metallic", asset.MetallicFactor);
        float roughness = ReadFloatParameter(asset, "Roughness", asset.RoughnessFactor);
        XJAssetHandle albedoTexture = ReadTextureParameter(asset, "AlbedoTexture", asset.AlbedoTexture);

        (void)metallic;
        (void)roughness;

        glm::vec3 kColor(baseColor);
        kMat->XJSetBaseColorA(kColor);
        kMat->XJSetBaseColorB(kColor);

         // Albedo 贴图 → TextureViewA
        if (asset.AlbedoTexture != 0)
        {
            auto kTex = GetOrLoadTexture(asset.AlbedoTexture, defaultTex);
            kMat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_A, kTex, defaultSampler);
            kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, true);
        }
        else
        {
            kMat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_A, defaultTex, defaultSampler);
            kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, false);
        }
        // B 贴图（一期：复用 A，后续 PBR 扩展用 MetallicRoughness）
        kMat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_B, defaultTex, defaultSampler);
        kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_B, false);
    
        return kMat;
        
    }

    std::shared_ptr<XJUnlitMaterial> XJMaterialFactory::CreateDefaultMaterial(
                const std::shared_ptr<XJTexture>& defaultTexture,
                const std::shared_ptr<XJSampler>& defaultSampler)
    {
        auto mat = CreateMaterial<XJUnlitMaterial>();

        mat->XJSetBaseColorA(glm::vec3(0.8f, 0.6f, 0.2f));
        mat->XJSetBaseColorB(glm::vec3(0.8f, 0.6f, 0.2f));

        if(defaultTexture && defaultSampler)
        {
            mat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_A, defaultTexture, defaultSampler);
            mat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, false);

            mat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_B, defaultTexture, defaultSampler);
            mat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_B, false);
        }
        return mat;
    }
}