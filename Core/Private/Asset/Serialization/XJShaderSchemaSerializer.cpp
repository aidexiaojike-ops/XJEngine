#include "Asset/Serialization/XJShaderSchemaSerializer.h"
// #include "Render/Shader/XJShaderParameter.h"
#include "Render/Shader/XJShaderParameterValueIO.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
    namespace
    {
        XJParameterDef ReadParameterDef(const nlohmann::json& j)
        {
            XJParameterDef def;
        
            def.Name = j.value("name", std::string{});
            def.DisplayName = j.value("displayName", def.Name);
            def.Type = XJShaderParameterTypeFromString(j.value("type", std::string{}));
            def.Category = j.value("category", std::string{});
            def.Editable = j.value("editable", true);
            def.UboName = j.value("ubo", std::string{});
            def.MemberName = j.value("member", std::string{});
            def.SamplerName = j.value("sampler", std::string{});
        
            if (j.contains("min") && j.contains("max"))
            {
                def.HasRange = true;
                def.Min = j.value("min", 0.0f);
                def.Max = j.value("max", 1.0f);
            }
        
            if (j.contains("default"))
                def.DefaultValue = ReadShaderParameterValue(j["default"], def.Type, glm::vec4(0.0f));
        
            return def;
        }
    
        nlohmann::json WriteParameterDef(const XJParameterDef& def)
        {
            nlohmann::json j;
        
            j["name"] = def.Name;
            j["displayName"] = def.DisplayName.empty() ? def.Name : def.DisplayName;
            j["type"] = XJShaderParameterTypeToString(def.Type);
            j["default"] = WriteShaderParameterValue(def.DefaultValue);
            j["editable"] = def.Editable;
        
            if (!def.Category.empty())
                j["category"] = def.Category;
        
            if (!def.UboName.empty())
                j["ubo"] = def.UboName;
        
            if (!def.MemberName.empty())
                j["member"] = def.MemberName;
        
            if (!def.SamplerName.empty())
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