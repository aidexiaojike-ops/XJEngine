#include "Asset/Serialization/XJMaterialAssetSerializer.h"
#include "Render/Shader/XJShaderParameterValueIO.h"
#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Asset/XJAssetPathUtils.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
    namespace
    {

        std::string ToProjectRelativePathString(const std::filesystem::path& path)//将路径转换为项目相对路径字符串
        {
            if (path.empty())
                return {};
        
            std::filesystem::path normalized = path.lexically_normal();
        
            std::string generic = normalized.generic_string();
        
            const std::string resourcePrefix = "Resource/";
            size_t resourcePos = generic.find(resourcePrefix);
            if (resourcePos != std::string::npos)
                return generic.substr(resourcePos);
        
            const std::string binResourcePrefix = "bin/Resource/";
            size_t binResourcePos = generic.find(binResourcePrefix);
            if (binResourcePos != std::string::npos)
                return generic.substr(binResourcePos + 4);
        
            return generic;
        }

        bool MaterialParameterValueEquals(const XJMaterialParameterValue& a, const XJMaterialParameterValue& b)//比较函数，用来判断 override 是否等于 default，避免保存无意义 override：
        {
            if (a.index() != b.index())
                return false;
        
            if (std::holds_alternative<std::monostate>(a))
                return true;
        
            if (std::holds_alternative<float>(a))
                return std::get<float>(a) == std::get<float>(b);
        
            if (std::holds_alternative<int>(a))
                return std::get<int>(a) == std::get<int>(b);
        
            if (std::holds_alternative<bool>(a))
                return std::get<bool>(a) == std::get<bool>(b);
        
            if (std::holds_alternative<glm::vec2>(a))
                return std::get<glm::vec2>(a) == std::get<glm::vec2>(b);
        
            if (std::holds_alternative<glm::vec3>(a))
                return std::get<glm::vec3>(a) == std::get<glm::vec3>(b);
        
            if (std::holds_alternative<glm::vec4>(a))
                return std::get<glm::vec4>(a) == std::get<glm::vec4>(b);
        
            if (std::holds_alternative<XJAssetHandle>(a))
                return std::get<XJAssetHandle>(a) == std::get<XJAssetHandle>(b);
        
            return false;
        }


        void LoadLegacyFields(const nlohmann::json& root, XJMaterialAsset& asset)
        {
            if (root.contains("baseColor") && root["baseColor"].is_array() && root["baseColor"].size() >= 4)
            {
                asset.BaseColorFactor = glm::vec4(
                    root["baseColor"][0].get<float>(),
                    root["baseColor"][1].get<float>(),
                    root["baseColor"][2].get<float>(),
                    root["baseColor"][3].get<float>());

                asset.ParameterOverrides["BaseColor"] = asset.BaseColorFactor;
            }

            // asset.MetallicFactor = root.value("metallic", asset.MetallicFactor);
            // asset.RoughnessFactor = root.value("roughness", asset.RoughnessFactor);
            asset.AlbedoTexture = root.value("albedoTexture", static_cast<XJAssetHandle>(0));

            asset.ParameterOverrides["BaseColor"] = asset.BaseColorFactor;
            // asset.ParameterOverrides["Metallic"] = asset.MetallicFactor;
            // asset.ParameterOverrides["Roughness"] = asset.RoughnessFactor;
            asset.ParameterOverrides["AlbedoTexture"] = asset.AlbedoTexture;

            asset.Parameters = asset.ParameterOverrides;
        }

    }

    std::shared_ptr<XJMaterialAsset> XJMaterialAssetSerializer::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream in(path);//获取路径
        if (!in.is_open())
            return nullptr;

        nlohmann::json root;//json文件容器
        try
        {
            root = nlohmann::json::parse(in);//把路径文件存入json容器
        }
        catch (const nlohmann::json::exception&)
        {
            return nullptr;
        }

        auto materialAsset = std::make_shared<XJMaterialAsset>();//参数
        materialAsset->mType = XJAssetType::Material;
        materialAsset->mName = path.stem().string();
        materialAsset->mPath = path;
        materialAsset->Version = root.value("version", 1u);

        if (root.contains("shader"))//是否是shader json文件
        {
            materialAsset->ShaderPath = ResolveRelativePath(path, root.value("shader", std::string{}));
        }

        std::shared_ptr<XJShaderAsset> shaderAsset;
        if (!materialAsset->ShaderPath.empty())
            shaderAsset = XJShaderAssetSerializer::LoadFromFile(materialAsset->ShaderPath);
        //开始解析json文件内容
        if (root.contains("parameters") && root["parameters"].is_object() && shaderAsset)
        {
            const auto& parametersJson = root["parameters"];

            for (const auto& def : shaderAsset->Schema.Parameters)
            {
                materialAsset->Parameters[def.Name] = def.DefaultValue;

                if (parametersJson.contains(def.Name))
                {
                    XJMaterialParameterValue overrideValue = ReadShaderParameterValue(parametersJson[def.Name], def.Type, glm::vec4(1.0f));
                
                    if (!MaterialParameterValueEquals(overrideValue, def.DefaultValue))
                    {
                        materialAsset->ParameterOverrides[def.Name] = overrideValue;
                        materialAsset->Parameters[def.Name] = overrideValue;
                    }
                }
            }
        }
        else
        {
            LoadLegacyFields(root, *materialAsset);
        }

        if (auto* value = materialAsset->FindParameter("BaseColor"))
        {
            if (std::holds_alternative<glm::vec4>(*value))
                materialAsset->BaseColorFactor = std::get<glm::vec4>(*value);
        }

        //if (auto* value = materialAsset->FindParameter("Metallic"))
        //{
        //    if (std::holds_alternative<float>(*value))
        //        materialAsset->MetallicFactor = std::get<float>(*value);
        //}
//
        //if (auto* value = materialAsset->FindParameter("Roughness"))
        //{
        //    if (std::holds_alternative<float>(*value))
        //        materialAsset->RoughnessFactor = std::get<float>(*value);
        //}

        if (auto* value = materialAsset->FindParameter("AlbedoTexture"))
        {
            if (std::holds_alternative<XJAssetHandle>(*value))
                materialAsset->AlbedoTexture = std::get<XJAssetHandle>(*value);
        }

        return materialAsset;
    }

    bool XJMaterialAssetSerializer::SaveToFile(const XJMaterialAsset& materialAsset, const std::filesystem::path& path)
    {   
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json root;
        root["version"] = materialAsset.Version;
        // root["version"] = 2;//To测试
        root["shader"] = materialAsset.ShaderPath.generic_string();
        // root["shader"] = ToProjectRelativePathString(materialAsset.ShaderPath);//To测试
        root["parameters"] = nlohmann::json::object();
        //保存时只写 overrides，并且如果 override 等于 schema default
        std::shared_ptr<XJShaderAsset> shaderAsset;
        if(!materialAsset.ShaderPath.empty())
            shaderAsset = XJShaderAssetSerializer::LoadFromFile(materialAsset.ShaderPath);

        for(const auto& [name ,value]:materialAsset.ParameterOverrides)
        {
            if(shaderAsset)
            {
                const XJParameterDef* def = shaderAsset->Schema.FindParameter(name);
                if(def && MaterialParameterValueEquals(value, def->DefaultValue))
                    continue;
            }

            root["parameters"][name] = WriteShaderParameterValue(value);
        }

        std::ofstream out(path);
        if (!out.is_open())
            return false;

        out << root.dump(2);
        return out.good();
    }
}