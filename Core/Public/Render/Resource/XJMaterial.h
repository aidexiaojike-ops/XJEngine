#ifndef XJ_MATERIAL_H
#define XJ_MATERIAL_H

#include "Edit/Mathinclude.h"

#include "Render/Material/XJMaterialParameterBlock.h"
#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Resource/XJTexture.h"
#include "Render/XJSampler.h"

#include "entt/core/type_info.hpp"

#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace XJ
{

    struct TextureParam
    {
        alignas(4) uint32_t enable{0};
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

            XJMaterialParameterLayout mParameterLayout;
            XJMaterialParameterBlock mParameterBlock;
            std::vector<XJMaterialTextureBinding> mTextureBindings;


            friend class XJMaterialFactory;
        public:
            XJMaterial(const XJMaterial&) = delete;
            XJMaterial &operator = (const XJMaterial&) = delete;

            static void UpdateTextureParams(const TextureView *textureView, TextureParam *param)//是否开启 是否可用
            {
                param->enable = (textureView->IsValid() && textureView->bEnable) ? 1u : 0u;
                param->uvRotation = textureView->uvRotation;
                param->uvTransform = { textureView->uvScale.x, textureView->uvScale.y, textureView->uvTranslation.x, textureView->uvTranslation.y };
            }

            int32_t XJGetIndex() const {return mIndex;}
            uint32_t GetIndex() const { return static_cast<uint32_t>(mIndex); }

            bool SetParameterValue(const std::string& parameterName, const XJMaterialParameterValue& value);//设置材质参数值
            bool SetUboMemberValue(const std::string& uboName,const std::string& memberName,XJShaderParameterType type,const XJMaterialParameterValue& value);//设置材质UBO成员值
            bool SetUboMemberBytes(const std::string& uboName, const std::string& memberName, const void* data, uint32_t size);//设置材质UBO成员字节数据
            //--------------------------------
            // Runtime Parameter
            //--------------------------------
            const XJMaterialParameterLayout& GetParameterLayout() const { return mParameterLayout; }
            XJMaterialParameterLayout& GetParameterLayout() { return mParameterLayout; }
            void SetParameterLayout(const XJMaterialParameterLayout& layout) { mParameterLayout = layout;  MarkParameterDirty();}
            
            const XJMaterialParameterBlock& GetParameterBlock() const { return mParameterBlock; }
            XJMaterialParameterBlock& GetParameterBlock() { return mParameterBlock; }
            void SetParameterBlock(const XJMaterialParameterBlock& block) { mParameterBlock = block; MarkParameterDirty(); }

            bool HasRuntimeParameterBlock() const { return mParameterLayout.IsValid() && !mParameterBlock.Empty(); }

            //--------------------------------
            // Runtime Texture Binding
            //--------------------------------
            const std::vector<XJMaterialTextureBinding>& GetTextureBindings() const { return mTextureBindings; }
            std::vector<XJMaterialTextureBinding>& GetTextureBindings() { return mTextureBindings; }
            void SetTextureBindings(const std::vector<XJMaterialTextureBinding>& bindings) { mTextureBindings = bindings; MarkTextureDirty(); }

            //--------------------------------
            // Texture View
            //--------------------------------
            bool HasTexture(uint32_t id) const;
            const TextureView* GetTextureView(uint32_t id) const;
            void SetTextureView(uint32_t id, const std::shared_ptr<XJTexture>& texture, const std::shared_ptr<XJSampler>& sampler);

            void UpdateTextureViewEnable(uint32_t id, bool enable);
            void UpdateTextureViewUVTranslation(uint32_t id, const glm::vec2 &uvTranslation);
            void UpdateTextureViewUVRotation(uint32_t id, float uvRotation);
            void UpdateTextureViewUVScale(uint32_t id, const glm::vec2 &uvScale);

            //--------------------------------
            // Dirty
            //--------------------------------
            bool IsParameterDirty() const { return bShouldFlushParams; }
            bool IsTextureDirty() const { return bShouldFlushResoure; }

            bool ShouldFlushParams() const { return IsParameterDirty(); }
            bool ShouldFlushResoure() const { return IsTextureDirty(); }

            void MarkParameterDirty() { bShouldFlushParams = true; }//标记材质参数需要刷新
            void MarkTextureDirty() { bShouldFlushResoure = true; }//标记材质参数和纹理需要刷新

            void ClearParameterDirty() { bShouldFlushParams = false; }
            void ClearTextureDirty() { bShouldFlushResoure = false; }
            void ClearDirty()
            {
                ClearParameterDirty();
                ClearTextureDirty();
            }

            void FinishFlushParams() { ClearParameterDirty(); }
            void FinishFlushResoure() { ClearTextureDirty(); }




        
        protected:
            XJMaterial() = default;
            // Compatibility names for current material setters. 当前材质设置器的兼容名称
            bool bShouldFlushParams = false;
            bool bShouldFlushResoure = false;
    };

    
}



#endif
