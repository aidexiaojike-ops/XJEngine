#ifndef XJ_MATERIAL_PARAMETER_LAYOUT_H
#define XJ_MATERIAL_PARAMETER_LAYOUT_H

#include "Render/Shader/XJShaderParameter.h"
#include "Render/Shader/XJShaderReflection.h"
#include "Render/Shader/XJShaderSchema.h"

#include <cstdint>
#include <string>
#include <vector>



namespace XJ
{
    struct XJMaterialParameterBinding//材质参数绑定
    {
        std::string ParameterName;
        XJShaderParameterType Type = XJShaderParameterType::None;

        std::string UboName;
        std::string MemberName;

        uint32_t Set = 0;
        uint32_t Binding = 0;
        uint32_t Offset = 0;
        uint32_t Size = 0;
    };

    struct XJMaterialTextureBinding//材质纹理绑定
    {
        std::string ParameterName;
        XJShaderParameterType Type = XJShaderParameterType::None;

        std::string SamplerName;
        std::string ImageName;

        uint32_t Set = 0;
        uint32_t Binding = 0;

        bool ExposedBySchema = false;//标签
    };

    struct XJMaterialUboMemberBinding//材质 UBO 成员绑定
    {
        std::string UboName;
        std::string MemberName;

        uint32_t Set = 0;
        uint32_t Binding = 0;
        uint32_t Offset = 0;
        uint32_t Size = 0;
    };

    struct XJMaterialUboLayout
    {
        std::string UboName;
        uint32_t Set = 0;
        uint32_t Binding = 0;
        uint32_t Size = 0;
    };

    inline uint64_t XJMakeMaterialUboKey(uint32_t set, uint32_t binding)
    {
        return (static_cast<uint64_t>(set) << 32) | static_cast<uint64_t>(binding);
    }

    struct XJMaterialParameterLayoutBuildResult//材质参数布局构建结果
    {
        bool Valid = false;
        std::vector<std::string> Errors;
        std::vector<std::string> Warnings;

        bool HasIssues() const
        {
            return !Errors.empty() || !Warnings.empty();
        }
    };
    
    class XJMaterialParameterLayout
    {
        public:
            XJMaterialParameterLayout() = default;

            XJMaterialParameterLayoutBuildResult Build(const XJShaderSchema& schema, const XJShaderReflectionResult& reflection);//构建材质参数布局

            void Clear();//清空材质参数布局
            bool IsValid() const {return mValid;}//判断材质参数布局是否有效

            uint32_t GetUboSize() const {return mUboSize;}//获取 UBO 大小
            const std::string& GetUboName() const {return mUboName;}//获取 UBO 名称
            uint32_t GetUboSet() const { return mUboSet; }
            uint32_t GetUboBinding() const { return mUboBinding; }

            const std::vector<XJMaterialParameterBinding>& GetParameterBindings() const {return mParameterBindings;}//获取材质参数绑定
            const std::vector<XJMaterialTextureBinding>& GetTextureBindings() const {return mTextureBindings;}//获取材质纹理绑定
            const std::vector<XJMaterialUboLayout>& GetUboLayouts() const { return mUboLayouts; }

            const XJMaterialParameterBinding* FindParameterBinding(const std::string& parameterName) const;//查找材质参数绑定
            const XJMaterialTextureBinding* FindTextureBinding(const std::string& parameterName) const;//查找材质纹理绑定
            const XJMaterialTextureBinding* FindTextureBindingBySampler(const std::string& samplerName) const;
            const XJMaterialUboMemberBinding* FindFirstUboBinding(const std::string& uboName) const;

            const std::vector<XJMaterialUboMemberBinding>& GetUboMemberBindings() const { return mUboMemberBindings; }//获取 UBO 成员绑定
            const XJMaterialUboMemberBinding* FindUboMemberBinding(const std::string& uboName, const std::string& memberName) const;//查找 UBO 成员绑定
            const XJMaterialUboLayout* FindUboLayout(uint32_t set, uint32_t binding) const;

        private:
            bool mValid = false;
            bool mHasPrimaryUbo = false;

            std::string mUboName;
            uint32_t mUboSize = 0;
            uint32_t mUboSet = 0;
            uint32_t mUboBinding = 0;

            std::vector<XJMaterialParameterBinding> mParameterBindings;
            std::vector<XJMaterialTextureBinding> mTextureBindings;
            std::vector<XJMaterialUboMemberBinding> mUboMemberBindings;
            std::vector<XJMaterialUboLayout> mUboLayouts;
    };
    
}

#endif
