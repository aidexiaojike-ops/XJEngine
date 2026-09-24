#include "Asset/Importer/XJScriptAssetCompiler.h"

#include <cstdlib>
#include <iostream>

namespace
{
    void Check(
        bool condition,
        const char* message)
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
    const auto valid =
        XJ::XJScriptAssetCompiler::
            CompileSource(
                "class Rotate "
                ": ScriptBehaviour {"
                "public:"
                "[[FieldId(\"1001\")]]"
                "float speed = 45.0f;"
                "void OnUpdate(float dt)"
                "{ Transform.RotateY("
                "speed * dt); }"
                "};",
                "Rotate.xjs");

    Check(
        valid &&
        valid->IsCompiled(),
        "valid script compiles");

    Check(
        valid->Module &&
        valid->Module->Fields.size() == 1,
        "public field compiled");

    Check(
        valid->Module
            ->Fields[0]
            .StableId == 1001,
        "stable FieldId preserved");

    const auto invalid =
        XJ::XJScriptAssetCompiler::
            CompileSource(
                "class Bad "
                ": ScriptBehaviour {"
                "public:"
                "float value = 1.0f;"
                "};",
                "Bad.xjs");

    Check(
        invalid &&
        !invalid->IsCompiled() &&
        !invalid->Diagnostics.empty(),
        "invalid script keeps diagnostics");

    std::cout
        << "XJScriptAssetCompiler "
           "tests passed\n";

    return EXIT_SUCCESS;
}