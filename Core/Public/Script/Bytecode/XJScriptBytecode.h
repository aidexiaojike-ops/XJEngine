#ifndef XJ_SCRIPT_BYTECODE_H
#define XJ_SCRIPT_BYTECODE_H

#include "Script/Language/XJScriptAst.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>
//字节码定义
namespace XJ
{
    enum class XJScriptOpCode : uint8_t
    {
        Nop,
        PushConstant,//字面量 
        LoadField, StoreField,//字段读取 
        LoadArgument, StoreArgument,//参数读取 
        LoadLocal, StoreLocal,//参数读取 
        //前置 ++target 
        Dup, Pop,//target = value 
        IntToFloat,//int→float 

        INegate, FNegate, LogicalNot,

        IAdd, ISubtract, IMultiply, IDivide,
        FAdd, FSubtract, FMultiply, FDivide,
        StringConcat,

        IEqual, FEqual, BoolEqual, StringEqual,
        INotEqual, FNotEqual, BoolNotEqual, StringNotEqual,

        ILess, ILessEqual, IGreater, IGreaterEqual,
        FLess, FLessEqual, FGreater, FGreaterEqual,

        Jump,
        JumpIfFalse,
        JumpIfTrue,

        CallScript,
        CallNative,
        //return; 
        ReturnVoid,
        ReturnValue,
        Count
    };

    enum class XJScriptNativeFunctionId : int32_t
    {
        TransformRotateY = 0
    };

    struct XJScriptStackEffect
    {
        int32_t Required = 0;
        int32_t Delta = 0;
        bool Dynamic = false;
    };

    // VM 必须遵守此栈契约。条件跳转会消费条件值；Store 会消费写入值。
    // Call 的效果取决于参数数量和返回类型，由 XJGetCallStackEffect 计算。
    inline XJScriptStackEffect XJGetStackEffect(XJScriptOpCode op)
    {
        switch (op)
        {
            case XJScriptOpCode::Nop:
                return {0, 0, false};

            case XJScriptOpCode::PushConstant:
            case XJScriptOpCode::LoadField:
            case XJScriptOpCode::LoadArgument:
            case XJScriptOpCode::LoadLocal:
                return {0, 1, false};

            case XJScriptOpCode::StoreField:
            case XJScriptOpCode::StoreArgument:
            case XJScriptOpCode::StoreLocal:
            case XJScriptOpCode::Pop:
            case XJScriptOpCode::JumpIfFalse:
            case XJScriptOpCode::JumpIfTrue:
            case XJScriptOpCode::ReturnValue:
                return {1, -1, false};

            case XJScriptOpCode::Dup:
                return {1, 1, false};

            case XJScriptOpCode::IntToFloat:
            case XJScriptOpCode::INegate:
            case XJScriptOpCode::FNegate:
            case XJScriptOpCode::LogicalNot:
                return {1, 0, false};

            case XJScriptOpCode::IAdd:
            case XJScriptOpCode::ISubtract:
            case XJScriptOpCode::IMultiply:
            case XJScriptOpCode::IDivide:
            case XJScriptOpCode::FAdd:
            case XJScriptOpCode::FSubtract:
            case XJScriptOpCode::FMultiply:
            case XJScriptOpCode::FDivide:
            case XJScriptOpCode::StringConcat:
            case XJScriptOpCode::IEqual:
            case XJScriptOpCode::FEqual:
            case XJScriptOpCode::BoolEqual:
            case XJScriptOpCode::StringEqual:
            case XJScriptOpCode::INotEqual:
            case XJScriptOpCode::FNotEqual:
            case XJScriptOpCode::BoolNotEqual:
            case XJScriptOpCode::StringNotEqual:
            case XJScriptOpCode::ILess:
            case XJScriptOpCode::ILessEqual:
            case XJScriptOpCode::IGreater:
            case XJScriptOpCode::IGreaterEqual:
            case XJScriptOpCode::FLess:
            case XJScriptOpCode::FLessEqual:
            case XJScriptOpCode::FGreater:
            case XJScriptOpCode::FGreaterEqual:
                return {2, -1, false};

            case XJScriptOpCode::CallScript:
            case XJScriptOpCode::CallNative:
                return {0, 0, true};

            case XJScriptOpCode::Jump:
            case XJScriptOpCode::ReturnVoid:
                return {0, 0, false};

            case XJScriptOpCode::Count:
                break;
        }
        return {};
    }

    inline XJScriptStackEffect XJGetCallStackEffect(uint32_t argumentCount, bool returnsValue)
    {
        return {
            static_cast<int32_t>(argumentCount),
            -static_cast<int32_t>(argumentCount) + (returnsValue ? 1 : 0),
            false
        };
    }

    using XJScriptConstant = std::variant<bool, int64_t, double, std::string>;
    using XJScriptValue = XJScriptConstant;

    struct XJScriptInstruction
    {
        XJScriptOpCode Op = XJScriptOpCode::Pop;
        // 操作数含义由 Op 决定：
        // slot、constant index、method index 或 jump target。
        int32_t A = 0;
        int32_t B = 0;

        uint32_t Line = 1;
        uint32_t Column = 1;
    };

    struct XJScriptBytecodeField//文件
    {
        std::string Name;
        uint64_t StableId = 0;
        XJScriptValueType Type;
        XJScriptAccess Access;
        XJScriptConstant DefaultValue;
    };

    struct XJScriptBytecodeFunction//函数
    {
        std::string Name;
        XJScriptValueType ReturnType;
        std::vector<XJScriptValueType> ParameterTypes;
        std::vector<XJScriptValueType> LocalTypes;
        uint32_t LocalCount = 0;
        uint32_t MaxStackDepth = 0;
        std::vector<XJScriptInstruction> Code;
    };

    struct XJScriptBytecodeModule//模型
    {
        std::string ClassName;
        std::vector<XJScriptConstant> Constants;
        std::vector<XJScriptBytecodeField> Fields;
        std::vector<XJScriptBytecodeFunction> Functions;

        std::optional<uint32_t> OnCreate;
        std::optional<uint32_t> OnUpdate;
        std::optional<uint32_t> OnFixedUpdate;
        std::optional<uint32_t> OnDestroy;
    };
}

#endif
