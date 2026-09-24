#include "Script/Language/XJScriptParser.h"
#include "Script/Language/XJScriptSemanticAnalyzer.h"

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

    bool AnalyzeIsValid(std::string_view source)
    {
        auto parsed = XJ::XJScriptParser::Parse(source);
        Check(parsed.IsValid(), "semantic test source must parse");
        return XJ::XJScriptSemanticAnalyzer::Analyze(*parsed.Class).IsValid();
    }
}

int main()
{
    auto parsed = XJ::XJScriptParser::Parse(
        "class Behaviour : ScriptBehaviour {\n"
        "private: float speed = 1; int count = 1;\n"
        "public:\n"
        "  void Move(int value) { count += value; }\n"
        "  void Move(float value) { speed += value; }\n"
        "  float Promote(int value) { return value; }\n"
        "  void OnUpdate(float dt) {\n"
        "    float copied = count;\n"
        "    float mixed = count + dt;\n"
        "    Move(count); Move(dt); Transform.RotateY(dt);\n"
        "  }\n"
        "};");
    Check(parsed.IsValid(), "valid semantic source parses");

    const auto valid = XJ::XJScriptSemanticAnalyzer::Analyze(*parsed.Class);
    Check(valid.IsValid(), "valid semantic source analyzes");
    Check(valid.Conversions.size() >= 3, "int to float conversions recorded");
    Check(valid.Calls.size() == 3, "script and builtin calls resolved");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: "
        "void Move(float value) {} void Run() { Move(1); } };"),
        "overload calls require exact argument types");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: void Run() { "
        "int value = 1.0f; } };"),
        "float to int conversion rejected");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: void Run() { "
        "if (1) { return; } } };"),
        "if condition requires bool");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: void Run() { "
        "int value = missing; } };"),
        "unknown identifier rejected");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: int Select(bool condition) { "
        "if (condition) return 1; } };"),
        "non-void method requires all paths to return");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: void Run() { "
        "Transform.RotateY(1); } };"),
        "builtin calls require exact argument types");

    Check(AnalyzeIsValid(
        "class Valid : ScriptBehaviour { public: int Run() { "
        "while (true) { return 1; } } };"),
        "constant true loop with returning body satisfies return analysis");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: void Ping() {} "
        "void Run() { bool equal = Ping() == Ping(); } };"),
        "void expressions cannot be compared");

    Check(!AnalyzeIsValid(
        "class Bad : ScriptBehaviour { public: void Run() { "
        "int Transform = 0; Transform.RotateY(1.0f); } };"),
        "script variables cannot masquerade as Transform builtin");

    auto compoundParsed = XJ::XJScriptParser::Parse(
        "class Bad : ScriptBehaviour { public: void Run() { "
        "int target = 1; float value = 2.0f; target += value; } };");
    Check(compoundParsed.IsValid(), "invalid compound assignment source parses");
    const auto compound = XJ::XJScriptSemanticAnalyzer::Analyze(*compoundParsed.Class);
    Check(!compound.IsValid() && compound.Conversions.empty(),
          "rejected compound assignment records no conversion");

    std::cout << "XJScriptSemanticAnalyzer tests passed\n";
    return EXIT_SUCCESS;
}
