#ifndef XJ_SCRIPT_VALUE_NATIVE_INVOKER_H
#define XJ_SCRIPT_VALUE_NATIVE_INVOKER_H

#include "Script/Runtime/XJScriptNativeInvoker.h"
//纯值类型 Native 适配器
namespace XJ
{
    class XJScriptValueNativeInvoker final : public XJScriptNativeInvoker
    {
        public:
            explicit XJScriptValueNativeInvoker(XJScriptNativeInvoker& host)
                : mHost(host)
            {
            }

            std::shared_ptr<const XJScriptNativeCatalogue>
            GetNativeCatalogue() const override
            {
                return mHost.GetNativeCatalogue();
            }

            XJScriptNativeInvokeResult Invoke(
                XJScriptNativeFunctionId functionId,
                std::span<const XJScriptValue> arguments,
                XJScriptNativeContext& context) override
            {
                if (functionId != XJScriptNativeFunctionId::Vec3Create)
                    return mHost.Invoke(functionId, arguments, context);

                if (arguments.size() != 3)
                {
                    return XJScriptNativeInvokeResult::Failure(
                        XJScriptNativeErrorCode::InvalidArguments,
                        "Vec3 requires three float arguments");
                }

                for (const auto& value : arguments)
                {
                    if (!std::holds_alternative<double>(value) ||
                        !XJIsWellFormedScriptValue(value))
                    {
                        return XJScriptNativeInvokeResult::Failure(
                            XJScriptNativeErrorCode::InvalidArguments,
                            "Vec3 requires finite float arguments");
                    }
                }

                return XJScriptNativeInvokeResult::Success(
                    XJScriptVec3{
                        std::get<double>(arguments[0]),
                        std::get<double>(arguments[1]),
                        std::get<double>(arguments[2])
                    });
            }

        private:
            XJScriptNativeInvoker& mHost;
    };
}

#endif