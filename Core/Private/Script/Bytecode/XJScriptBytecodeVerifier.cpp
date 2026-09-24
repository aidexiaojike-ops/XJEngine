#include "Script/Bytecode/XJScriptBytecodeVerifier.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace XJ
{
    namespace
    {
        struct WorkItem
        {
            uint32_t Pc = 0;
            std::vector<XJScriptValueType> Stack;
            std::vector<bool> InitializedLocals;
        };

        struct FlowState
        {
            std::vector<XJScriptValueType> Stack;
            std::vector<bool> InitializedLocals;
        };

        bool IsValidIndex(int32_t value, size_t size)
        {
            return value >= 0 && static_cast<size_t>(value) < size;
        }

        bool IsRuntimeValueType(XJScriptValueType type)
        {
            return type == XJScriptValueType::Bool ||
                   type == XJScriptValueType::Int ||
                   type == XJScriptValueType::Float ||
                   type == XJScriptValueType::String;
        }

        bool IsReturnType(XJScriptValueType type)
        {
            return type == XJScriptValueType::Void || IsRuntimeValueType(type);
        }

        class Verifier
        {
        public:
            explicit Verifier(const XJScriptBytecodeModule& module)
                : mModule(module)
            {
                mResult.FunctionMaxStackDepths.resize(module.Functions.size(), 0);
            }

            XJScriptBytecodeVerifyResult Run()
            {
                VerifyConstants();
                VerifyFields();
                VerifyLifecycleIndices();
                for (uint32_t index = 0; index < mModule.Functions.size(); ++index)
                    VerifyFunction(index);
                return std::move(mResult);
            }

        private:
            static XJScriptValueType ConstantType(const XJScriptConstant& value)
            {
                if (std::holds_alternative<bool>(value)) return XJScriptValueType::Bool;
                if (std::holds_alternative<int64_t>(value)) return XJScriptValueType::Int;
                if (std::holds_alternative<double>(value)) return XJScriptValueType::Float;
                return XJScriptValueType::String;
            }

            void VerifyFields()
            {
                std::unordered_set<uint64_t> fieldIds;
                for (size_t index = 0; index < mModule.Fields.size(); ++index)
                {
                    const auto& field = mModule.Fields[index];
                    if (field.Access == XJScriptAccess::Public && field.StableId == 0)
                        AddModuleError("Public field has no stable FieldId");
                    if (field.StableId != 0 && !fieldIds.insert(field.StableId).second)
                        AddModuleError("Duplicate field stable ID");
                    if (!IsRuntimeValueType(field.Type))
                    {
                        AddModuleError("Field has an invalid runtime type");
                        continue;
                    }
                    if (ConstantType(field.DefaultValue) != field.Type)
                        AddModuleError("Field default value does not match field type");
                    if (const auto* real = std::get_if<double>(&field.DefaultValue);
                        real && !std::isfinite(*real))
                        AddModuleError("Field default value must be finite");
                }
            }

            void VerifyConstants()
            {
                for (const auto& constant : mModule.Constants)
                {
                    if (const auto* real = std::get_if<double>(&constant);
                        real && !std::isfinite(*real))
                        AddModuleError("Float constants must be finite");
                }
            }

            void AddModuleError(std::string message)
            {
                mResult.Diagnostics.push_back({
                    std::nullopt, std::nullopt, 1, 1, std::move(message)
                });
            }

            void AddFunctionError(uint32_t functionIndex, std::string message)
            {
                mResult.Diagnostics.push_back({
                    functionIndex, std::nullopt, 1, 1, std::move(message)
                });
            }

            void AddInstructionError(uint32_t functionIndex, uint32_t instructionIndex,
                                     const XJScriptInstruction& instruction, std::string message)
            {
                mResult.Diagnostics.push_back({
                    functionIndex, instructionIndex,
                    instruction.Line, instruction.Column, std::move(message)
                });
            }

            void VerifyLifecycleIndices()
            {
                VerifyLifecycle(mModule.OnCreate, "OnCreate", 0);
                VerifyLifecycle(mModule.OnUpdate, "OnUpdate", 1);
                VerifyLifecycle(mModule.OnFixedUpdate, "OnFixedUpdate", 1);
                VerifyLifecycle(mModule.OnDestroy, "OnDestroy", 0);
            }

            void VerifyLifecycle(const std::optional<uint32_t>& index,
                                 const char* expectedName, size_t parameterCount)
            {
                if (!index)
                    return;
                if (*index >= mModule.Functions.size())
                {
                    AddModuleError(std::string(expectedName) + " index is out of range");
                    return;
                }

                const auto& function = mModule.Functions[*index];
                if (function.Name != expectedName ||
                    function.ReturnType != XJScriptValueType::Void ||
                    function.ParameterTypes.size() != parameterCount ||
                    (parameterCount == 1 && function.ParameterTypes[0] != XJScriptValueType::Float))
                {
                    AddModuleError(std::string(expectedName) + " has an invalid signature");
                }
            }

            void VerifyFunction(uint32_t functionIndex)
            {
                const auto& function = mModule.Functions[functionIndex];
                if (!IsReturnType(function.ReturnType))
                {
                    AddFunctionError(functionIndex, "Function has an invalid return type");
                    return;
                }
                for (XJScriptValueType type : function.ParameterTypes)
                {
                    if (!IsRuntimeValueType(type))
                    {
                        AddFunctionError(functionIndex, "Function has an invalid parameter type");
                        return;
                    }
                }
                for (XJScriptValueType type : function.LocalTypes)
                {
                    if (!IsRuntimeValueType(type))
                    {
                        AddFunctionError(functionIndex, "Function has an invalid local type");
                        return;
                    }
                }
                if (function.LocalCount != function.LocalTypes.size())
                {
                    AddFunctionError(functionIndex, "LocalCount does not match LocalTypes");
                    return;
                }
                if (function.Code.empty())
                {
                    AddFunctionError(functionIndex, "Function has no bytecode");
                    return;
                }
                if (!VerifyOperands(functionIndex))
                    return;
                VerifyControlFlow(functionIndex);
            }

            bool VerifyOperands(uint32_t functionIndex)
            {
                const auto& function = mModule.Functions[functionIndex];
                bool valid = true;
                for (uint32_t index = 0; index < function.Code.size(); ++index)
                {
                    const auto& instruction = function.Code[index];
                    if (static_cast<uint32_t>(instruction.Op) >=
                        static_cast<uint32_t>(XJScriptOpCode::Count))
                    {
                        AddInstructionError(functionIndex, index, instruction, "Unknown opcode");
                        valid = false;
                        continue;
                    }

                    bool operandValid = true;
                    switch (instruction.Op)
                    {
                        case XJScriptOpCode::PushConstant:
                            operandValid = IsValidIndex(instruction.A, mModule.Constants.size()); break;
                        case XJScriptOpCode::LoadField:
                        case XJScriptOpCode::StoreField:
                            operandValid = IsValidIndex(instruction.A, mModule.Fields.size()); break;
                        case XJScriptOpCode::LoadArgument:
                        case XJScriptOpCode::StoreArgument:
                            operandValid = IsValidIndex(instruction.A, function.ParameterTypes.size()); break;
                        case XJScriptOpCode::LoadLocal:
                        case XJScriptOpCode::StoreLocal:
                            operandValid = IsValidIndex(instruction.A, function.LocalCount); break;
                        case XJScriptOpCode::Jump:
                        case XJScriptOpCode::JumpIfFalse:
                        case XJScriptOpCode::JumpIfTrue:
                            operandValid = IsValidIndex(instruction.A, function.Code.size()); break;
                        case XJScriptOpCode::CallScript:
                            operandValid = IsValidIndex(instruction.A, mModule.Functions.size()) &&
                                instruction.B >= 0 &&
                                static_cast<size_t>(instruction.B) ==
                                    mModule.Functions[static_cast<size_t>(instruction.A)].ParameterTypes.size();
                            break;
                        case XJScriptOpCode::CallNative:
                            operandValid = instruction.A == static_cast<int32_t>(
                                XJScriptNativeFunctionId::TransformRotateY) && instruction.B == 1;
                            break;
                        default:
                            break;
                    }

                    if (!operandValid)
                    {
                        AddInstructionError(functionIndex, index, instruction,
                                            "Instruction operand is out of range or inconsistent");
                        valid = false;
                    }
                }
                return valid;
            }

            bool PopExpected(std::vector<XJScriptValueType>& stack,
                             XJScriptValueType expected,
                             uint32_t functionIndex,
                             uint32_t instructionIndex,
                             const XJScriptInstruction& instruction)
            {
                if (stack.empty())
                {
                    AddInstructionError(functionIndex, instructionIndex, instruction, "Stack underflow");
                    return false;
                }
                if (stack.back() != expected)
                {
                    AddInstructionError(functionIndex, instructionIndex, instruction,
                                        "Stack value type does not match instruction");
                    return false;
                }
                stack.pop_back();
                return true;
            }

            std::optional<FlowState> ApplyInstruction(
                uint32_t functionIndex,
                uint32_t instructionIndex,
                const FlowState& incoming)
            {
                const auto& function = mModule.Functions[functionIndex];
                const auto& instruction = function.Code[instructionIndex];
                FlowState outgoing = incoming;
                auto& stack = outgoing.Stack;

                auto unary = [&](XJScriptValueType type) {
                    return PopExpected(stack, type, functionIndex, instructionIndex, instruction)
                        ? (stack.push_back(type), true) : false;
                };
                auto binary = [&](XJScriptValueType input, XJScriptValueType output) {
                    if (!PopExpected(stack, input, functionIndex, instructionIndex, instruction) ||
                        !PopExpected(stack, input, functionIndex, instructionIndex, instruction))
                        return false;
                    stack.push_back(output);
                    return true;
                };

                bool valid = true;
                switch (instruction.Op)
                {
                    case XJScriptOpCode::Nop:
                    case XJScriptOpCode::Jump:
                    case XJScriptOpCode::ReturnVoid:
                        break;
                    case XJScriptOpCode::PushConstant:
                        stack.push_back(ConstantType(mModule.Constants[static_cast<size_t>(instruction.A)])); break;
                    case XJScriptOpCode::LoadField:
                        stack.push_back(mModule.Fields[static_cast<size_t>(instruction.A)].Type); break;
                    case XJScriptOpCode::LoadArgument:
                        stack.push_back(function.ParameterTypes[static_cast<size_t>(instruction.A)]); break;
                    case XJScriptOpCode::LoadLocal:
                        if (!outgoing.InitializedLocals[static_cast<size_t>(instruction.A)])
                        {
                            AddInstructionError(functionIndex, instructionIndex, instruction,
                                                "LoadLocal reads an uninitialized local");
                            valid = false;
                        }
                        else
                            stack.push_back(function.LocalTypes[static_cast<size_t>(instruction.A)]);
                        break;
                    case XJScriptOpCode::StoreField:
                        valid = PopExpected(stack, mModule.Fields[static_cast<size_t>(instruction.A)].Type,
                                            functionIndex, instructionIndex, instruction); break;
                    case XJScriptOpCode::StoreArgument:
                        valid = PopExpected(stack, function.ParameterTypes[static_cast<size_t>(instruction.A)],
                                            functionIndex, instructionIndex, instruction); break;
                    case XJScriptOpCode::StoreLocal:
                        valid = PopExpected(stack, function.LocalTypes[static_cast<size_t>(instruction.A)],
                                            functionIndex, instructionIndex, instruction);
                        if (valid)
                            outgoing.InitializedLocals[static_cast<size_t>(instruction.A)] = true;
                        break;
                    case XJScriptOpCode::Dup:
                        if (stack.empty()) { AddInstructionError(functionIndex, instructionIndex, instruction, "Stack underflow"); valid = false; }
                        else stack.push_back(stack.back());
                        break;
                    case XJScriptOpCode::Pop:
                        if (stack.empty()) { AddInstructionError(functionIndex, instructionIndex, instruction, "Stack underflow"); valid = false; }
                        else stack.pop_back();
                        break;
                    case XJScriptOpCode::IntToFloat:
                        if (PopExpected(stack, XJScriptValueType::Int, functionIndex, instructionIndex, instruction))
                            stack.push_back(XJScriptValueType::Float);
                        else valid = false;
                        break;
                    case XJScriptOpCode::INegate: valid = unary(XJScriptValueType::Int); break;
                    case XJScriptOpCode::FNegate: valid = unary(XJScriptValueType::Float); break;
                    case XJScriptOpCode::LogicalNot: valid = unary(XJScriptValueType::Bool); break;
                    case XJScriptOpCode::IAdd: case XJScriptOpCode::ISubtract:
                    case XJScriptOpCode::IMultiply: case XJScriptOpCode::IDivide:
                        valid = binary(XJScriptValueType::Int, XJScriptValueType::Int); break;
                    case XJScriptOpCode::FAdd: case XJScriptOpCode::FSubtract:
                    case XJScriptOpCode::FMultiply: case XJScriptOpCode::FDivide:
                        valid = binary(XJScriptValueType::Float, XJScriptValueType::Float); break;
                    case XJScriptOpCode::StringConcat:
                        valid = binary(XJScriptValueType::String, XJScriptValueType::String); break;
                    case XJScriptOpCode::IEqual: case XJScriptOpCode::INotEqual:
                    case XJScriptOpCode::ILess: case XJScriptOpCode::ILessEqual:
                    case XJScriptOpCode::IGreater: case XJScriptOpCode::IGreaterEqual:
                        valid = binary(XJScriptValueType::Int, XJScriptValueType::Bool); break;
                    case XJScriptOpCode::FEqual: case XJScriptOpCode::FNotEqual:
                    case XJScriptOpCode::FLess: case XJScriptOpCode::FLessEqual:
                    case XJScriptOpCode::FGreater: case XJScriptOpCode::FGreaterEqual:
                        valid = binary(XJScriptValueType::Float, XJScriptValueType::Bool); break;
                    case XJScriptOpCode::BoolEqual: case XJScriptOpCode::BoolNotEqual:
                        valid = binary(XJScriptValueType::Bool, XJScriptValueType::Bool); break;
                    case XJScriptOpCode::StringEqual: case XJScriptOpCode::StringNotEqual:
                        valid = binary(XJScriptValueType::String, XJScriptValueType::Bool); break;
                    case XJScriptOpCode::JumpIfFalse: case XJScriptOpCode::JumpIfTrue:
                        valid = PopExpected(stack, XJScriptValueType::Bool,
                                            functionIndex, instructionIndex, instruction); break;
                    case XJScriptOpCode::CallScript:
                    {
                        const auto& target = mModule.Functions[static_cast<size_t>(instruction.A)];
                        for (size_t index = target.ParameterTypes.size(); index > 0 && valid; --index)
                            valid = PopExpected(stack, target.ParameterTypes[index - 1],
                                                functionIndex, instructionIndex, instruction);
                        if (valid && target.ReturnType != XJScriptValueType::Void)
                            stack.push_back(target.ReturnType);
                        break;
                    }
                    case XJScriptOpCode::CallNative:
                        valid = PopExpected(stack, XJScriptValueType::Float,
                                            functionIndex, instructionIndex, instruction); break;
                    case XJScriptOpCode::ReturnValue:
                        valid = PopExpected(stack, function.ReturnType,
                                            functionIndex, instructionIndex, instruction); break;
                    case XJScriptOpCode::Count:
                        valid = false; break;
                }
                return valid ? std::optional<FlowState>{std::move(outgoing)} : std::nullopt;
            }

            void VerifyControlFlow(uint32_t functionIndex)
            {
                const auto& function = mModule.Functions[functionIndex];
                std::vector<std::optional<FlowState>> incoming(function.Code.size());
                std::deque<WorkItem> worklist;
                FlowState entry;
                entry.InitializedLocals.resize(function.LocalTypes.size(), false);
                incoming[0] = entry;
                worklist.push_back(WorkItem{0, {}, entry.InitializedLocals});
                uint32_t maxDepth = 0;

                auto enqueue = [&](uint32_t target, const FlowState& state,
                                   uint32_t fromIndex, const XJScriptInstruction& from)
                {
                    if (target >= function.Code.size())
                    {
                        AddInstructionError(functionIndex, fromIndex, from,
                                            "Control flow leaves function without return");
                        return;
                    }
                    if (!incoming[target])
                    {
                        incoming[target] = state;
                        worklist.push_back(WorkItem{target, state.Stack, state.InitializedLocals});
                    }
                    else if (incoming[target]->Stack != state.Stack)
                    {
                        AddInstructionError(functionIndex, fromIndex, from,
                                            "Control-flow merge has different stack types");
                    }
                    else
                    {
                        bool changed = false;
                        for (size_t index = 0; index < state.InitializedLocals.size(); ++index)
                        {
                            const bool merged = incoming[target]->InitializedLocals[index] &&
                                                state.InitializedLocals[index];
                            if (merged != incoming[target]->InitializedLocals[index])
                            {
                                incoming[target]->InitializedLocals[index] = merged;
                                changed = true;
                            }
                        }
                        if (changed)
                            worklist.push_back(WorkItem{target, incoming[target]->Stack,
                                                       incoming[target]->InitializedLocals});
                    }
                };

                while (!worklist.empty())
                {
                    const WorkItem item = worklist.front();
                    worklist.pop_front();
                    const auto& instruction = function.Code[item.Pc];
                    const FlowState current{item.Stack, item.InitializedLocals};
                    const auto next = ApplyInstruction(functionIndex, item.Pc, current);
                    if (!next)
                        continue;
                    maxDepth = std::max<uint32_t>(maxDepth,
                        static_cast<uint32_t>(std::max(item.Stack.size(), next->Stack.size())));

                    if (instruction.Op == XJScriptOpCode::ReturnVoid)
                    {
                        if (function.ReturnType != XJScriptValueType::Void || !item.Stack.empty())
                            AddInstructionError(functionIndex, item.Pc, instruction,
                                                "ReturnVoid requires void return type and empty stack");
                        continue;
                    }
                    if (instruction.Op == XJScriptOpCode::ReturnValue)
                    {
                        if (function.ReturnType == XJScriptValueType::Void || item.Stack.size() != 1)
                            AddInstructionError(functionIndex, item.Pc, instruction,
                                                "ReturnValue requires non-void return type and one stack value");
                        continue;
                    }

                    if (instruction.Op == XJScriptOpCode::Jump)
                    {
                        enqueue(static_cast<uint32_t>(instruction.A), *next, item.Pc, instruction);
                    }
                    else if (instruction.Op == XJScriptOpCode::JumpIfFalse ||
                             instruction.Op == XJScriptOpCode::JumpIfTrue)
                    {
                        enqueue(static_cast<uint32_t>(instruction.A), *next, item.Pc, instruction);
                        enqueue(item.Pc + 1, *next, item.Pc, instruction);
                    }
                    else
                    {
                        enqueue(item.Pc + 1, *next, item.Pc, instruction);
                    }
                }

                mResult.FunctionMaxStackDepths[functionIndex] = maxDepth;
            }

            const XJScriptBytecodeModule& mModule;
            XJScriptBytecodeVerifyResult mResult;
        };
    }

    XJScriptBytecodeVerifyResult XJScriptBytecodeVerifier::Verify(
        const XJScriptBytecodeModule& module)
    {
        Verifier verifier(module);
        return verifier.Run();
    }
}
