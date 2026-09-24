#include "Script/Runtime/XJEcsScriptNativeInvoker.h"

#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"

#include <cmath>
#include <limits>

namespace XJ
{
    XJScriptNativeInvokeResult
    XJEcsScriptNativeInvoker::Invoke(
        XJScriptNativeFunctionId functionId,
        std::span<
            const XJScriptValue> arguments,
        XJScriptNativeContext& context)
    {
        switch (functionId)
        {
            case XJScriptNativeFunctionId::
                TransformRotateY:
            {
                return InvokeTransformRotateY(
                    arguments,
                    context);
            }
        }

        return XJScriptNativeInvokeResult::Failure(
            XJScriptNativeErrorCode::
                UnsupportedFunction,
            "Unsupported native script function");
    }

    XJScriptNativeInvokeResult
    XJEcsScriptNativeInvoker::
        InvokeTransformRotateY(
            std::span<
                const XJScriptValue> arguments,
            XJScriptNativeContext& context)
    {
        if (!XJEntity::IsValid(context.Owner))
        {
            return XJScriptNativeInvokeResult::Failure(
                XJScriptNativeErrorCode::
                    MissingEntity,
                "Script owner entity is invalid");
        }

        if (!context.Owner
                ->HasComponent<
                    XJTransformComponent>())
        {
            return XJScriptNativeInvokeResult::Failure(
                XJScriptNativeErrorCode::
                    MissingTransform,
                "Script owner has no Transform");
        }

        if (arguments.size() != 1 ||
            !std::holds_alternative<double>(
                arguments[0]))
        {
            return XJScriptNativeInvokeResult::Failure(
                XJScriptNativeErrorCode::
                    InvalidArguments,
                "Transform.RotateY requires "
                "one float argument");
        }

        const double deltaDegrees =
            std::get<double>(arguments[0]);

        if (!std::isfinite(deltaDegrees))
        {
            return XJScriptNativeInvokeResult::Failure(
                XJScriptNativeErrorCode::
                    NonFiniteInput,
                "Transform.RotateY argument "
                "must be finite");
        }

        auto& transform =
            context.Owner->GetComponent<
                XJTransformComponent>();

        if (!std::isfinite(
                static_cast<double>(
                    transform.rotation.y)))
        {
            return XJScriptNativeInvokeResult::Failure(
                XJScriptNativeErrorCode::
                    NonFiniteInput,
                "Transform rotation is not finite");
        }

        const double result =
            static_cast<double>(
                transform.rotation.y) +
            deltaDegrees;

        constexpr double maxFloat =
            static_cast<double>(
                std::numeric_limits<float>::max());

        if (!std::isfinite(result) ||
            result > maxFloat ||
            result < -maxFloat)
        {
            return XJScriptNativeInvokeResult::Failure(
                XJScriptNativeErrorCode::
                    NonFiniteResult,
                "Transform.RotateY result "
                "is outside float range");
        }

        transform.rotation.y =
            static_cast<float>(result);

        // Transform 数据改变后必须同步 model matrix。
        transform.UpdateModelMatrix();

        return
            XJScriptNativeInvokeResult::Success();
    }
}