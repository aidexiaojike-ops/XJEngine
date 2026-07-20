#ifndef XJ_UNLIT_MATERIAL_BINDING_UTILS_H
#define XJ_UNLIT_MATERIAL_BINDING_UTILS_H

#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Resource/XJMaterial.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
// Compatibility mapping for current Unlit shader slots.
// Runtime descriptor binding should prefer sampler-name based TextureView.

namespace XJ
{
    inline uint32_t ResolveUnlitTextureSlot(const XJMaterialTextureBinding& binding)
    {
        if(binding.ParameterName == "AlbedoTexture" || binding.SamplerName == "albedoTexture")
            return UNLIT_MAT_BASE_COLOR;

        return UNLIT_MAT_BASE_COLOR;
    }

    inline const TextureView* GetUnlitTextureViewForBinding(const XJMaterial& material, const XJMaterialTextureBinding& binding)
    {
        return material.GetTextureView(ResolveUnlitTextureSlot(binding));
    }
}


#endif
