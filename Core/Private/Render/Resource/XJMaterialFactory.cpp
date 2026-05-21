#include "Render/Resource/XJMaterialFactory.h"

namespace XJ
{
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
        glm::vec3 kColor(asset.BaseColorFactor);
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
}