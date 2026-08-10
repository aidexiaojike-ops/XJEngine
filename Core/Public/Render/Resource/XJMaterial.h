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
#include <cstddef>
#include <filesystem>

namespace XJ
{

    struct TextureParam
    {
        alignas(4) uint32_t enable{0};
        alignas(4) float uvRotation{0.0f};//内存对齐的
        alignas(16) glm::vec4 uvTransform{1.0f,1.0f,1.0f,0.0f};//手动对其
    };

    // 与 Shader/Unlit.frag 中 `MaterialUbo` 内的 `struct TextureParam`（std140）逐字节一致。
    // 注意 vec4 之前的 8 字节填充是 std140 要求的（16 字节对齐），由 alignas(16) 保证。
    static_assert(sizeof(TextureParam) == 32,                "TextureParam: sizeof 必须为 32（std140）");
    static_assert(offsetof(TextureParam, enable) == 0,       "TextureParam.enable 偏移必须为 0");
    static_assert(offsetof(TextureParam, uvRotation) == 4,   "TextureParam.uvRotation 偏移必须为 4");
    static_assert(offsetof(TextureParam, uvTransform) == 16, "TextureParam.uvTransform 偏移必须为 16（16 字节对齐）");

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

    // 与 Shader/BaseVertex.vert 的 `layout(push_constant)` 一致。
    // push constant 采用 std430 布局规则：mat4（64B）后紧跟 uint32_t（偏移 64）。
    static_assert(sizeof(PushConstants) == 68,               "PushConstants: sizeof 必须为 68（std430）");
    static_assert(offsetof(PushConstants, matrix) == 0,      "PushConstants.matrix 偏移必须为 0");
    static_assert(offsetof(PushConstants, colorType) == 64,  "PushConstants.colorType 偏移必须为 64");
   
    struct ModelPC
    {
        alignas(16) glm::mat4 modelMat{1.0f};   // offset 0  size 64
        alignas(16) glm::mat4 normalMat{1.0f};  // offset 64 size 64
    };

    // 与 Shader/Unlit.vert 的 push constant 一致。
    // 这里用 mat4 承载法线矩阵，避免 C++ glm::mat3 的 36 字节紧凑布局和 GLSL mat3 的列 stride 产生错位。
    static_assert(sizeof(ModelPC) == 128,                    "ModelPC: sizeof 必须为 128（mat4 + mat4）");
    static_assert(offsetof(ModelPC, modelMat) == 0,          "ModelPC.modelMat 偏移必须为 0");
    static_assert(offsetof(ModelPC, normalMat) == 64,        "ModelPC.normalMat 偏移必须为 64");

    class XJMaterial
    {
        private:
            /* data */
            int32_t mIndex = -1;//材质索引   DescriptorSet
            std::unordered_map<uint32_t, TextureView> mTextures;

            XJMaterialParameterLayout mParameterLayout;
            XJMaterialParameterBlock mParameterBlock;
            std::unordered_map<uint64_t, XJMaterialParameterBlock> mParameterBlocks;
            std::vector<XJMaterialTextureBinding> mTextureBindings;
            std::filesystem::path mShaderPath;

            std::unordered_map<std::string, TextureView> mSamplerTextures;


            friend class XJMaterialFactory;
        public:
            XJMaterial(const XJMaterial&) = delete;
            XJMaterial &operator = (const XJMaterial&) = delete;

            static void UpdateTextureParams(const TextureView *textureView, TextureParam *param)//是否开启 是否可用
            {
                if (!param)
                    return;

                if (!textureView)
                {
                    *param = {};
                    return;
                }

                param->enable = (textureView->IsValid() && textureView->bEnable) ? 1u : 0u;
                param->uvRotation = textureView->uvRotation;
                param->uvTransform = { textureView->uvScale.x, textureView->uvScale.y, textureView->uvTranslation.x, textureView->uvTranslation.y };
            }

            int32_t XJGetIndex() const {return mIndex;}
            uint32_t GetIndex() const { return static_cast<uint32_t>(mIndex); }

            const std::filesystem::path& GetShaderPath() const { return mShaderPath; }
            void SetShaderPath(const std::filesystem::path& path) { mShaderPath = path; }

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
            const std::unordered_map<uint64_t, XJMaterialParameterBlock>& GetParameterBlocks() const { return mParameterBlocks; }
            std::unordered_map<uint64_t, XJMaterialParameterBlock>& GetParameterBlocks() { return mParameterBlocks; }
            void SetParameterBlocks(const std::unordered_map<uint64_t, XJMaterialParameterBlock>& blocks);

            bool HasRuntimeParameterBlock() const { return mParameterLayout.IsValid() && (!mParameterBlock.Empty() || !mParameterBlocks.empty()); }

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
            //UboUnlit 手动 setter 仍保留，但不再写死 UBO 名称，而是使用当前 runtime layout 的 UBO 名称。
            const std::string& GetPrimaryUboName() const;
            bool SetPrimaryUboMemberValue(const std::string& memberName, XJShaderParameterType type, const XJMaterialParameterValue& value);
            bool SetPrimaryUboMemberBytes(const std::string& memberName, const void* data, uint32_t size);
            //
            bool HasSamplerTexture(const std::string& samplerName) const;
            const TextureView* GetSamplerTextureView(const std::string& samplerName) const;

            void SetSamplerTextureView(
                const std::string& samplerName,
                const std::shared_ptr<XJTexture>& texture,
                const std::shared_ptr<XJSampler>& sampler);
            
            void UpdateSamplerTextureViewEnable(const std::string& samplerName, bool enable);

        protected:
            XJMaterial() = default;
            // Compatibility names for current material setters. 当前材质设置器的兼容名称
            bool bShouldFlushParams = false;
            bool bShouldFlushResoure = false;
            
    };

    
}
#endif
