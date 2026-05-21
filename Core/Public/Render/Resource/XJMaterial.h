#ifndef XJ_MATERIAL_H
#define XJ_MATERIAL_H

#include "Edit/Mathinclude.h"
#include "Render/XJSampler.h"
#include "Render/Resource/XJTexture.h"
#include "entt/core/type_info.hpp"

namespace XJ
{

    struct TextureParam
    {
        bool enable;
        alignas(4) float uvRotation{0.0f};//内存对齐的
        alignas(16) glm::vec4 uvTransform{1.0f,1.0f,1.0f,0.0f};//手动对其
    };

    struct TextureView
    {
        std::shared_ptr<XJTexture> texture;//纹理
        std::shared_ptr<XJSampler> sampler;//采样
        bool bEnable = true;//是否启用
        glm::vec2 uvTranslation{0.f, 0.f};//uv位移
        float uvRotation{0.f};//uv旋转
        glm::vec2 uvScale{1.0f ,1.0f};//uv缩放


        bool IsValid() const
        {
            return bEnable && texture != nullptr && sampler !=nullptr;
        }
    };

    struct PushConstants
    {
        glm::mat4 matrix{1.0f}; // 4x4 矩阵，默认初始化为单位矩阵
        uint32_t colorType = 0;
    };// 推送常量结构体
   
    struct ModelPC
    {
        alignas(16) glm::mat4 modelMat;
        alignas(16) glm::mat3 normalMat;
    };

    class XJMaterial
    {
        private:
            /* data */
            int32_t mIndex = -1;//材质索引   DescriptorSet
            std::unordered_map<uint32_t, TextureView> mTextures;
            friend class XJMaterialFactory;
        public:
            XJMaterial(const XJMaterial&) = delete;
            XJMaterial &operator = (const XJMaterial&) = delete;

            static void UpdateTextureParams(const TextureView *textureView, TextureParam *param)//是否开启 是否可用
            {
                param->enable = textureView->IsValid() && textureView->bEnable;
                param->uvRotation = textureView->uvRotation;
                param->uvTransform = { textureView->uvScale.x, textureView->uvScale.y, textureView->uvTranslation.x, textureView->uvTranslation.y };
            }

            int32_t XJGetIndex() const {return mIndex;}
            bool ShouldFlushParams() const {return bShouldFlushParams;}  
            bool ShouldFlushResoure() const {return bShouldFlushResoure;}
            void FinishFlushParams()  { bShouldFlushParams = false;}  
            void FinishFlushResoure() { bShouldFlushResoure = false;}

            bool HasTexture(uint32_t id) const;
            const TextureView* XJGetTextureView(uint32_t id) const;
            void XJSetTextureView(uint32_t id, const std::shared_ptr<XJTexture>& texture, const std::shared_ptr<XJSampler>& sampler);
            
            void UpdateTextureViewEnable(uint32_t id, bool enable);
            void UpdateTextureViewUVTranslation(uint32_t id, const glm::vec2 &uvTranslation);
            void UpdateTextureViewUVRotation(uint32_t id, float uvRotation);
            void UpdateTextureViewUVScale(uint32_t id, const glm::vec2 &uvScale);
        
        protected:
            XJMaterial() = default;

            bool bShouldFlushParams = false;
            bool bShouldFlushResoure = false;
    };

    
}



#endif