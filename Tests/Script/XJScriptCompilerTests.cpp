#include "Script/Compiler/XJScriptCompiler.h"
#include "Script/Language/XJScriptParser.h"
#include "Script/Language/XJScriptSemanticAnalyzer.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    bool Contains(const XJ::XJScriptBytecodeFunction& function, XJ::XJScriptOpCode op)
    {
        return std::any_of(function.Code.begin(), function.Code.end(),
            [op](const XJ::XJScriptInstruction& instruction) { return instruction.Op == op; });
    }

    const XJ::XJScriptBytecodeFunction* FindFunction(
        const XJ::XJScriptBytecodeModule& module,
        std::string_view name,
        XJ::XJScriptValueType parameterType)
    {
        for (const auto& function : module.Functions)
        {
            if (function.Name == name && function.ParameterTypes.size() == 1 &&
                function.ParameterTypes[0] == parameterType)
                return &function;
        }
        return nullptr;
    }
}

int main()
{
    auto parsed = XJ::XJScriptParser::Parse(
        "class Compiled : ScriptBehaviour {\n"
        "private: int count = 1; float speed = 1;\n"
        "public:\n"
        "  int Add(int value) { return count + value; }\n"
        "  float Add(float value) { return speed + value; }\n"
        "  void OnUpdate(float dt) {\n"
        "    float mixed = count + dt;\n"
        "    count++; Add(count); Add(dt); Transform.RotateY(dt);\n"
        "    if (count < 10) { count += 1; } else { count -= 1; }\n"
        "    while (count < 12) ++count;\n"
        "  }\n"
        "};");
    Check(parsed.IsValid(), "compiler source parses");

    const auto semantic = XJ::XJScriptSemanticAnalyzer::Analyze(*parsed.Class);
    Check(semantic.IsValid(), "compiler source analyzes");

    const auto compiled = XJ::XJScriptCompiler::Compile(*parsed.Class, semantic);
    Check(compiled.IsValid(), "valid source compiles");
    const auto& module = *compiled.Module;

    Check(module.ClassName == "Compiled", "module class name");
    Check(module.Fields.size() == 2, "compiled field count");
    Check(module.Fields[0].StableId == 0, "private field needs no stable ID");
    Check(std::get<double>(module.Fields[1].DefaultValue) == 1.0,
          "float field keeps promoted default");
    Check(module.Functions.size() == 3, "compiled function count");
    Check(module.OnUpdate && *module.OnUpdate == 2, "lifecycle index");

    const auto* addInt = FindFunction(module, "Add", XJ::XJScriptValueType::Int);
    const auto* addFloat = FindFunction(module, "Add", XJ::XJScriptValueType::Float);
    Check(addInt && Contains(*addInt, XJ::XJScriptOpCode::IAdd), "int overload uses IAdd");
    Check(addFloat && Contains(*addFloat, XJ::XJScriptOpCode::FAdd), "float overload uses FAdd");

    const auto& update = module.Functions[*module.OnUpdate];
    Check(update.LocalCount == 1, "local slot count");
    Check(update.MaxStackDepth > 0, "verified maximum stack depth stored");
    Check(Contains(update, XJ::XJScriptOpCode::IntToFloat), "promotion instruction emitted");
    Check(Contains(update, XJ::XJScriptOpCode::StoreLocal), "local store emitted");
    Check(Contains(update, XJ::XJScriptOpCode::CallScript), "script call emitted");
    Check(Contains(update, XJ::XJScriptOpCode::CallNative), "native call emitted");
    Check(Contains(update, XJ::XJScriptOpCode::JumpIfFalse), "branch instruction emitted");
    Check(Contains(update, XJ::XJScriptOpCode::Jump), "loop/back-edge instruction emitted");
    Check(update.Code.back().Op == XJ::XJScriptOpCode::ReturnVoid,
          "void method receives implicit return");

    const auto conditionalEffect = XJ::XJGetStackEffect(XJ::XJScriptOpCode::JumpIfFalse);
    Check(conditionalEffect.Required == 1 && conditionalEffect.Delta == -1,
          "conditional jumps consume their condition");
    const auto storeEffect = XJ::XJGetStackEffect(XJ::XJScriptOpCode::StoreLocal);
    Check(storeEffect.Required == 1 && storeEffect.Delta == -1,
          "store consumes its value");
    const auto binaryEffect = XJ::XJGetStackEffect(XJ::XJScriptOpCode::FAdd);
    Check(binaryEffect.Required == 2 && binaryEffect.Delta == -1,
          "binary operation consumes two and produces one");
    const auto callEffect = XJ::XJGetCallStackEffect(2, true);
    Check(callEffect.Required == 2 && callEffect.Delta == -1,
          "call effect accounts for arguments and return value");

    auto badParsed = XJ::XJScriptParser::Parse(
        "class Bad : ScriptBehaviour { public: void Move(float value) {} "
        "void Run() { Move(1); } };");
    Check(badParsed.IsValid(), "bad semantic source still parses");
    const auto badSemantic = XJ::XJScriptSemanticAnalyzer::Analyze(*badParsed.Class);
    Check(!badSemantic.IsValid(), "bad overload rejected by semantic analyzer");
    const auto badCompile = XJ::XJScriptCompiler::Compile(*badParsed.Class, badSemantic);
    Check(!badCompile.IsValid() && !badCompile.Module,
          "compiler rejects invalid semantic input");

    std::cout << "XJScriptCompiler tests passed\n";
    return EXIT_SUCCESS;
}
