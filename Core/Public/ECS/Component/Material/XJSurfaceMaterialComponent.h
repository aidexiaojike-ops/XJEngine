#ifndef XJ_SURFACE_MATERIAL_COMPONENT_H
#define XJ_SURFACE_MATERIAL_COMPONENT_H

#include "ECS/Component/Material/XJMaterialComponent.h"

namespace XJ
{

    enum SurfaceMaterialTexture
    {
        SURFACE_MAT_BASE_COLOR
    };

    class XJSurfaceMaterial : public XJMaterial
    {
        public:
            //设置材质参数值
            void SetBaseColor(const glm::vec4& color)
            {
                SetPrimaryUboMemberValue("baseColor", XJShaderParameterType::Color4, color);
            }

          
    };

    class XJSurfaceMaterialComponent : public XJMaterialComponent<XJSurfaceMaterial>//runtime render data
    {
        private:
            /* data */
        public:
       
    };
    

    
}

#endif
