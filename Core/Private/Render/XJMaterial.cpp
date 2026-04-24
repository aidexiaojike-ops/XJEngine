#include "Render/XJMaterial.h"


namespace XJ
{
    XJMaterialFactory XJMaterialFactory::s_MaterialFactory{};

    bool XJMaterial::HasTexture(uint32_t id) const 
    {
        if(mTextures.find(id) != mTextures.end())
        {
            return true;
        }
        return false;
    }

    const TextureView *XJMaterial::XJGetTextureView(uint32_t id) const
    {
        if(HasTexture(id))
        {
            return &mTextures.at(id);
        }
        return nullptr;
    }

    void XJMaterial::XJSetTextureView(uint32_t id, XJTexture *texture, XJSampler *sampler)//更新纹理和采样方式
    {
        if(HasTexture(id))
        {
            mTextures[id].texture = texture;
            mTextures[id].sampler = sampler;

        }
        else
        {
            mTextures[id] = { texture, sampler};
        } 
        bShouldFlushResoure = true;//纹理变更 更新材质
    }
    void XJMaterial::UpdateTextureViewEnable(uint32_t id, bool enable) 
    {
        if(HasTexture(id))
        {
            mTextures[id].bEnable = enable;
            bShouldFlushParams = true;//更新参数
        }
    }

    void XJMaterial::UpdateTextureViewUVTranslation(uint32_t id, const glm::vec2 &uvTranslation) 
    {
        if(HasTexture(id))
        {
            mTextures[id].uvTranslation = uvTranslation;
            bShouldFlushParams = true;
        }
    }

    void XJMaterial::UpdateTextureViewUVRotation(uint32_t id, float uvRotation) 
    {
        if(HasTexture(id))
        {
            mTextures[id].uvRotation = uvRotation;
            bShouldFlushParams = true;
        }
    }

    void XJMaterial::UpdateTextureViewUVScale(uint32_t id, const glm::vec2 &uvScale) 
    {
        if(HasTexture(id))
        {
            mTextures[id].uvScale = uvScale;
            bShouldFlushParams = true;
        }
    }

}