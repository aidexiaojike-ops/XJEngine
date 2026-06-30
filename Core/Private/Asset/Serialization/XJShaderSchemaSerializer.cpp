#include "Asset/Serialization/XJShaderSchemaSerializer.h"
// #include "Render/Shader/XJShaderParameter.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
    namespace
    {
        XJMaterialParameterValue ReadParameterValue(const nlohmann::json& value, XJShaderParameterType type)//读取参数
        {   
            switch(type)
            {
                case XJShaderParameterType::Float:
                    return value.is_number()? value.get<float>() : 0.0f;

                case XJShaderParameterType::Int:
                    return value.is_number_integer()? value.get<int>(): 0;

                case XJShaderParameterType::Bool:
                    return value.is_boolean()? value.get<bool>(): false;

                case XJShaderParameterType::Vec2:
                {
                    if(!value.is_array() || value.size() < 2)
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
                        return glm::vec4(0.0f);

                    return glm::vec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                }

                case XJShaderParameterType::Texture2D:
                {
                    if(value.is_number_unsigned())
                        return static_cast<XJAssetHandle>(value.get<uint64_t>());

                    if(value.is_number_integer())
                        return static_cast<XJAssetHandle>(value.get<int64_t>());

                    return static_cast<XJAssetHandle>(0);
                }

                default:
                    return std::monostate{};
            }

        }

        nlohmann::json WriteParameterValue(const XJMaterialParameterValue& value)//写参数
        {
            if(std::holds_alternative<float>(value))
                return std::get<float>(value);
            
            if(std::holds_alternative<int>(value))
                return std::get<int>(value);

            if(std::holds_alternative<bool>(value))
                return std::get<bool>(value);
            
            if(std::holds_alternative<glm::vec2>(value))
            {
                const auto& v = std::get<glm::vec2>(value);
                return nlohmann::json::array({v.x, v.y});
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

            if(std::holds_alternative<XJAssetHandle>(value))
                return static_cast<uint64_t>(std::get<XJAssetHandle>(value));

            return nullptr;
        }

        XJParameterDef ReadParameterDef(const nlohmann::json& j)//默认值
        {
            XJParameterDef def;
            //读取字段
            def.Name = j.value("name", std::string{});
            def.DisplayName = j.value("displayName", def.Name);
            def.Type = XJShaderParameterTypeFromString(j.value("type",std::string{}));
            def.Category = j.value("category", std::string{});
            def.Editable = j.value("editable", true);
            def.UboName = j.value("ubo",std::string{});
            def.MemberName = j.value("member",std::string{});
            def.SamplerName = j.value("sampler",std::string{});

            if(j.contains("min") && j.contains("max"))
            {
                def.HasRange = true;
                def.Min = j.value("min",0.0f);
                def.Max = j.value("max", 1.0f);
            }

            if(j.contains("default"))
                def.DefaultValue = ReadParameterValue(j["default"], def.Type);

            return def;
        }
        //保存字段
        nlohmann::json WriteParameterDef(const XJParameterDef& def)//写默认值
        {
            nlohmann::json j;
            j["name"] = def.Name;
            j["displayName"] = def.DisplayName.empty() ? def.Name : def.DisplayName;
            j["type"] = XJShaderParameterTypeToString(def.Type);
            j["default"] = WriteParameterValue(def.DefaultValue);
            j["editable"] = def.Editable;

            if (!def.Category.empty())
                j["category"] = def.Category;

            if(!def.UboName.empty())
                j["ubo"] = def.UboName;

            if(!def.MemberName.empty())
                j["member"] = def.MemberName;

            if(!def.SamplerName.empty())
                j["sampler"] = def.SamplerName;

            if (def.HasRange)
            {
                j["min"] = def.Min;
                j["max"] = def.Max;
            }

            return j;
        }
    }

    std::shared_ptr<XJShaderSchema> XJShaderSchemaSerializer::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if(!in.is_open())
            return nullptr;

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(in);
        }
        catch(const nlohmann::json::exception&)
        {
            return nullptr;
        }

        auto schema = std::make_shared<XJShaderSchema>();
        schema->Version = root.value("version", 1u);

        if(!root.contains("parameters") || !root["parameters"].is_array())
            return schema;

        for (const auto& parameterJson : root["parameters"])
        {
            XJParameterDef def = ReadParameterDef(parameterJson);
            if (!def.Name.empty() && def.Type != XJShaderParameterType::None)
                schema->Parameters.push_back(def);
        }

        return schema;
    }

    bool XJShaderSchemaSerializer::SaveToFile(const XJShaderSchema& schema, const std::filesystem::path& path)
    {
        if(path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json root;
        root["version"] = schema.Version;
        root["parameters"] = nlohmann::json::array();

        for (const auto& parameter : schema.Parameters)
            root["parameters"].push_back(WriteParameterDef(parameter));

        std::ofstream out(path);
        if (!out.is_open())
            return false;

        out << root.dump(2);
        return out.good();
    }
}