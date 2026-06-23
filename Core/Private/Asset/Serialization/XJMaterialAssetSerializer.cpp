#include "Asset/Serialization/XJMaterialAssetSerializer.h"

#include "Asset/Serialization/XJShaderAssetSerializer.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
    namespace
    {

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

        std::filesystem::path ResolveRelativePath(const std::filesystem::path& ownerFile, const std::filesystem::path& referencedPath)//路径解析
        {
            if (referencedPath.empty())
                return referencedPath;      

            if (referencedPath.is_absolute())
                return referencedPath.lexically_normal();       

            if (std::filesystem::exists(referencedPath))
                return referencedPath.lexically_normal();       

            std::filesystem::path resolved = ownerFile.parent_path() / referencedPath;
            return resolved.lexically_normal();
        }

        XJMaterialParameterValue ReadValueByType(const nlohmann::json& value, XJShaderParameterType type)//读取参数？
        {
            switch (type)
            {
                case XJShaderParameterType::Float:
                    return value.is_number() ? value.get<float>() : 0.0f;

                case XJShaderParameterType::Int:
                    return value.is_number_integer() ? value.get<int>() : 0;

                case XJShaderParameterType::Bool:
                    return value.is_boolean() ? value.get<bool>() : false;

                case XJShaderParameterType::Vec2:
                {
                    if (!value.is_array() || value.size() < 2)
                        return glm::vec2(0.0f);

                    return glm::vec2(value[0].get<float>(), value[1].get<float>());
                }

                case XJShaderParameterType::Vec3:
                case XJShaderParameterType::Color3:
                {
                    if (!value.is_array() || value.size() < 3)
                        return glm::vec3(0.0f);

                    return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
                }

                case XJShaderParameterType::Vec4:
                case XJShaderParameterType::Color4:
                {
                    if (!value.is_array() || value.size() < 4)
                        return glm::vec4(1.0f);

                    return glm::vec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                }

                case XJShaderParameterType::Texture2D:
                {
                    if (value.is_number_unsigned())
                        return static_cast<XJAssetHandle>(value.get<uint64_t>());

                    if (value.is_number_integer())
                        return static_cast<XJAssetHandle>(value.get<int64_t>());

                    return static_cast<XJAssetHandle>(0);
                }

                default:
                    return std::monostate{};
            }
        }

        nlohmann::json WriteValue(const XJMaterialParameterValue& value)//写入参数
        {
            if (std::holds_alternative<float>(value))
                return std::get<float>(value);

            if (std::holds_alternative<int>(value))
                return std::get<int>(value);

            if (std::holds_alternative<bool>(value))
                return std::get<bool>(value);

            if (std::holds_alternative<glm::vec2>(value))
            {
                const auto& v = std::get<glm::vec2>(value);
                return nlohmann::json::array({ v.x, v.y });
            }

            if (std::holds_alternative<glm::vec3>(value))
            {
                const auto& v = std::get<glm::vec3>(value);
                return nlohmann::json::array({ v.x, v.y, v.z });
            }

            if (std::holds_alternative<glm::vec4>(value))
            {
                const auto& v = std::get<glm::vec4>(value);
                return nlohmann::json::array({ v.x, v.y, v.z, v.w });
            }

            if (std::holds_alternative<XJAssetHandle>(value))
                return static_cast<uint64_t>(std::get<XJAssetHandle>(value));

            return nullptr;
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

                asset.Parameters["BaseColor"] = asset.BaseColorFactor;
            }

            asset.MetallicFactor = root.value("metallic", asset.MetallicFactor);
            asset.RoughnessFactor = root.value("roughness", asset.RoughnessFactor);
            asset.AlbedoTexture = root.value("albedoTexture", static_cast<XJAssetHandle>(0));

            asset.Parameters["Metallic"] = asset.MetallicFactor;
            asset.Parameters["Roughness"] = asset.RoughnessFactor;
            asset.Parameters["AlbedoTexture"] = asset.AlbedoTexture;
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
                if (parametersJson.contains(def.Name))
                    materialAsset->Parameters[def.Name] = ReadValueByType(parametersJson[def.Name], def.Type);
                else
                    materialAsset->Parameters[def.Name] = def.DefaultValue;
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

        if (auto* value = materialAsset->FindParameter("Metallic"))
        {
            if (std::holds_alternative<float>(*value))
                materialAsset->MetallicFactor = std::get<float>(*value);
        }

        if (auto* value = materialAsset->FindParameter("Roughness"))
        {
            if (std::holds_alternative<float>(*value))
                materialAsset->RoughnessFactor = std::get<float>(*value);
        }

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
        root["shader"] = materialAsset.ShaderPath.generic_string();
        root["parameters"] = nlohmann::json::object();

        for (const auto& [name, value] : materialAsset.Parameters)
            root["parameters"][name] = WriteValue(value);

        std::ofstream out(path);
        if (!out.is_open())
            return false;

        out << root.dump(2);
        return out.good();
    }
}