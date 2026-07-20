#ifndef XJ_UNLIT_MATERIAL_COMPONENT_H
#define XJ_UNLIT_MATERIAL_COMPONENT_H

#include "ECS/Component/Material/XJMaterialComponent.h"


namespace XJ
{

    enum UnlitMaterialTexture
    {
        UNLIT_MAT_BASE_COLOR   
    };

    struct FrameUbo
    {
        glm::mat4  projMat{ 1.f };
        glm::mat4  viewMat{ 1.f };
        alignas(8) glm::ivec2 resolution;
        alignas(4) uint32_t frameId;
        alignas(4) float time;
    };

    
    class XJUnlitMaterial : public XJMaterial
    {
        public:
            //设置材质参数值
            void SetBaseColor(const glm::vec4& color)
            {
                SetPrimaryUboMemberValue("baseColor", XJShaderParameterType::Color4, color);
            }

          
    };

    class XJUnlitMaterialComponent : public XJMaterialComponent<XJUnlitMaterial>//runtime render data
    {
        private:
            /* data */
        public:
       
    };
    

    
}

#endif
