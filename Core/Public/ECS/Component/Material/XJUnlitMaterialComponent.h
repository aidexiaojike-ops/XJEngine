#ifndef XJ_UNLIT_MATERIAL_COMPONENT_H
#define XJ_UNLIT_MATERIAL_COMPONENT_H

#include "ECS/Component/Material/XJMaterialComponent.h"


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
    struct UnlitMaterialUbo
    {
        alignas(16) glm::vec3 baseColorA;
        alignas(16) glm::vec3 baseColorB;
        alignas(4) float mixValue;
        alignas(16) TextureParam textureParamA;
        alignas(16) TextureParam textureParamB;
    };

    
    class XJUnlitMaterial : public XJMaterial
    {
        public:
        const  UnlitMaterialUbo &XJGetParams() const { return mParams; }
        const  glm::vec3 XJGetBaseColorA() const {return mParams.baseColorA;};
        const  glm::vec3 XJGetBaseColorB() const {return mParams.baseColorB;};
        float XJGetMixValue() const{return mParams.mixValue;};
        const  TextureParam &XJGetTextureParamA() const {return mParams.textureParamA;};
        const  TextureParam &XJGetTextureParamB() const {return mParams.textureParamB;};

        void XJSetBaseColorA(const glm::vec3 &color){    mParams.baseColorA = color; bShouldFlushParams = true;}
        void XJSetBaseColorB(const glm::vec3 &color){    mParams.baseColorB = color; bShouldFlushParams = true;}
        void XJSetMixValue(float mixValue){    mParams.mixValue = mixValue; bShouldFlushParams = true;}
        void XJSetTextureParamA(const TextureParam &texParam){    mParams.textureParamA = texParam; bShouldFlushParams = true;}
        void XJSetTextureParamB(const TextureParam &texParam){    mParams.textureParamB = texParam; bShouldFlushParams = true;}

        private:
            UnlitMaterialUbo mParams{};//材质参数
    };

    class XJUnlitMaterialComponent : public XJMaterialComponent<XJUnlitMaterial>
    {
        private:
            /* data */
        public:
       
    };
    

    
}

#endif