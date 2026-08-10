#ifndef XJ_JSON_IO_H
#define XJ_JSON_IO_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>

namespace XJ
{
    inline bool JsonReadFloat(const nlohmann::json& value, float& outValue)
    {
        if (!value.is_number())
            return false;

        outValue = value.get<float>();
        return true;
    }

    inline float JsonReadFloatOr(const nlohmann::json& object, const char* key, float fallback)
    {
        if (!object.is_object() || !object.contains(key))
            return fallback;

        float value = fallback;
        return JsonReadFloat(object[key], value) ? value : fallback;
    }

    inline bool JsonReadBoolOr(const nlohmann::json& object, const char* key, bool fallback)
    {
        if (!object.is_object() || !object.contains(key) || !object[key].is_boolean())
            return fallback;

        return object[key].get<bool>();
    }

    inline int JsonReadIntOr(const nlohmann::json& object, const char* key, int fallback)
    {
        if (!object.is_object() || !object.contains(key) || !object[key].is_number_integer())
            return fallback;

        return object[key].get<int>();
    }

    inline std::string JsonReadStringOr(const nlohmann::json& object, const char* key, const std::string& fallback = {})
    {
        if (!object.is_object() || !object.contains(key) || !object[key].is_string())
            return fallback;

        return object[key].get<std::string>();
    }

    inline bool JsonReadUInt64(const nlohmann::json& value, uint64_t& outValue)
    {
        if (value.is_number_unsigned())
        {
            outValue = value.get<uint64_t>();
            return true;
        }

        if (value.is_number_integer())
        {
            const int64_t signedValue = value.get<int64_t>();
            if (signedValue < 0)
                return false;

            outValue = static_cast<uint64_t>(signedValue);
            return true;
        }

        if (value.is_string())
        {
            const std::string text = value.get<std::string>();
            if (text.empty())
                return false;

            try
            {
                int base = 10;
                std::string number = text;

                if (number.rfind("0x", 0) == 0 || number.rfind("0X", 0) == 0)
                {
                    base = 16;
                    number = number.substr(2);
                }
                else if (number.find_first_of("abcdefABCDEF") != std::string::npos)
                {
                    base = 16;
                }

                size_t parsedLength = 0;
                outValue = std::stoull(number, &parsedLength, base);
                if (parsedLength != number.size())
                    return false;

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        return false;
    }

    inline uint64_t JsonReadUInt64Or(const nlohmann::json& object, const char* key, uint64_t fallback = 0)
    {
        if (!object.is_object() || !object.contains(key))
            return fallback;

        uint64_t value = fallback;
        return JsonReadUInt64(object[key], value) ? value : fallback;
    }

    inline uint32_t JsonReadUInt32Or(const nlohmann::json& object, const char* key, uint32_t fallback)
    {
        if (!object.is_object() || !object.contains(key))
            return fallback;

        uint64_t value = fallback;
        if (!JsonReadUInt64(object[key], value))
            return fallback;

        if (value > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()))
            return fallback;

        return static_cast<uint32_t>(value);
    }

    inline glm::vec2 JsonReadVec2Or(const nlohmann::json& value, const glm::vec2& fallback)
    {
        if (!value.is_array() || value.size() < 2)
            return fallback;

        float x = 0.0f;
        float y = 0.0f;

        if (!JsonReadFloat(value[0], x) || !JsonReadFloat(value[1], y))
            return fallback;

        return { x, y };
    }

    inline glm::vec3 JsonReadVec3Or(const nlohmann::json& value, const glm::vec3& fallback)
    {
        if (!value.is_array() || value.size() < 3)
            return fallback;

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        if (!JsonReadFloat(value[0], x) ||
            !JsonReadFloat(value[1], y) ||
            !JsonReadFloat(value[2], z))
        {
            return fallback;
        }

        return { x, y, z };
    }

    inline glm::vec4 JsonReadVec4Or(const nlohmann::json& value, const glm::vec4& fallback)
    {
        if (!value.is_array() || value.size() < 4)
            return fallback;

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        if (!JsonReadFloat(value[0], x) ||
            !JsonReadFloat(value[1], y) ||
            !JsonReadFloat(value[2], z) ||
            !JsonReadFloat(value[3], w))
        {
            return fallback;
        }

        return { x, y, z, w };
    }

    inline bool WriteJsonFileAtomic(const std::filesystem::path& path, const nlohmann::json& root)
    {
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        const std::filesystem::path tempPath = path.string() + ".tmp";

        {
            std::ofstream out(tempPath, std::ios::trunc);
            if (!out.is_open())
                return false;

            out << root.dump(2);
            if (!out.good())
                return false;
        }

        std::error_code ec;
        std::filesystem::rename(tempPath, path, ec);
        if (!ec)
            return true;

        // On Windows, rename can fail when the destination exists. Remove+rename is
        // not a perfect atomic replace, but it avoids writing a half JSON file in place.
        std::filesystem::remove(path, ec);
        ec.clear();

        std::filesystem::rename(tempPath, path, ec);
        if (!ec)
            return true;

        std::filesystem::remove(tempPath);
        return false;
    }
}

#endif
