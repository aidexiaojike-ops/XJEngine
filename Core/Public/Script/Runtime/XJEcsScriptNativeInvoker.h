#ifndef XJ_ECS_SCRIPT_NATIVE_INVOKER_H
#define XJ_ECS_SCRIPT_NATIVE_INVOKER_H

#include "Script/Runtime/XJScriptNativeInvoker.h"
    //原生调用者
    
namespace XJ
{
    class XJEcsScriptNativeInvoker final
        : public XJScriptNativeInvoker
    {
        public:
            XJEcsScriptNativeInvoker() = default;
            ~XJEcsScriptNativeInvoker() override =
                default;

            XJScriptNativeInvokeResult Invoke(
                XJScriptNativeFunctionId functionId,
                std::span<
                    const XJScriptValue> arguments,
                XJScriptNativeContext& context)
                override;

        private:
            XJScriptNativeInvokeResult
            InvokeTransformRotateY(
                std::span<
                    const XJScriptValue> arguments,
                XJScriptNativeContext& context);
    };
}

#endif