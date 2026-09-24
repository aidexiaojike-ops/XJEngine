#include "Script/Language/XJScriptParser.h"

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
}

int main()
{
    const auto valid = XJ::XJScriptParser::Parse(
        "class Rotate : ScriptBehaviour {\n"
        "public:\n"
        "  [[FieldId(\"1001\")]] float speed = 45.0f;\n"
        "  [[FieldId(\"1002\")]] bool enabled = true;\n"
        "  [[FieldId(\"1003\")]] string message = \"hello\\nworld\";\n"
        "  void OnCreate() {}\n"
        "  void OnUpdate(float dt) {\n"
        "    float amount = speed * dt;\n"
        "    if (enabled && amount > 0.0f) {\n"
        "      Transform.RotateY(amount);\n"
        "      counter++;\n"
        "    } else { counter -= 1; }\n"
        "    while (counter < 10) ++counter;\n"
        "    return;\n"
        "  }\n"
        "  void OnFixedUpdate(float dt) {}\n"
        "  void OnDestroy() {}\n"
        "private:\n"
        "  int counter = -2;\n"
        "};");

    Check(valid.IsValid(), "valid ScriptBehaviour class parses");
    Check(valid.Class->Name == "Rotate", "class name");
    Check(valid.Class->BaseName == "ScriptBehaviour", "base name");
    Check(valid.Class->Fields.size() == 4, "field count");
    Check(valid.Class->Methods.size() == 4, "method count");
    Check(valid.Class->Fields[0].StableId == 1001, "first FieldId parsed");
    Check(valid.Class->Fields[1].StableId == 1002, "second FieldId parsed");
    Check(std::get<double>(*valid.Class->Fields[0].DefaultValue) == 45.0,
          "float default value");
    Check(std::get<bool>(*valid.Class->Fields[1].DefaultValue),
          "bool default value");
    Check(std::get<std::string>(*valid.Class->Fields[2].DefaultValue) == "hello\nworld",
          "decoded string default value");
    Check(std::get<int64_t>(*valid.Class->Fields[3].DefaultValue) == -2,
          "negative integer default value");
    const auto* updateBody = valid.Class->Methods[1].Body.get();
    Check(updateBody != nullptr, "method body parsed");
    Check(updateBody->Statements.size() == 4, "method statement count");

    const auto* variable = dynamic_cast<const XJ::XJScriptVariableStmt*>(
        updateBody->Statements[0].get());
    Check(variable != nullptr && variable->Name == "amount", "local variable parsed");
    const auto* multiply = dynamic_cast<const XJ::XJScriptBinaryExpr*>(
        variable->Initializer.get());
    Check(multiply != nullptr && multiply->Op == XJ::XJScriptBinaryOp::Multiply,
          "multiplication expression parsed");

    const auto* ifStatement = dynamic_cast<const XJ::XJScriptIfStmt*>(
        updateBody->Statements[1].get());
    Check(ifStatement != nullptr && ifStatement->ElseBranch != nullptr,
          "if else parsed");

    const auto* whileStatement = dynamic_cast<const XJ::XJScriptWhileStmt*>(
        updateBody->Statements[2].get());
    Check(whileStatement != nullptr, "while parsed");

    const auto* returnStatement = dynamic_cast<const XJ::XJScriptReturnStmt*>(
        updateBody->Statements[3].get());
    Check(returnStatement != nullptr && returnStatement->Value == nullptr,
          "empty return parsed");

    Check(!XJ::XJScriptParser::Parse(
        "class Bad : OtherBase { public: void OnCreate() {} };").IsValid(),
        "invalid base rejected");

    Check(!XJ::XJScriptParser::Parse(
        "class Bad : ScriptBehaviour { int value; int value; };").IsValid(),
        "duplicate field rejected");

    Check(!XJ::XJScriptParser::Parse(
        "class Bad : ScriptBehaviour { public: int OnUpdate(int dt) {} };").IsValid(),
        "invalid lifecycle signature rejected");

    const auto lexerFailure = XJ::XJScriptParser::Parse(
        "class Bad : ScriptBehaviour { public: float value = 12abc; };");
    Check(!lexerFailure.IsValid() && !lexerFailure.Diagnostics.empty(),
          "lexer diagnostics propagate through parser");

    Check(!XJ::XJScriptParser::Parse(
        "class Bad : ScriptBehaviour { public: void OnUpdate(float dt) "
        "{ (dt + 1.0f) = 2.0f; } };").IsValid(),
        "invalid assignment target rejected");

    Check(XJ::XJScriptParser::Parse(
        "class Overload : ScriptBehaviour { public: "
        "void Move(int value) {} void Move(float value) {} };").IsValid(),
        "parameter type overloads accepted");

    Check(!XJ::XJScriptParser::Parse(
        "class Duplicate : ScriptBehaviour { public: "
        "void Move(int value) {} void Move(int other) {} };").IsValid(),
        "identical overload rejected");

    Check(!XJ::XJScriptParser::Parse(
        "class MissingInit : ScriptBehaviour { int field; "
        "public: void Run() { int local; } };").IsValid(),
        "fields and locals require initializers");

    Check(!XJ::XJScriptParser::Parse(
        "class MissingId : ScriptBehaviour { public: float value = 1.0f; };").IsValid(),
        "public field requires FieldId");

    Check(!XJ::XJScriptParser::Parse(
        "class DuplicateId : ScriptBehaviour { public: "
        "[[FieldId(\"1001\")]] float a = 1.0f; "
        "[[FieldId(\"1001\")]] float b = 2.0f; };").IsValid(),
        "duplicate FieldId rejected");

    Check(!XJ::XJScriptParser::Parse(
        "class MethodId : ScriptBehaviour { public: "
        "[[FieldId(\"1001\")]] void Run() {} };").IsValid(),
        "FieldId on method rejected");

    Check(XJ::XJScriptParser::Parse(
        "class NoSemicolon : ScriptBehaviour { }").IsValid(),
        "top-level class semicolon is optional");

    std::cout << "XJScriptParser tests passed\n";
    return EXIT_SUCCESS;
}
