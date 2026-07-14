#ifndef XJ_UNLIT_MATERIAL_COMPONENT_H
#define XJ_UNLIT_MATERIAL_COMPONENT_H

#include "ECS/Component/Material/XJMaterialComponent.h"
#include <spdlog/spdlog.h>


namespace XJ
{

    enum UnlitMaterialTexture
    {
        UNLIT_MAT_BASE_COLOR_A,
        UNLIT_MAT_BASE_COLOR_B
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
            void SetBaseColorA(const glm::vec4 &color)
            { 
                spdlog::info("SetBaseColorA rgba=({}, {}, {}, {})", color.r, color.g, color.b, color.a);
                SetPrimaryUboMemberValue("baseColorA", XJShaderParameterType::Color4, color);
            }
            void SetBaseColorB(const glm::vec4 &color)
            { 
                spdlog::info("SetBaseColorB rgba=({}, {}, {}, {})", color.r, color.g, color.b, color.a);
                SetPrimaryUboMemberValue("baseColorB", XJShaderParameterType::Color4, color);
            }
            void SetMixValue(float mixValue){ SetPrimaryUboMemberValue( "mixValue", XJShaderParameterType::Float, mixValue);}
            void SetTextureParamA(const TextureParam &texParam){ SetPrimaryUboMemberBytes("textureParamA", &texParam, sizeof(texParam));}
            void SetTextureParamB(const TextureParam &texParam){ SetPrimaryUboMemberBytes( "textureParamB", &texParam, sizeof(texParam));}

        private:
          
    };

    class XJUnlitMaterialComponent : public XJMaterialComponent<XJUnlitMaterial>//runtime render data
    {
        private:
            /* data */
        public:
       
    };
    

    
}

#endif