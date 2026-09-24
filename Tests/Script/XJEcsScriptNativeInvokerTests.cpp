#include "Script/Runtime/XJEcsScriptNativeInvoker.h"

#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    void Check(
        bool condition,
        std::string_view message)
    {
        if (!condition)
        {
            std::cerr
                << "FAILED: "
                << message
                << '\n';

            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    XJ::XJScene scene;
    XJ::XJEcsScriptNativeInvoker invoker;

    XJ::XJEntity* entity =
        scene.CreateEntityWithTransform(
            "ScriptOwner");

    Check(entity != nullptr,
          "owner entity created");

    XJ::XJScriptNativeContext context;
    context.Owner = entity;

    const XJ::XJScriptValue angle =
        15.0;

    const auto success = invoker.Invoke(
        XJ::XJScriptNativeFunctionId::
            TransformRotateY,
        std::span<
            const XJ::XJScriptValue>(
                &angle,
                1),
        context);

    Check(success.IsValid(),
          "RotateY succeeds");

    const auto& transform =
        entity->GetComponent<
            XJ::XJTransformComponent>();

    Check(
        std::abs(
            transform.rotation.y -
            15.0f) < 0.0001f,
        "RotateY changes rotation");

    const XJ::XJScriptValue wrongType =
        int64_t{15};

    const auto invalidArguments =
        invoker.Invoke(
            XJ::XJScriptNativeFunctionId::
                TransformRotateY,
            std::span<
                const XJ::XJScriptValue>(
                    &wrongType,
                    1),
            context);

    Check(
        !invalidArguments.IsValid() &&
        invalidArguments.ErrorCode ==
            XJ::XJScriptNativeErrorCode::
                InvalidArguments,
        "integer argument rejected");

    XJ::XJEntity* noTransform =
        scene.CreateEntity(
            "NoTransform");

    context.Owner = noTransform;

    const auto missingTransform =
        invoker.Invoke(
            XJ::XJScriptNativeFunctionId::
                TransformRotateY,
            std::span<
                const XJ::XJScriptValue>(
                    &angle,
                    1),
            context);

    Check(
        !missingTransform.IsValid() &&
        missingTransform.ErrorCode ==
            XJ::XJScriptNativeErrorCode::
                MissingTransform,
        "missing Transform rejected");

    context.Owner = nullptr;

    const auto missingEntity =
        invoker.Invoke(
            XJ::XJScriptNativeFunctionId::
                TransformRotateY,
            std::span<
                const XJ::XJScriptValue>(
                    &angle,
                    1),
            context);

    Check(
        !missingEntity.IsValid() &&
        missingEntity.ErrorCode ==
            XJ::XJScriptNativeErrorCode::
                MissingEntity,
        "missing owner rejected");

    context.Owner = entity;

    const XJ::XJScriptValue infinity =
        std::numeric_limits<
            double>::infinity();

    const auto nonFinite =
        invoker.Invoke(
            XJ::XJScriptNativeFunctionId::
                TransformRotateY,
            std::span<
                const XJ::XJScriptValue>(
                    &infinity,
                    1),
            context);

    Check(
        !nonFinite.IsValid() &&
        nonFinite.ErrorCode ==
            XJ::XJScriptNativeErrorCode::
                NonFiniteInput,
        "non-finite angle rejected");

    std::cout
        << "XJEcsScriptNativeInvoker "
           "tests passed\n";

    return EXIT_SUCCESS;
}