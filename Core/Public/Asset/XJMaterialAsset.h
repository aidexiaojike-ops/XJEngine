#ifndef XJ_MATERIAL_ASSET_H
#define XJ_MATERIAL_ASSET_H

#include "Asset/XJAsset.h"
#include "Render/Shader/XJShaderParameter.h"

#include <glm/glm.hpp>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace XJ
{
    class XJMaterialAsset : public XJAsset
    {
        public:

            uint32_t Version = 1;
            // uint32_t Version = 2;//To测试

            std::filesystem::path ShaderPath;
            // 最终运行时使用值：schema defaults + overrides
            std::unordered_map<std::string, XJMaterialParameterValue> Parameters; 
            // 只保存用户改过的参数
            std::unordered_map<std::string, XJMaterialParameterValue> ParameterOverrides;

            //老版本
            // 贴图引用（句柄，指向 XJTextureAsset）
            XJAssetHandle AlbedoTexture   = 0;
            XJAssetHandle NormalTexture   = 0;      
            XJAssetHandle MetallicRoughnessTexture = 0; 
            // PBR 参数
            glm::vec4 BaseColorFactor  = glm::vec4(1.0f);
            float MetallicFactor       = 1.0f;      
            float RoughnessFactor      = 1.0f;       

            const XJMaterialParameterValue* FindParameter(const std::string& name) const//获取参数
            {
                auto it = Parameters.find(name);
                if (it == Parameters.end())
                    return nullptr;

                return &it->second;
            }

            XJMaterialParameterValue* FindParameter(const std::string& name)//获取参数
            {
                auto it = Parameters.find(name);
                if (it == Parameters.end())
                    return nullptr;

                return &it->second;
            }

            void SetParameter(const std::string& name, const XJMaterialParameterValue& value)//设置参数
            {
                Parameters[name] = value;
            }

            const XJMaterialParameterValue* FindParameterOverride(const std::string& name) const//找到覆盖参数
            {
                auto it = ParameterOverrides.find(name);
                if(it == ParameterOverrides.end())
                    return nullptr;

                return &it->second;
            }

            bool HasParameterOverride(const std::string& name) const//是否有覆盖的参数
            {
                return ParameterOverrides.find(name) != ParameterOverrides.end();
            }

            void SetParameterOverride(const std::string& name, const XJMaterialParameterValue& value)//设置覆盖的参数
            {
                ParameterOverrides[name] = value;
                Parameters[name] = value;
            }

            void ClearParameterOverride(const std::string& name)//清理覆盖的参数
            {
                ParameterOverrides.erase(name);
            }

    };

}

#endif
