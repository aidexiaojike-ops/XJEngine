#include "Asset/Serialization/XJShaderSchemaSerializer.h"
// #include "Render/Shader/XJShaderParameter.h"
#include "Render/Shader/XJShaderParameterValueIO.h"
#include "Asset/Serialization/XJJsonIO.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
    namespace
    {
        XJParameterDef ReadParameterDef(const nlohmann::json& j)
        {
            XJParameterDef def;
            if (!j.is_object())
                return def;
        
            def.Name = JsonReadStringOr(j, "name");
            def.DisplayName = JsonReadStringOr(j, "displayName", def.Name);
            def.Type = XJShaderParameterTypeFromString(JsonReadStringOr(j, "type"));
            def.Category = JsonReadStringOr(j, "category");
            def.Editable = JsonReadBoolOr(j, "editable", true);
            def.UboName = JsonReadStringOr(j, "ubo");
            def.MemberName = JsonReadStringOr(j, "member");
            def.SamplerName = JsonReadStringOr(j, "sampler");
        
            if (j.contains("min") && j.contains("max"))
            {
                def.HasRange = true;
                def.Min = JsonReadFloatOr(j, "min", 0.0f);
                def.Max = JsonReadFloatOr(j, "max", 1.0f);
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
        schema->Version = JsonReadUInt32Or(root, "version", 1u);

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
        nlohmann::json root;
        root["version"] = schema.Version;
        root["parameters"] = nlohmann::json::array();

        for (const auto& parameter : schema.Parameters)
            root["parameters"].push_back(WriteParameterDef(parameter));

        return WriteJsonFileAtomic(path, root);
    }
}
