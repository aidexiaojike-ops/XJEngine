#include "Script/Compiler/XJScriptCompiler.h"
#include "Script/Language/XJScriptParser.h"
#include "Script/Language/XJScriptSemanticAnalyzer.h"
#include "Script/Runtime/XJScriptRuntime.h"

#include <cstdlib>
#include <limits>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

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

    std::shared_ptr<const XJ::XJScriptBytecodeModule> Compile(std::string_view source)
    {
        auto parsed = XJ::XJScriptParser::Parse(source);
        Check(parsed.IsValid(), "runtime source parses");
        const auto semantic = XJ::XJScriptSemanticAnalyzer::Analyze(*parsed.Class);
        Check(semantic.IsValid(), "runtime source analyzes");
        auto compiled = XJ::XJScriptCompiler::Compile(*parsed.Class, semantic);
        Check(compiled.IsValid(), "runtime source compiles");
        return std::make_shared<const XJ::XJScriptBytecodeModule>(std::move(*compiled.Module));
    }

    class FakeNativeInvoker final : public XJ::XJScriptNativeInvoker
    {
    public:
        XJ::XJScriptNativeInvokeResult Invoke(
            XJ::XJScriptNativeFunctionId id,
            std::span<const XJ::XJScriptValue> arguments,
            XJ::XJScriptNativeContext&) override
        {
            ++Calls;
            LastId = id;
            LastArguments.assign(arguments.begin(), arguments.end());
            if (InstanceToReinitialize && ModuleForReinitialize)
            {
                const auto error = XJ::XJScriptRuntime::InitializeInstance(
                    *InstanceToReinitialize, ModuleForReinitialize);
                ReinitializeError = error ? error->Code : XJ::XJScriptRuntimeErrorCode::None;
            }
            if (UnexpectedReturn)
                return XJ::XJScriptNativeInvokeResult::Success(XJ::XJScriptValue{1.0});
            return Failure
                ? XJ::XJScriptNativeInvokeResult::Failure(
                      XJ::XJScriptNativeErrorCode::MissingTransform, "missing transform")
                : XJ::XJScriptNativeInvokeResult::Success();
        }

        bool Failure = false;
        bool UnexpectedReturn = false;
        uint32_t Calls = 0;
        XJ::XJScriptNativeFunctionId LastId = XJ::XJScriptNativeFunctionId::TransformRotateY;
        std::vector<XJ::XJScriptValue> LastArguments;
        XJ::XJScriptInstance* InstanceToReinitialize = nullptr;
        std::shared_ptr<const XJ::XJScriptBytecodeModule> ModuleForReinitialize;
        XJ::XJScriptRuntimeErrorCode ReinitializeError = XJ::XJScriptRuntimeErrorCode::None;
    };
}

int main()
{
    const auto module = Compile(
        "class Runtime : ScriptBehaviour {\n"
        "private: int count = 1; float total = 0.0f;\n"
        "public:\n"
        "  int Add(int a, int b) { return a + b; }\n"
        "  void OnUpdate(float dt) {\n"
        "    count += 2; total = count + dt;\n"
        "    int sum = Add(count, 3);\n"
        "    if (sum > 0) total += 1.0f;\n"
        "    while (count < 5) ++count;\n"
        "    Transform.RotateY(dt);\n"
        "  }\n"
        "};");

    XJ::XJScriptInstance first;
    XJ::XJScriptInstance second;
    Check(!XJ::XJScriptRuntime::InitializeInstance(first, module), "first instance initializes");
    Check(!XJ::XJScriptRuntime::InitializeInstance(second, module), "second instance initializes");

    FakeNativeInvoker native;
    XJ::XJScriptNativeContext context;
    const XJ::XJScriptValue delta = 0.5;
    const auto executed = XJ::XJScriptRuntime::Execute(
        first, *module->OnUpdate, std::span<const XJ::XJScriptValue>(&delta, 1), native, context);
    Check(executed.IsValid(), "script executes");
    Check(first.GetField(0) && std::get<int64_t>(*first.GetField(0)) == 5,
          "loop and field writes execute");
    Check(first.GetField(1) && std::get<double>(*first.GetField(1)) == 4.5,
          "promotion and arithmetic execute");
    Check(second.GetField(0) && std::get<int64_t>(*second.GetField(0)) == 1,
          "instances keep isolated fields");
    Check(native.Calls == 1 && std::get<double>(native.LastArguments[0]) == 0.5,
          "native call receives argument");
    Check(!second.SetField(0, XJ::XJScriptValue{2.0}), "field setter rejects wrong type");
    Check(!second.SetField(1, XJ::XJScriptValue{std::numeric_limits<double>::infinity()}),
          "field setter rejects non-finite float");

    XJ::XJScriptInstance nonFinite;
    Check(!XJ::XJScriptRuntime::InitializeInstance(nonFinite, module),
          "non-finite argument instance initializes");
    const XJ::XJScriptValue infinity = std::numeric_limits<double>::infinity();
    const auto nonFiniteResult = XJ::XJScriptRuntime::Execute(
        nonFinite, *module->OnUpdate,
        std::span<const XJ::XJScriptValue>(&infinity, 1), native, context);
    Check(!nonFiniteResult.IsValid() &&
              nonFiniteResult.Error->Code == XJ::XJScriptRuntimeErrorCode::NonFiniteFloat,
          "non-finite host argument faults instance");

    XJ::XJScriptInstance reentrant;
    Check(!XJ::XJScriptRuntime::InitializeInstance(reentrant, module),
          "reentrant instance initializes");
    native.InstanceToReinitialize = &reentrant;
    native.ModuleForReinitialize = module;
    const auto reentrantResult = XJ::XJScriptRuntime::Execute(
        reentrant, *module->OnUpdate, std::span<const XJ::XJScriptValue>(&delta, 1),
        native, context);
    Check(reentrantResult.IsValid() &&
              native.ReinitializeError == XJ::XJScriptRuntimeErrorCode::ReentrantExecution,
          "initialization during execution is rejected without corrupting VM");
    native.InstanceToReinitialize = nullptr;
    native.ModuleForReinitialize.reset();

    XJ::XJScriptInstance nativeContract;
    Check(!XJ::XJScriptRuntime::InitializeInstance(nativeContract, module),
          "native contract instance initializes");
    native.UnexpectedReturn = true;
    const auto nativeContractResult = XJ::XJScriptRuntime::Execute(
        nativeContract, *module->OnUpdate,
        std::span<const XJ::XJScriptValue>(&delta, 1), native, context);
    Check(!nativeContractResult.IsValid() &&
              nativeContractResult.Error->Code == XJ::XJScriptRuntimeErrorCode::NativeInvocationFailed,
          "void native returning a value is rejected");
    native.UnexpectedReturn = false;

    const auto overflowModule = Compile(
        "class Overflow : ScriptBehaviour { private: int value = 9223372036854775807; "
        "public: void Run() { value += 1; } };");
    XJ::XJScriptInstance overflow;
    Check(!XJ::XJScriptRuntime::InitializeInstance(overflow, overflowModule), "overflow instance initializes");
    const auto overflowResult = XJ::XJScriptRuntime::Execute(
        overflow, 0, {}, native, context);
    Check(!overflowResult.IsValid() && overflow.IsFaulted() &&
              overflowResult.Error->Code == XJ::XJScriptRuntimeErrorCode::IntegerOverflow,
          "integer overflow faults instance");

    const auto budgetModule = Compile(
        "class Budget : ScriptBehaviour { private: int value = 0; "
        "public: void Run() { while (true) { value += 1; } } };");
    XJ::XJScriptInstance budget;
    Check(!XJ::XJScriptRuntime::InitializeInstance(budget, budgetModule), "budget instance initializes");
    const auto budgetResult = XJ::XJScriptRuntime::Execute(
        budget, 0, {}, native, context, {.InstructionBudget = 20, .MaxCallDepth = 64});
    Check(!budgetResult.IsValid() &&
              budgetResult.Error->Code == XJ::XJScriptRuntimeErrorCode::InstructionBudgetExceeded,
          "instruction budget stops infinite loop");

    const auto recursiveModule = Compile(
        "class Recursive : ScriptBehaviour { public: "
        "void Recurse() { Recurse(); } void Run() { Recurse(); } };");
    XJ::XJScriptInstance recursive;
    Check(!XJ::XJScriptRuntime::InitializeInstance(recursive, recursiveModule),
          "recursive instance initializes");
    const auto recursiveResult = XJ::XJScriptRuntime::Execute(
        recursive, 1, {}, native, context, {.InstructionBudget = 100, .MaxCallDepth = 4});
    Check(!recursiveResult.IsValid() &&
              recursiveResult.Error->Code == XJ::XJScriptRuntimeErrorCode::CallDepthExceeded,
          "call depth stops recursion");

    std::cout << "XJScriptRuntime tests passed\n";
    return EXIT_SUCCESS;
}
