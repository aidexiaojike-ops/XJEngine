#ifndef XJ_SURFACE_MATERIAL_BINDING_UTILS_H
#define XJ_SURFACE_MATERIAL_BINDING_UTILS_H

#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Resource/XJMaterial.h"
#include "ECS/Component/Material/XJSurfaceMaterialComponent.h"
// Compatibility mapping for current Unlit shader slots.
// Runtime descriptor binding should prefer sampler-name based TextureView.

namespace XJ
{
    inline uint32_t ResolveSurfaceTextureSlot(const XJMaterialTextureBinding& binding)
    {
        if(binding.ParameterName == "AlbedoTexture" || binding.SamplerName == "albedoTexture")
            return SURFACE_MAT_BASE_COLOR;

        return SURFACE_MAT_BASE_COLOR;
    }

    inline const TextureView* GetSurfaceTextureViewForBinding(const XJMaterial& material, const XJMaterialTextureBinding& binding)
    {
        return material.GetTextureView(ResolveSurfaceTextureSlot(binding));
    }
}


#endif
