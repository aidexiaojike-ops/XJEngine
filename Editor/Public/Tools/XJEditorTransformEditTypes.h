#ifndef XJ_EDITOR_TRANSFORM_EDIT_TYPES_H
#define XJ_EDITOR_TRANSFORM_EDIT_TYPES_H

#include <cstdint>
#include <string>

/// 编辑器工具使用的变换字段。
    // 不依赖脚本层的 XJScriptTransformProperty。
namespace XJ
{
    enum class XJEditorTransformFields : uint8_t
    {
        None = 0,
        Position = 1,
        Rotation = 2,
        Scale = 4,
        All = 7
    };

    constexpr XJEditorTransformFields operator|(XJEditorTransformFields left,XJEditorTransformFields right)//重载位或运算符
    {
        return static_cast<XJEditorTransformFields>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
    }

    constexpr bool HasTransformField(XJEditorTransformFields fields,XJEditorTransformFields field)//检查是否包含指定的字段
    {
        return (static_cast<uint8_t>(fields) & static_cast<uint8_t>(field)) != 0;
    }

    constexpr bool IsValidTransformFields(XJEditorTransformFields fields)//检查是否是有效的变换字段
    {
        const auto bits = static_cast<uint8_t>(fields);
        return bits != 0 && (bits & static_cast<uint8_t>(0xF8)) == 0;
    }

    enum class XJEditorTransformEditOutcome//变换编辑结果
    {
        Applied,//已应用
        Unchanged,//没有变化
        Rejected//已拒绝
    };

    struct XJEditorTransformEditResult
    {
        XJEditorTransformEditOutcome Outcome = XJEditorTransformEditOutcome::Unchanged;
        std::string Error;

        bool Changed() const
        {
            return Outcome == XJEditorTransformEditOutcome::Applied;
        }

        bool Rejected() const
        {
            return Outcome == XJEditorTransformEditOutcome::Rejected;
        }
    };

}

#endif