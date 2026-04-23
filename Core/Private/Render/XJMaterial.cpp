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

    TextureView *XJMaterial::XJGetTextureView(uint32_t id)
    {
        if(HasTexture(id))
        {
            return &mTextures.at(id);
        }
        return nullptr;
    }

    void XJMaterial::XJSetTextureView(uint32_t id, XJTexture *texture, XJSampler *sampler)
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
        bShouldFlushResoure = true;
    }
}