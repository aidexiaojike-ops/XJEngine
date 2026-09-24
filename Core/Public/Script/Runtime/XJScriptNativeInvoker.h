#ifndef XJ_SCRIPT_NATIVE_INVOKER_H
#define XJ_SCRIPT_NATIVE_INVOKER_H

#include "Script/Bytecode/XJScriptBytecode.h"

#include <optional>
#include <span>
#include <string>
#include <utility>

namespace XJ
{
    class XJEntity;

    // Native 调用时由 ScriptSystem 提供的引擎上下文。
    // VM 本身不直接依赖 ECS。
    struct XJScriptNativeContext
    {
        XJEntity* Owner = nullptr;
    };

    enum class XJScriptNativeErrorCode
    {
        None = 0,

        InvalidArguments,
        UnsupportedFunction,

        MissingEntity,
        MissingTransform,

        NonFiniteInput,
        NonFiniteResult
    };

    struct XJScriptNativeInvokeResult
    {
        // void Native 保持为空；有返回值的 Native 写入这里。
        std::optional<XJScriptValue>
            ReturnValue;

        XJScriptNativeErrorCode ErrorCode = XJScriptNativeErrorCode::None;

        std::string ErrorMessage;

        bool IsValid() const
        {
            return ErrorCode == XJScriptNativeErrorCode::None;
        }

        static XJScriptNativeInvokeResult
        Success()
        {
            return {};
        }

        static XJScriptNativeInvokeResult
        Success(XJScriptValue value)
        {
            XJScriptNativeInvokeResult result;
            result.ReturnValue = std::move(value);
            return result;
        }

        static XJScriptNativeInvokeResult
        Failure(XJScriptNativeErrorCode code, std::string message)
        {
            XJScriptNativeInvokeResult result;
            result.ErrorCode = code;
            result.ErrorMessage = std::move(message);
            return result;
        }

    };

    class XJScriptNativeInvoker
    {
        public:
            virtual ~XJScriptNativeInvoker() = default;

            XJScriptNativeInvoker(const XJScriptNativeInvoker&) = delete;

            XJScriptNativeInvoker& operator=(const XJScriptNativeInvoker&) = delete;

            virtual XJScriptNativeInvokeResult
            Invoke(XJScriptNativeFunctionId functionId,
                std::span<
                    const XJScriptValue> arguments,
                XJScriptNativeContext& context) = 0;

        protected:
            XJScriptNativeInvoker() = default;
    };
}

#endif