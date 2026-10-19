#ifndef XJ_SCRIPT_VALUE_H
#define XJ_SCRIPT_VALUE_H

#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <optional>
#include <string_view>

////脚本参数
namespace XJ
{
    enum class XJScriptValueType : uint8_t//脚本参数类型
    {
        Void,
        Bool,
        Int,
        Float,
        String,
        Vec3,
        EntityRef
    };

    struct XJScriptVec3
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;

        bool IsFinite() const
        {
            return std::isfinite(X) &&
                   std::isfinite(Y) &&
                   std::isfinite(Z);
        }

        bool operator == (const XJScriptVec3&) const = default;
    };

    // RuntimeSession 的句柄，不是 UUID，也不是实体指针。
    struct XJScriptEntityRef
    {
        uint64_t ScopeId = 0;
        uint32_t Index = 0;
        uint32_t Generation = 0;
        
        bool IsNull() const
        {
            return ScopeId == 0 && Index == 0 && Generation == 0;
        }

        bool IsWellFormed() const
        {
            return IsNull() ||
                   (ScopeId != 0 && Generation != 0);
        }

        bool operator==(const XJScriptEntityRef&) const = default;
    };

    using XJScriptValue = std::variant<bool,
        int64_t,
        double,
        std::string,
        XJScriptVec3,
        XJScriptEntityRef>;

    using XJScriptConstant = XJScriptValue;

    template<typename>
    inline constexpr bool XJ_SCRIPT_UNSUPPORTED_VALUE = false;//脚本不支持的值



    inline XJScriptValueType XJGetScriptValueType(const XJScriptValue &value)
    {
        if (value.valueless_by_exception())
            return XJScriptValueType::Void;

        return std::visit([](const auto& item)
        {
            using T = std::decay_t<decltype(item)>;

            if constexpr (std::is_same_v<T, bool>)
                return XJScriptValueType::Bool;
            else if constexpr (std::is_same_v<T, int64_t>)
                return XJScriptValueType::Int;
            else if constexpr (std::is_same_v<T, double>)
                return XJScriptValueType::Float;
            else if constexpr (std::is_same_v<T, std::string>)
                return XJScriptValueType::String;
            else if constexpr (std::is_same_v<T, XJScriptVec3>)
                return XJScriptValueType::Vec3;
            else if constexpr (std::is_same_v<T, XJScriptEntityRef>)
                return XJScriptValueType::EntityRef;
            else
                static_assert(XJ_SCRIPT_UNSUPPORTED_VALUE<T>);
        }, value);
    }

    inline bool XJIsRuntimeScriptValueType(XJScriptValueType type)
    {
        switch (type)
        {
            case XJScriptValueType::Bool:
            case XJScriptValueType::Int:
            case XJScriptValueType::Float:
            case XJScriptValueType::String:
            case XJScriptValueType::Vec3:
            case XJScriptValueType::EntityRef:
                return true;

            default:
                return false;
        }
    }

    inline bool XJIsWellFormedScriptValue(const XJScriptValue& value)
    {
        if (value.valueless_by_exception())
            return false;
        if (const auto* number = std::get_if<double>(&value))
            return std::isfinite(*number);

        if (const auto* vector = std::get_if<XJScriptVec3>(&value))
            return vector->IsFinite();

        if (const auto* entity = std::get_if<XJScriptEntityRef>(&value))
            return entity->IsWellFormed();

        return true;
    }

    inline bool XJIsSerializableScriptValue(const XJScriptValue& value)
    {
        return XJIsWellFormedScriptValue(value) &&
               !std::holds_alternative<XJScriptEntityRef>(value);
    }

    inline bool XJIsScriptConstantValue(const XJScriptValue& value)
    {
       if (!XJIsWellFormedScriptValue(value))
           return false;

       const auto* reference = std::get_if<XJScriptEntityRef>(&value);

       // 非空 EntityRef 只能由运行时宿主创建，不能写入字节码常量。
       return !reference || reference->IsNull();
    }

    inline char XJGetScriptTypeCode(XJScriptValueType type)
    {
        switch (type)
        {
            case XJScriptValueType::Void:      return 'v';
            case XJScriptValueType::Bool:      return 'b';
            case XJScriptValueType::Int:       return 'i';
            case XJScriptValueType::Float:     return 'f';
            case XJScriptValueType::String:    return 's';
            case XJScriptValueType::Vec3:      return '3';
            case XJScriptValueType::EntityRef: return 'e';
        }

        return '?';
    }

    inline std::optional<uint32_t> XJGetVec3ComponentIndex(std::string_view name)
    {
        if (name == "x") return 0;
        if (name == "y") return 1;
        if (name == "z") return 2;
        return std::nullopt;
    }

    inline std::optional<double> XJReadVec3Component(
        const XJScriptVec3& value,
        uint32_t component)
    {
        switch (component)
        {
            case 0: return value.X;
            case 1: return value.Y;
            case 2: return value.Z;
            default: return std::nullopt;
        }
    }

    inline bool XJWriteVec3Component(
        XJScriptVec3& value,
        uint32_t component,
        double number)
    {
        if (!std::isfinite(number))
            return false;

        switch (component)
        {
            case 0: value.X = number; return true;
            case 1: value.Y = number; return true;
            case 2: value.Z = number; return true;
            default: return false;
        }
    }

}

#endif
