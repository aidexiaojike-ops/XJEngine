#include "Script/Runtime/XJScriptRuntime.h"

#include "Script/Bytecode/XJScriptBytecodeVerifier.h"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace XJ
{
    namespace
    {
        XJScriptValueType ValueType(const XJScriptValue& value)
        {
            if (std::holds_alternative<bool>(value)) return XJScriptValueType::Bool;
            if (std::holds_alternative<int64_t>(value)) return XJScriptValueType::Int;
            if (std::holds_alternative<double>(value)) return XJScriptValueType::Float;
            return XJScriptValueType::String;
        }

        bool CheckedAdd(int64_t left, int64_t right, int64_t& result)
        {
            if ((right > 0 && left > std::numeric_limits<int64_t>::max() - right) ||
                (right < 0 && left < std::numeric_limits<int64_t>::min() - right))
                return false;
            result = left + right;
            return true;
        }

        bool CheckedSubtract(int64_t left, int64_t right, int64_t& result)
        {
            if ((right < 0 && left > std::numeric_limits<int64_t>::max() + right) ||
                (right > 0 && left < std::numeric_limits<int64_t>::min() + right))
                return false;
            result = left - right;
            return true;
        }

        bool CheckedMultiply(int64_t left, int64_t right, int64_t& result)
        {
            if (left == 0 || right == 0)
            {
                result = 0;
                return true;
            }
            if ((left == -1 && right == std::numeric_limits<int64_t>::min()) ||
                (right == -1 && left == std::numeric_limits<int64_t>::min()))
                return false;

            if (left > 0)
            {
                if (right > 0 && left > std::numeric_limits<int64_t>::max() / right) return false;
                if (right < 0 && right < std::numeric_limits<int64_t>::min() / left) return false;
            }
            else
            {
                if (right > 0 && left < std::numeric_limits<int64_t>::min() / right) return false;
                if (right < 0 && left < std::numeric_limits<int64_t>::max() / right) return false;
            }

            result = left * right;
            return true;
        }

        struct Frame
        {
            uint32_t FunctionIndex = 0;
            uint32_t Pc = 0;
            std::vector<XJScriptValue> Arguments;
            std::vector<std::optional<XJScriptValue>> Locals;
            std::vector<XJScriptValue> Stack;
        };
    }

    class XJScriptMachine
    {
        public:
            XJScriptMachine(XJScriptInstance& instance,
                    XJScriptNativeInvoker& nativeInvoker,
                    XJScriptNativeContext& nativeContext,
                    XJScriptExecutionLimits limits)
                : mInstance(instance),
                  mModuleOwner(instance.mModule),
                  mModule(*mModuleOwner),
                  mNativeInvoker(nativeInvoker),
                  mNativeContext(nativeContext),
                  mLimits(limits)
            {
            }

            XJScriptExecutionResult Run(uint32_t functionIndex,
                                        std::span<const XJScriptValue> arguments)
            {
                if (!PushFrame(functionIndex, arguments))
                    return Finish();

                try
                {
                    while (!mFrames.empty() && !mResult.Error)
                        ExecuteNext();
                }
                catch (const std::exception& exception)
                {
                    Fail(XJScriptRuntimeErrorCode::InvalidBytecode,
                         std::string("VM internal failure: ") + exception.what());
                }
                catch (...)
                {
                    Fail(XJScriptRuntimeErrorCode::InvalidBytecode,
                         "VM internal failure with unknown exception");
                }
                return Finish();
            }

        private:
            XJScriptExecutionResult Finish()
            {
                if (mResult.Error)
                {
                    mInstance.mFaulted = true;
                    mInstance.mLastError = mResult.Error;
                }
                return std::move(mResult);
            }

            bool PushFrame(uint32_t functionIndex,
                           std::span<const XJScriptValue> arguments)
            {
                mCurrentFunction = functionIndex;
                mCurrentInstruction = 0;
                if (functionIndex >= mModule.Functions.size())
                    return Fail(XJScriptRuntimeErrorCode::InvalidFunction, "Function index is out of range");
                if (mFrames.size() >= mLimits.MaxCallDepth)
                    return Fail(XJScriptRuntimeErrorCode::CallDepthExceeded, "Script call depth exceeded");

                const auto& function = mModule.Functions[functionIndex];
                if (arguments.size() != function.ParameterTypes.size())
                    return Fail(XJScriptRuntimeErrorCode::InvalidArguments, "Function argument count mismatch");
                for (size_t index = 0; index < arguments.size(); ++index)
                {
                    if (ValueType(arguments[index]) != function.ParameterTypes[index])
                        return Fail(XJScriptRuntimeErrorCode::InvalidArguments, "Function argument type mismatch");
                    if (const auto* real = std::get_if<double>(&arguments[index]);
                        real && !std::isfinite(*real))
                        return Fail(XJScriptRuntimeErrorCode::NonFiniteFloat,
                                    "Function argument contains a non-finite float");
                }

                Frame frame;
                frame.FunctionIndex = functionIndex;
                frame.Arguments.assign(arguments.begin(), arguments.end());
                frame.Locals.resize(function.LocalTypes.size());
                const uint32_t capacity = mInstance.mFunctionMaxStackDepths[functionIndex];
                frame.Stack.reserve(capacity);
                mFrames.push_back(std::move(frame));
                return true;
            }

            void ExecuteNext()
            {
                Frame& frame = mFrames.back();
                const auto& function = mModule.Functions[frame.FunctionIndex];
                if (frame.Pc >= function.Code.size())
                {
                    Fail(XJScriptRuntimeErrorCode::InvalidBytecode, "Program counter left function");
                    return;
                }
                if (mResult.ExecutedInstructions >= mLimits.InstructionBudget)
                {
                    Fail(XJScriptRuntimeErrorCode::InstructionBudgetExceeded,
                         "Script instruction budget exceeded");
                    return;
                }

                const uint32_t instructionIndex = frame.Pc++;
                const XJScriptInstruction instruction = function.Code[instructionIndex];
                ++mResult.ExecutedInstructions;
                mCurrentFunction = frame.FunctionIndex;
                mCurrentInstruction = instructionIndex;

                switch (instruction.Op)
                {
                    case XJScriptOpCode::Nop: break;
                    case XJScriptOpCode::PushConstant:
                        Push(mModule.Constants[static_cast<size_t>(instruction.A)]); break;
                    case XJScriptOpCode::LoadField:
                        Push(mInstance.mFields[static_cast<size_t>(instruction.A)]); break;
                    case XJScriptOpCode::StoreField:
                        Store(mInstance.mFields[static_cast<size_t>(instruction.A)]); break;
                    case XJScriptOpCode::LoadArgument:
                        Push(frame.Arguments[static_cast<size_t>(instruction.A)]); break;
                    case XJScriptOpCode::StoreArgument:
                        Store(frame.Arguments[static_cast<size_t>(instruction.A)]); break;
                    case XJScriptOpCode::LoadLocal:
                    {
                        auto& local = frame.Locals[static_cast<size_t>(instruction.A)];
                        if (!local) Fail(XJScriptRuntimeErrorCode::InvalidBytecode, "Read of uninitialized local");
                        else Push(*local);
                        break;
                    }
                    case XJScriptOpCode::StoreLocal:
                    {
                        auto value = Pop();
                        if (value) frame.Locals[static_cast<size_t>(instruction.A)] = std::move(*value);
                        break;
                    }
                    case XJScriptOpCode::Dup:
                        if (frame.Stack.empty()) Fail(XJScriptRuntimeErrorCode::StackUnderflow, "Dup on empty stack");
                        else Push(frame.Stack.back());
                        break;
                    case XJScriptOpCode::Pop: Pop(); break;
                    case XJScriptOpCode::IntToFloat: ConvertIntToFloat(); break;
                    case XJScriptOpCode::INegate: NegateInteger(); break;
                    case XJScriptOpCode::FNegate: UnaryFloat([](double value) { return -value; }); break;
                    case XJScriptOpCode::LogicalNot: UnaryBool([](bool value) { return !value; }); break;

                    case XJScriptOpCode::IAdd: IntegerBinary(CheckedAdd); break;
                    case XJScriptOpCode::ISubtract: IntegerBinary(CheckedSubtract); break;
                    case XJScriptOpCode::IMultiply: IntegerBinary(CheckedMultiply); break;
                    case XJScriptOpCode::IDivide: DivideInteger(); break;
                    case XJScriptOpCode::FAdd: FloatBinary([](double a, double b) { return a + b; }); break;
                    case XJScriptOpCode::FSubtract: FloatBinary([](double a, double b) { return a - b; }); break;
                    case XJScriptOpCode::FMultiply: FloatBinary([](double a, double b) { return a * b; }); break;
                    case XJScriptOpCode::FDivide: DivideFloat(); break;
                    case XJScriptOpCode::StringConcat: ConcatStrings(); break;

                    case XJScriptOpCode::IEqual: Compare<int64_t>([](auto a, auto b) { return a == b; }); break;
                    case XJScriptOpCode::INotEqual: Compare<int64_t>([](auto a, auto b) { return a != b; }); break;
                    case XJScriptOpCode::ILess: Compare<int64_t>([](auto a, auto b) { return a < b; }); break;
                    case XJScriptOpCode::ILessEqual: Compare<int64_t>([](auto a, auto b) { return a <= b; }); break;
                    case XJScriptOpCode::IGreater: Compare<int64_t>([](auto a, auto b) { return a > b; }); break;
                    case XJScriptOpCode::IGreaterEqual: Compare<int64_t>([](auto a, auto b) { return a >= b; }); break;
                    case XJScriptOpCode::FEqual: Compare<double>([](auto a, auto b) { return a == b; }); break;
                    case XJScriptOpCode::FNotEqual: Compare<double>([](auto a, auto b) { return a != b; }); break;
                    case XJScriptOpCode::FLess: Compare<double>([](auto a, auto b) { return a < b; }); break;
                    case XJScriptOpCode::FLessEqual: Compare<double>([](auto a, auto b) { return a <= b; }); break;
                    case XJScriptOpCode::FGreater: Compare<double>([](auto a, auto b) { return a > b; }); break;
                    case XJScriptOpCode::FGreaterEqual: Compare<double>([](auto a, auto b) { return a >= b; }); break;
                    case XJScriptOpCode::BoolEqual: Compare<bool>([](auto a, auto b) { return a == b; }); break;
                    case XJScriptOpCode::BoolNotEqual: Compare<bool>([](auto a, auto b) { return a != b; }); break;
                    case XJScriptOpCode::StringEqual: Compare<std::string>([](const auto& a, const auto& b) { return a == b; }); break;
                    case XJScriptOpCode::StringNotEqual: Compare<std::string>([](const auto& a, const auto& b) { return a != b; }); break;

                    case XJScriptOpCode::Jump: frame.Pc = static_cast<uint32_t>(instruction.A); break;
                    case XJScriptOpCode::JumpIfFalse: ConditionalJump(instruction, false); break;
                    case XJScriptOpCode::JumpIfTrue: ConditionalJump(instruction, true); break;
                    case XJScriptOpCode::CallScript: CallScript(instruction); break;
                    case XJScriptOpCode::CallNative: CallNative(instruction); break;
                    case XJScriptOpCode::ReturnVoid: ReturnVoid(); break;
                    case XJScriptOpCode::ReturnValue: ReturnValue(); break;
                    case XJScriptOpCode::Count:
                        Fail(XJScriptRuntimeErrorCode::InvalidBytecode, "Invalid opcode"); break;
                }
            }

            bool Push(XJScriptValue value)
            {
                Frame& frame = mFrames.back();
                const uint32_t capacity = mInstance.mFunctionMaxStackDepths[frame.FunctionIndex];
                if (frame.Stack.size() >= capacity)
                    return Fail(XJScriptRuntimeErrorCode::StackOverflow, "Operand stack overflow");
                frame.Stack.push_back(std::move(value));
                return true;
            }

            std::optional<XJScriptValue> Pop()
            {
                Frame& frame = mFrames.back();
                if (frame.Stack.empty())
                {
                    Fail(XJScriptRuntimeErrorCode::StackUnderflow, "Operand stack underflow");
                    return std::nullopt;
                }
                XJScriptValue value = std::move(frame.Stack.back());
                frame.Stack.pop_back();
                return value;
            }

            void Store(XJScriptValue& destination)
            {
                auto value = Pop();
                if (value) destination = std::move(*value);
            }

            template<typename T>
            std::optional<T> PopTyped()
            {
                auto value = Pop();
                if (!value) return std::nullopt;
                if (!std::holds_alternative<T>(*value))
                {
                    Fail(XJScriptRuntimeErrorCode::TypeMismatch, "Operand type mismatch");
                    return std::nullopt;
                }
                return std::get<T>(std::move(*value));
            }

            void ConvertIntToFloat()
            {
                auto value = PopTyped<int64_t>();
                if (value) Push(static_cast<double>(*value));
            }

            void NegateInteger()
            {
                auto value = PopTyped<int64_t>();
                if (!value) return;
                if (*value == std::numeric_limits<int64_t>::min())
                {
                    Fail(XJScriptRuntimeErrorCode::IntegerOverflow, "Integer negation overflow");
                    return;
                }
                Push(-*value);
            }

            template<typename Operation>
            void UnaryFloat(Operation operation)
            {
                auto value = PopTyped<double>();
                if (!value) return;
                const double result = operation(*value);
                if (!std::isfinite(result)) Fail(XJScriptRuntimeErrorCode::NonFiniteFloat, "Non-finite float result");
                else Push(result);
            }

            template<typename Operation>
            void UnaryBool(Operation operation)
            {
                auto value = PopTyped<bool>();
                if (value) Push(operation(*value));
            }

            template<typename Operation>
            void IntegerBinary(Operation operation)
            {
                auto right = PopTyped<int64_t>();
                auto left = PopTyped<int64_t>();
                if (!left || !right) return;
                int64_t result = 0;
                if (!operation(*left, *right, result)) Fail(XJScriptRuntimeErrorCode::IntegerOverflow, "Integer arithmetic overflow");
                else Push(result);
            }

            void DivideInteger()
            {
                auto right = PopTyped<int64_t>();
                auto left = PopTyped<int64_t>();
                if (!left || !right) return;
                if (*right == 0) { Fail(XJScriptRuntimeErrorCode::DivideByZero, "Integer divide by zero"); return; }
                if (*left == std::numeric_limits<int64_t>::min() && *right == -1)
                { Fail(XJScriptRuntimeErrorCode::IntegerOverflow, "Integer division overflow"); return; }
                Push(*left / *right);
            }

            template<typename Operation>
            void FloatBinary(Operation operation)
            {
                auto right = PopTyped<double>();
                auto left = PopTyped<double>();
                if (!left || !right) return;
                const double result = operation(*left, *right);
                if (!std::isfinite(result)) Fail(XJScriptRuntimeErrorCode::NonFiniteFloat, "Non-finite float result");
                else Push(result);
            }

            void DivideFloat()
            {
                auto right = PopTyped<double>();
                auto left = PopTyped<double>();
                if (!left || !right) return;
                if (*right == 0.0) { Fail(XJScriptRuntimeErrorCode::DivideByZero, "Float divide by zero"); return; }
                const double result = *left / *right;
                if (!std::isfinite(result)) Fail(XJScriptRuntimeErrorCode::NonFiniteFloat, "Non-finite float result");
                else Push(result);
            }

            void ConcatStrings()
            {
                auto right = PopTyped<std::string>();
                auto left = PopTyped<std::string>();
                if (left && right) Push(*left + *right);
            }

            template<typename T, typename Comparison>
            void Compare(Comparison comparison)
            {
                auto right = PopTyped<T>();
                auto left = PopTyped<T>();
                if (left && right) Push(comparison(*left, *right));
            }

            void ConditionalJump(const XJScriptInstruction& instruction, bool expected)
            {
                auto condition = PopTyped<bool>();
                if (condition && *condition == expected)
                    mFrames.back().Pc = static_cast<uint32_t>(instruction.A);
            }

            std::optional<std::vector<XJScriptValue>> PopArguments(uint32_t count)
            {
                std::vector<XJScriptValue> arguments(count);
                for (uint32_t index = count; index > 0; --index)
                {
                    auto value = Pop();
                    if (!value) return std::nullopt;
                    arguments[index - 1] = std::move(*value);
                }
                return arguments;
            }

            void CallScript(const XJScriptInstruction& instruction)
            {
                auto arguments = PopArguments(static_cast<uint32_t>(instruction.B));
                if (arguments)
                    PushFrame(static_cast<uint32_t>(instruction.A), *arguments);
            }

            void CallNative(const XJScriptInstruction& instruction)
            {
                auto arguments = PopArguments(static_cast<uint32_t>(instruction.B));
                if (!arguments) return;
                const auto result = mNativeInvoker.Invoke(
                    static_cast<XJScriptNativeFunctionId>(instruction.A), *arguments, mNativeContext);
                if (!result.IsValid())
                {
                    FailNative(result);
                    return;
                }
                if (result.ReturnValue)
                    Fail(XJScriptRuntimeErrorCode::NativeInvocationFailed,
                         "Void native returned an unexpected value");
            }

            void ReturnVoid()
            {
                if (!mFrames.back().Stack.empty())
                { Fail(XJScriptRuntimeErrorCode::InvalidBytecode, "ReturnVoid with non-empty stack"); return; }
                mFrames.pop_back();
            }

            void ReturnValue()
            {
                auto value = Pop();
                if (!value) return;
                const auto& function = mModule.Functions[mFrames.back().FunctionIndex];
                if (ValueType(*value) != function.ReturnType)
                { Fail(XJScriptRuntimeErrorCode::TypeMismatch, "Return value type mismatch"); return; }
                if (!mFrames.back().Stack.empty())
                { Fail(XJScriptRuntimeErrorCode::InvalidBytecode, "ReturnValue with extra stack values"); return; }
                mFrames.pop_back();
                if (mFrames.empty()) mResult.ReturnValue = std::move(*value);
                else Push(std::move(*value));
            }

            bool Fail(XJScriptRuntimeErrorCode code, std::string message)
            {
                if (mResult.Error) return false;
                XJScriptRuntimeError error;
                error.Code = code;
                error.FunctionIndex = mCurrentFunction;
                error.InstructionIndex = mCurrentInstruction;
                error.Message = std::move(message);
                if (mCurrentFunction < mModule.Functions.size())
                {
                    const auto& function = mModule.Functions[mCurrentFunction];
                    if (mCurrentInstruction < function.Code.size())
                    {
                        error.Line = function.Code[mCurrentInstruction].Line;
                        error.Column = function.Code[mCurrentInstruction].Column;
                    }
                }
                for (auto frame = mFrames.rbegin(); frame != mFrames.rend(); ++frame)
                {
                    const auto& function = mModule.Functions[frame->FunctionIndex];
                    const uint32_t instructionIndex = frame->Pc == 0 ? 0 : frame->Pc - 1;
                    uint32_t line = 1;
                    uint32_t column = 1;
                    if (instructionIndex < function.Code.size())
                    {
                        line = function.Code[instructionIndex].Line;
                        column = function.Code[instructionIndex].Column;
                    }
                    error.StackTrace.push_back({
                        frame->FunctionIndex, instructionIndex, line, column, function.Name
                    });
                }
                mResult.Error = std::move(error);
                return false;
            }

            void FailNative(const XJScriptNativeInvokeResult& native)
            {
                Fail(XJScriptRuntimeErrorCode::NativeInvocationFailed,
                     native.ErrorMessage.empty() ? "Native invocation failed" : native.ErrorMessage);
                mResult.Error->NativeErrorCode = native.ErrorCode;
            }

            XJScriptInstance& mInstance;
            std::shared_ptr<const XJScriptBytecodeModule> mModuleOwner;
            const XJScriptBytecodeModule& mModule;
            XJScriptNativeInvoker& mNativeInvoker;
            XJScriptNativeContext& mNativeContext;
            XJScriptExecutionLimits mLimits;
            std::vector<Frame> mFrames;
            XJScriptExecutionResult mResult;
            uint32_t mCurrentFunction = 0;
            uint32_t mCurrentInstruction = 0;
    };

    bool XJScriptInstance::SetField(uint32_t index, XJScriptValue value)
    {
        if (mExecuting || !mModule || index >= mFields.size() ||
            index >= mModule->Fields.size())
            return false;
        if (ValueType(value) != mModule->Fields[index].Type)
            return false;
        if (const auto* real = std::get_if<double>(&value); real && !std::isfinite(*real))
            return false;
        mFields[index] = std::move(value);
        return true;
    }

    std::optional<XJScriptRuntimeError> XJScriptRuntime::InitializeInstance(
        XJScriptInstance& instance,
        std::shared_ptr<const XJScriptBytecodeModule> module)
    {
        if (instance.mExecuting)
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::ReentrantExecution;
            error.Message = "Cannot initialize an executing script instance";
            return error;
        }

        instance.mModule.reset();
        instance.mFields.clear();
        instance.mFunctionMaxStackDepths.clear();
        instance.mFaulted = false;
        instance.mLastError.reset();
        if (!module)
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::InvalidModule;
            error.Message = "Script module is null";
            instance.mFaulted = true;
            instance.mLastError = error;
            return error;
        }

        const auto verified = XJScriptBytecodeVerifier::Verify(*module);
        if (!verified.IsValid())
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::InvalidModule;
            error.Message = verified.Diagnostics.front().Message;
            instance.mFaulted = true;
            instance.mLastError = error;
            return error;
        }

        instance.mModule = std::move(module);
        instance.mFunctionMaxStackDepths = verified.FunctionMaxStackDepths;
        instance.mFields.reserve(instance.mModule->Fields.size());
        for (const auto& field : instance.mModule->Fields)
            instance.mFields.push_back(field.DefaultValue);
        return std::nullopt;
    }

    XJScriptExecutionResult XJScriptRuntime::Execute(
        XJScriptInstance& instance,
        uint32_t functionIndex,
        std::span<const XJScriptValue> arguments,
        XJScriptNativeInvoker& nativeInvoker,
        XJScriptNativeContext& nativeContext,
        XJScriptExecutionLimits limits)
    {
        XJScriptExecutionResult result;
        if (instance.mFaulted)
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::InstanceFaulted;
            error.Message = "Script instance is faulted";
            result.Error = std::move(error);
            return result;
        }
        if (instance.mExecuting)
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::ReentrantExecution;
            error.Message = "Script instance is already executing";
            result.Error = std::move(error);
            return result;
        }
        if (!instance.mModule ||
            instance.mFields.size() != instance.mModule->Fields.size() ||
            instance.mFunctionMaxStackDepths.size() != instance.mModule->Functions.size() ||
            limits.InstructionBudget == 0 || limits.MaxCallDepth == 0)
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::InvalidModule;
            error.Message = "Script instance or execution limits are invalid";
            result.Error = error;
            instance.mFaulted = true;
            instance.mLastError = error;
            return result;
        }

        for (size_t index = 0; index < instance.mFields.size(); ++index)
        {
            if (ValueType(instance.mFields[index]) != instance.mModule->Fields[index].Type)
            {
                XJScriptRuntimeError error;
                error.Code = XJScriptRuntimeErrorCode::InvalidModule;
                error.Message = "Script field state does not match module metadata";
                result.Error = error;
                instance.mFaulted = true;
                instance.mLastError = error;
                return result;
            }
            if (const auto* real = std::get_if<double>(&instance.mFields[index]);
                real && !std::isfinite(*real))
            {
                XJScriptRuntimeError error;
                error.Code = XJScriptRuntimeErrorCode::NonFiniteFloat;
                error.Message = "Script field contains a non-finite float";
                result.Error = error;
                instance.mFaulted = true;
                instance.mLastError = error;
                return result;
            }
        }

        instance.mExecuting = true;
        try
        {
            XJScriptMachine machine(instance, nativeInvoker, nativeContext, limits);
            result = machine.Run(functionIndex, arguments);
        }
        catch (const std::exception& exception)
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::InvalidBytecode;
            error.Message = std::string("Failed to start VM: ") + exception.what();
            result.Error = error;
            instance.mFaulted = true;
            instance.mLastError = error;
        }
        catch (...)
        {
            XJScriptRuntimeError error;
            error.Code = XJScriptRuntimeErrorCode::InvalidBytecode;
            error.Message = "Failed to start VM with unknown exception";
            result.Error = error;
            instance.mFaulted = true;
            instance.mLastError = error;
        }
        instance.mExecuting = false;
        return result;
    }
}
