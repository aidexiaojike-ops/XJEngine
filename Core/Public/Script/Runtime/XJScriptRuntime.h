#ifndef XJ_SCRIPT_RUNTIME_H
#define XJ_SCRIPT_RUNTIME_H

#include "Script/Bytecode/XJScriptBytecode.h"
#include "Script/Runtime/XJScriptNativeInvoker.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace XJ
{
    class XJScriptRuntime;
    class XJScriptMachine;

    struct XJScriptExecutionLimits
    {
        // 单次 Execute 最多执行的指令数量。
        uint64_t InstructionBudget = 100000;

        // 包含根函数 Frame。
        uint32_t MaxCallDepth = 64;
    };

    enum class XJScriptRuntimeErrorCode
    {
        None = 0,

        InvalidModule,
        InvalidFunction,
        InvalidArguments,
        InvalidBytecode,

        StackUnderflow,
        StackOverflow,
        TypeMismatch,

        IntegerOverflow,
        DivideByZero,
        NonFiniteFloat,

        InstructionBudgetExceeded,
        CallDepthExceeded,

        NativeInvocationFailed,

        InstanceFaulted,
        ReentrantExecution
    };

    struct XJScriptStackTraceEntry
    {
        uint32_t FunctionIndex = 0;
        uint32_t InstructionIndex = 0;

        uint32_t Line = 1;
        uint32_t Column = 1;

        std::string FunctionName;
    };


    struct XJScriptRuntimeError
    {
        XJScriptRuntimeErrorCode Code = XJScriptRuntimeErrorCode::None;

        uint32_t FunctionIndex = 0;
        uint32_t InstructionIndex = 0;

        uint32_t Line = 1;
        uint32_t Column = 1;

        std::string Message;

        std::optional<XJScriptNativeErrorCode> NativeErrorCode;

        std::vector<XJScriptStackTraceEntry> StackTrace;
    };


    class XJScriptInstance
    {
    public:
        const std::shared_ptr<const XJScriptBytecodeModule>& GetModule() const { return mModule; }
        const std::vector<XJScriptValue>& GetFields() const { return mFields; }
        const XJScriptValue* GetField(uint32_t index) const
        {
            return index < mFields.size() ? &mFields[index] : nullptr;
        }
        bool SetField(uint32_t index, XJScriptValue value);
        bool IsFaulted() const { return mFaulted; }
        bool IsExecuting() const { return mExecuting; }
        const std::optional<XJScriptRuntimeError>& GetLastError() const { return mLastError; }

    private:
        friend class XJScriptRuntime;
        friend class XJScriptMachine;

        // Module 不可变，可以被多个实体实例共享。
        std::shared_ptr<const XJScriptBytecodeModule> mModule;

        // 每个实例独立持有字段状态。
        std::vector<XJScriptValue> mFields;

        // InitializeInstance 重新验证模块后保存，运行时不依赖可伪造的缓存值。
        std::vector<uint32_t> mFunctionMaxStackDepths;

        // 发生运行时错误后停止调度该实例。
        bool mFaulted = false;

        // 防止同一实例递归进入 Execute。
        bool mExecuting = false;

        std::optional<XJScriptRuntimeError> mLastError;
    };

    struct XJScriptExecutionResult
    {
        // void 方法保持为空。
        std::optional<XJScriptValue>
            ReturnValue;

        std::optional<
            XJScriptRuntimeError>
            Error;

        uint64_t ExecutedInstructions = 0;

        bool IsValid() const
        {
            return !Error.has_value();
        }
    };

    class XJScriptRuntime
    {
        public:
            // 验证 Module，并复制字段默认值到实例。
            // 成功返回 nullopt，失败返回结构化错误。
            static std::optional<
                XJScriptRuntimeError>
            InitializeInstance(
                XJScriptInstance& instance,
                std::shared_ptr<
                    const XJScriptBytecodeModule>
                    module);
            
            // 从指定函数开始执行。
            static XJScriptExecutionResult Execute(
                XJScriptInstance& instance,
                uint32_t functionIndex,
                std::span<
                    const XJScriptValue> arguments,
                XJScriptNativeInvoker& nativeInvoker,
                XJScriptNativeContext& nativeContext,
                XJScriptExecutionLimits limits = {});
            
            // 清除 Faulted 和 LastError。
            // 不重置字段值。
            static void ClearFault(
                XJScriptInstance& instance)
            {
                instance.mFaulted = false;
                instance.mLastError.reset();
            }
    };
}

#endif
