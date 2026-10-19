#ifndef XJ_SCRIPT_VALUE_CODEC_H
#define XJ_SCRIPT_VALUE_CODEC_H

#include "Script/Value/XJScriptValue.h"

#include <nlohmann/json.hpp>
#include <charconv>
#include <optional>
//代码转值？
namespace XJ
{
    inline std::optional<nlohmann::json> XJEncodeScriptValue(//编码脚本值
        const XJScriptValue& value)
    {
        if (!XJIsSerializableScriptValue(value))
            return std::nullopt;

        if (const auto* item = std::get_if<bool>(&value))
            return nlohmann::json{
                {"type", "bool"}, {"value", *item}};

        if (const auto* item = std::get_if<int64_t>(&value))
            return nlohmann::json{
                {"type", "int"}, {"value", std::to_string(*item)}};

        if (const auto* item = std::get_if<double>(&value))
            return nlohmann::json{
                {"type", "float"}, {"value", *item}};

        if (const auto* item = std::get_if<std::string>(&value))
            return nlohmann::json{
                {"type", "string"}, {"value", *item}};

        if (const auto* item = std::get_if<XJScriptVec3>(&value))
        {
            return nlohmann::json{
                {"type", "vec3"},
                {"value", nlohmann::json::array(
                    {item->X, item->Y, item->Z})}
            };
        }

        return std::nullopt;
    }

    inline std::optional<XJScriptValue> XJDecodeScriptValue(//解码脚本值
        const nlohmann::json& json,
        bool allowVec3)
    {
        if (!json.is_object() ||
            !json.contains("type") ||
            !json["type"].is_string() ||
            !json.contains("value"))
        {
            return std::nullopt;
        }

        const auto type = json["type"].get<std::string>();
        const auto& value = json["value"];

        if (type == "bool" && value.is_boolean())
            return XJScriptValue{value.get<bool>()};

        if (type == "string" && value.is_string())
            return XJScriptValue{value.get<std::string>()};

        if (type == "float" && value.is_number())
        {
            const double number = value.get<double>();
            if (std::isfinite(number))
                return XJScriptValue{number};
        }

        if (type == "int" && value.is_string())
        {
            const auto text = value.get<std::string>();
            int64_t number = 0;

            const auto parsed = std::from_chars(
                text.data(), text.data() + text.size(), number);

            if (parsed.ec == std::errc{} &&
                parsed.ptr == text.data() + text.size())
            {
                return XJScriptValue{number};
            }
        }

        if (type == "vec3" && allowVec3 &&
            value.is_array() && value.size() == 3)
        {
            for (const auto& coordinate : value)
            {
                if (!coordinate.is_number())
                    return std::nullopt;
            }

            const XJScriptVec3 vector{
                value[0].get<double>(),
                value[1].get<double>(),
                value[2].get<double>()
            };

            if (vector.IsFinite())
                return XJScriptValue{vector};
        }

        // EntityRef 无论是否为空，都不是可持久化字段值。
        return std::nullopt;
    }
}

#endif