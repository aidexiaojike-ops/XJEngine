#include "Script/Native/XJScriptNativeCatalogue.h"

#include <spdlog/spdlog.h>
#include <array>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    using Type = XJ::XJScriptValueType;
    using Id = XJ::XJScriptNativeFunctionId;

    XJ::XJScriptValue vector = XJ::XJScriptVec3{1.0, 2.0, 3.0};

    Check(XJ::XJGetScriptValueType(vector) == Type::Vec3,
          "Vec3 has an explicit runtime type");

    Check(XJ::XJIsWellFormedScriptValue(vector),
          "finite Vec3 is accepted");

    vector = XJ::XJScriptVec3{
        std::numeric_limits<double>::infinity(), 0.0, 0.0};

    Check(!XJ::XJIsWellFormedScriptValue(vector),
          "non-finite Vec3 is rejected");

    const XJ::XJScriptEntityRef nullRef;
    const XJ::XJScriptEntityRef validShape{7, 0, 1};
    const XJ::XJScriptEntityRef malformed{0, 5, 1};

    Check(nullRef.IsNull() && nullRef.IsWellFormed(),
          "null entity reference has a defined representation");
    Check(validShape.IsWellFormed() && !validShape.IsNull(),
          "index zero is valid with a scope and generation");
    Check(!malformed.IsWellFormed(),
          "partial entity reference is rejected");

    std::string error;
    auto catalogue = XJ::XJBuildInitialNativeCatalogue(error);

    Check(catalogue.has_value() && error.empty(),
          "initial catalogue builds");

    const std::array floatArgument{Type::Float};
    const std::array intArgument{Type::Int};
    const std::array vectorArguments{
        Type::Float, Type::Float, Type::Float};

    Check(catalogue->Resolve(
              "Transform", "RotateY", floatArgument) != nullptr,
          "existing native signature is preserved");

    Check(catalogue->Resolve(
              "Transform", "RotateY", intArgument) == nullptr,
          "native calls use exact type matching");

    const auto* constructor =
        catalogue->Resolve("", "Vec3", vectorArguments);

    Check(constructor &&
          constructor->Id == Id::Vec3Create &&
          constructor->ReturnType == Type::Vec3,
          "constructor exposes a typed return value");

    auto invalid = XJ::XJScriptNativeCatalogue::Build(
    {
        {Id::TransformRotateY, "Transform", "RotateY",
         Type::Void, {Type::Float}},
        {Id::TransformRotateY, "Other", "Call",
         Type::Void, {}}
    }, error);

    Check(!invalid && !error.empty(),
          "duplicate native IDs are rejected");

    invalid = XJ::XJScriptNativeCatalogue::Build(
    {
        {Id::TransformRotateY, "Test", "Call",
         Type::Void, {Type::Float}},
        {Id::Vec3Create, "Test", "Call",
         Type::Float, {Type::Float}}
    }, error);

    Check(!invalid && !error.empty(),
          "return type alone cannot create an overload");

    spdlog::info("Script native contract tests passed.");
    return EXIT_SUCCESS;
}