#ifndef XJ_SCRIPT_NATIVE_INVOKER_H
#define XJ_SCRIPT_NATIVE_INVOKER_H

#include "Script/Bytecode/XJScriptBytecode.h"
#include "Script/Native/XJScriptNativeCatalogue.h"
#include "Script/Host/XJScriptHostServices.h"

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
        // 暂时保留，兼容现有宿主和测试。
        XJEntity* Owner = nullptr;

        // 新 Dispatcher 使用这个上下文，不从 VM 访问 ECS。
        XJScriptCallContext Call;
    };

    enum class XJScriptNativeErrorCode
    {
        None = 0,

        InvalidArguments,
        UnsupportedFunction,

        MissingEntity,
        MissingTransform,

        NonFiniteInput,
        NonFiniteResult,

        HostFailure
    };

    struct XJScriptNativeInvokeResult//脚本原生调用结果
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

        static XJScriptNativeInvokeResult Success()
        {
            return {};
        }

        static XJScriptNativeInvokeResult Success(XJScriptValue value)//成功
        {
            XJScriptNativeInvokeResult result;
            result.ReturnValue = std::move(value);
            return result;
        }

        static XJScriptNativeInvokeResult Failure(XJScriptNativeErrorCode code, std::string message)//失败
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

            virtual std::shared_ptr<const XJScriptNativeCatalogue>
            GetNativeCatalogue() const
            {
                return XJGetInitialNativeCatalogue();
            }

        protected:
            XJScriptNativeInvoker() = default;
    };
}

#endif
