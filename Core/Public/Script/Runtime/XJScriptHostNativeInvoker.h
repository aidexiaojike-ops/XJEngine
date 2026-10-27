#ifndef XJ_SCRIPT_HOST_NATIVE_INVOKER_H
#define XJ_SCRIPT_HOST_NATIVE_INVOKER_H

#include "Script/Runtime/XJScriptNativeInvoker.h"
#include "Script/Host/XJScriptHostServices.h"

namespace XJ
{
    class XJScriptHostNativeInvoker final
        : public XJScriptNativeInvoker
    {
        public:
            explicit XJScriptHostNativeInvoker(
                XJScriptHostServices services);
            
            std::shared_ptr<const XJScriptNativeCatalogue>
            GetNativeCatalogue() const override;
            
            XJScriptNativeInvokeResult Invoke(
                XJScriptNativeFunctionId functionId,
                std::span<const XJScriptValue> arguments,
                XJScriptNativeContext& context) override;
            
        private:
            XJScriptNativeInvokeResult InvokeTransform(
                int operation,
                bool explicitEntity,
                std::span<const XJScriptValue> arguments,
                XJScriptNativeContext& context);
            
            XJScriptHostServices mServices;
    };
}

#endif