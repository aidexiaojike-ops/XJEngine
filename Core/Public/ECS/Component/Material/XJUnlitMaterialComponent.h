#ifndef XJ_UNLIT_MATERIAL_COMPONENT_H
#define XJ_UNLIT_MATERIAL_COMPONENT_H

#include "ECS/Component/Material/XJMaterialComponent.h"


namespace XJ
{
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
            UnlitMaterialUbo mParams{};
    };

    class XJUnlitMaterialComponent : public XJMaterialComponent<XJUnlitMaterial>
    {
        private:
            /* data */
        public:
       
    };
    

    
}

#endif