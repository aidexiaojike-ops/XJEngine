#ifndef XJ_UNLIT_MATERIAL_BINDING_UTILS_H
#define XJ_UNLIT_MATERIAL_BINDING_UTILS_H

#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "Render/Material/XJMaterialParameterLayout.h"
// Compatibility mapping for current Unlit shader slots.
// Runtime descriptor binding should prefer sampler-name based TextureView.

namespace XJ
{
    inline uint32_t ResolveUnlitTextureSlot(const XJMaterialTextureBinding& binding)
    {
        if(binding.ParameterName == "AlbedoTexture" || binding.SamplerName == "textureA")
            return UNLIT_MAT_BASE_COLOR_A;

        if (binding.SamplerName == "textureB")
            return UNLIT_MAT_BASE_COLOR_B;

        return UNLIT_MAT_BASE_COLOR_A;
    }

    inline const TextureView* GetUnlitTextureViewForBinding(const XJUnlitMaterial& material, const XJMaterialTextureBinding& binding)
    {
        return material.GetTextureView(ResolveUnlitTextureSlot(binding));
    }
}


#endif