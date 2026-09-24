#include "Script/Compiler/XJScriptCompiler.h"
#include "Script/Bytecode/XJScriptBytecodeVerifier.h"

#include <cstdint>
#include <limits>
#include <utility>

namespace XJ
{
    namespace
    {
        class Compiler
        {
            public:
                Compiler(const XJScriptClassDecl& declaration,
                         const XJScriptSemanticResult& semantic)
                    : mDeclaration(declaration), mSemantic(semantic)
                {
                    mModule.ClassName = declaration.Name;
                }

                XJScriptCompileResult Run()
                {
                    if (!mSemantic.IsValid())
                    {
                        mResult.Diagnostics = mSemantic.Diagnostics;
                        return std::move(mResult);
                    }

                    CompileFields();
                    for (size_t index = 0; index < mDeclaration.Methods.size(); ++index)
                        CompileFunction(mDeclaration.Methods[index], index);

                    if (mResult.Diagnostics.empty())
                    {
                        const auto verified = XJScriptBytecodeVerifier::Verify(mModule);
                        if (!verified.IsValid())
                        {
                            for (const auto& diagnostic : verified.Diagnostics)
                            {
                                mResult.Diagnostics.push_back({
                                    diagnostic.Line,
                                    diagnostic.Column,
                                    diagnostic.Message
                                });
                            }
                        }
                        else
                        {
                            for (size_t index = 0; index < mModule.Functions.size(); ++index)
                            {
                                mModule.Functions[index].MaxStackDepth =
                                    verified.FunctionMaxStackDepths[index];
                            }
                            mResult.Module = std::move(mModule);
                        }
                    }
                    return std::move(mResult);
                }

            private:
                void Error(size_t line, size_t column, std::string message)
                {
                    mResult.Diagnostics.push_back({line, column, std::move(message)});
                }

                void CompileFields()
                {
                    for (const auto& field : mDeclaration.Fields)
                    {
                        if (!field.DefaultValue)
                        {
                            Error(field.Line, field.Column, "Field has no compiled default value");
                            continue;
                        }
                        mModule.Fields.push_back({
                            field.Name,
                            field.StableId,
                            field.Type,
                            field.Access,
                            *field.DefaultValue
                        });
                    }
                }

                void CompileFunction(const XJScriptMethodDecl& method, size_t methodIndex)
                {
                    XJScriptBytecodeFunction function;
                    function.Name = method.Name;
                    function.ReturnType = method.ReturnType;
                    for (const auto& parameter : method.Parameters)
                        function.ParameterTypes.push_back(parameter.Type);

                    const auto localTypes =
                        mSemantic.MethodLocalTypes.find(
                            &method);
                        
                    if (localTypes == mSemantic.MethodLocalTypes.end())
                    {
                        Error(
                            method.Line,
                            method.Column,
                            "Missing local type metadata");
                    }
                    else
                    {
                        function.LocalTypes = localTypes->second;
                    
                        function.LocalCount =
                            static_cast<uint32_t>(
                                function.LocalTypes.size());
                    }

                    mModule.Functions.push_back(std::move(function));
                    mCurrentFunction = &mModule.Functions.back();

                    if (method.Body)
                        CompileBlock(*method.Body);

                    // void 方法允许源码省略 return；统一补一条出口指令。
                    if (method.ReturnType == XJScriptValueType::Void &&
                        (mCurrentFunction->Code.empty() ||
                         mCurrentFunction->Code.back().Op != XJScriptOpCode::ReturnVoid))
                    {
                        Emit(XJScriptOpCode::ReturnVoid, 0, 0, method.Line, method.Column);
                    }

                    if (method.Name == "OnCreate") mModule.OnCreate = static_cast<uint32_t>(methodIndex);
                    else if (method.Name == "OnUpdate") mModule.OnUpdate = static_cast<uint32_t>(methodIndex);
                    else if (method.Name == "OnFixedUpdate") mModule.OnFixedUpdate = static_cast<uint32_t>(methodIndex);
                    else if (method.Name == "OnDestroy") mModule.OnDestroy = static_cast<uint32_t>(methodIndex);
                }

                void CompileBlock(const XJScriptBlockStmt& block)
                {
                    for (const auto& statement : block.Statements)
                    {
                        if (statement)
                            CompileStatement(*statement);
                    }
                }

                void CompileStatement(const XJScriptStmt& statement)
                {
                    if (const auto* block = dynamic_cast<const XJScriptBlockStmt*>(&statement))
                    {
                        CompileBlock(*block);
                        return;
                    }

                    if (const auto* variable = dynamic_cast<const XJScriptVariableStmt*>(&statement))
                    {
                        if (!variable->Initializer || !CompileExpression(*variable->Initializer))
                            return;
                        const auto slot = mSemantic.LocalDeclarations.find(variable);
                        if (slot == mSemantic.LocalDeclarations.end())
                        {
                            Error(variable->Line, variable->Column, "Missing local slot binding");
                            return;
                        }
                        EmitStore(slot->second, variable->Line, variable->Column);
                        return;
                    }

                    if (const auto* expression = dynamic_cast<const XJScriptExpressionStmt*>(&statement))
                    {
                        if (!expression->Expression || !CompileExpression(*expression->Expression))
                            return;
                        const auto type = TypeOf(*expression->Expression);
                        if (type && *type != XJScriptValueType::Void)
                            Emit(XJScriptOpCode::Pop, 0, 0, expression->Line, expression->Column);
                        return;
                    }

                    if (const auto* ifStatement = dynamic_cast<const XJScriptIfStmt*>(&statement))
                    {
                        if (!ifStatement->Condition || !CompileExpression(*ifStatement->Condition))
                            return;
                        const size_t jumpToElse = Emit(XJScriptOpCode::JumpIfFalse, 0, 0,
                                                       ifStatement->Line, ifStatement->Column);
                        if (ifStatement->ThenBranch)
                            CompileStatement(*ifStatement->ThenBranch);
                        const size_t jumpToEnd = Emit(XJScriptOpCode::Jump, 0, 0,
                                                      ifStatement->Line, ifStatement->Column);
                        const size_t elseLabel = EmitLabel(ifStatement->Line, ifStatement->Column);
                        PatchJump(jumpToElse, elseLabel);
                        if (ifStatement->ElseBranch)
                            CompileStatement(*ifStatement->ElseBranch);
                        const size_t endLabel = EmitLabel(ifStatement->Line, ifStatement->Column);
                        PatchJump(jumpToEnd, endLabel);
                        return;
                    }

                    if (const auto* whileStatement = dynamic_cast<const XJScriptWhileStmt*>(&statement))
                    {
                        const size_t loopStart = CurrentOffset();

                        // 当前语言没有 break；while(true) 不生成不可达的退出边。
                        if (whileStatement->Condition && IsLiteralTrue(*whileStatement->Condition))
                        {
                            if (whileStatement->Body)
                                CompileStatement(*whileStatement->Body);
                            Emit(XJScriptOpCode::Jump, ToOperand(loopStart), 0,
                                 whileStatement->Line, whileStatement->Column);
                            return;
                        }

                        if (!whileStatement->Condition || !CompileExpression(*whileStatement->Condition))
                            return;
                        const size_t jumpToEnd = Emit(XJScriptOpCode::JumpIfFalse, 0, 0,
                                                      whileStatement->Line, whileStatement->Column);
                        if (whileStatement->Body)
                            CompileStatement(*whileStatement->Body);
                        Emit(XJScriptOpCode::Jump, ToOperand(loopStart), 0,
                             whileStatement->Line, whileStatement->Column);
                        const size_t endLabel = EmitLabel(whileStatement->Line, whileStatement->Column);
                        PatchJump(jumpToEnd, endLabel);
                        return;
                    }

                    if (const auto* returnStatement = dynamic_cast<const XJScriptReturnStmt*>(&statement))
                    {
                        if (returnStatement->Value)
                        {
                            if (CompileExpression(*returnStatement->Value))
                                Emit(XJScriptOpCode::ReturnValue, 0, 0,
                                     returnStatement->Line, returnStatement->Column);
                        }
                        else
                        {
                            Emit(XJScriptOpCode::ReturnVoid, 0, 0,
                                 returnStatement->Line, returnStatement->Column);
                        }
                    }
                }

                bool CompileExpression(const XJScriptExpr& expression)
                {
                    bool compiled = false;
                    if (const auto* literal = dynamic_cast<const XJScriptLiteralExpr*>(&expression))
                    {
                        Emit(XJScriptOpCode::PushConstant, AddConstant(literal->Value), 0,
                             literal->Line, literal->Column);
                        compiled = true;
                    }
                    else if (const auto* identifier = dynamic_cast<const XJScriptIdentifierExpr*>(&expression))
                    {
                        const auto binding = mSemantic.Bindings.find(identifier);
                        if (binding == mSemantic.Bindings.end())
                            Error(identifier->Line, identifier->Column, "Missing identifier binding");
                        else
                        {
                            EmitLoad(binding->second, identifier->Line, identifier->Column);
                            compiled = true;
                        }
                    }
                    else if (const auto* unary = dynamic_cast<const XJScriptUnaryExpr*>(&expression))
                        compiled = CompileUnary(*unary);
                    else if (const auto* binary = dynamic_cast<const XJScriptBinaryExpr*>(&expression))
                        compiled = CompileBinary(*binary);
                    else if (const auto* assignment = dynamic_cast<const XJScriptAssignmentExpr*>(&expression))
                        compiled = CompileAssignment(*assignment);
                    else if (const auto* call = dynamic_cast<const XJScriptCallExpr*>(&expression))
                        compiled = CompileCall(*call);
                    else
                        Error(expression.Line, expression.Column, "Unsupported expression in compiler");

                    if (compiled && mSemantic.Conversions.contains(&expression))
                        Emit(XJScriptOpCode::IntToFloat, 0, 0, expression.Line, expression.Column);
                    return compiled;
                }

                bool CompileUnary(const XJScriptUnaryExpr& expression)
                {
                    if (!expression.Operand)
                        return false;
                    const bool update = expression.Op == XJScriptUnaryOp::PreIncrement ||
                                        expression.Op == XJScriptUnaryOp::PreDecrement ||
                                        expression.Op == XJScriptUnaryOp::PostIncrement ||
                                        expression.Op == XJScriptUnaryOp::PostDecrement;
                    if (update)
                        return CompileUpdate(expression);

                    if (!CompileExpression(*expression.Operand))
                        return false;
                    const auto type = TypeOf(*expression.Operand);
                    if (!type)
                        return false;
                    if (expression.Op == XJScriptUnaryOp::Not)
                        Emit(XJScriptOpCode::LogicalNot, 0, 0, expression.Line, expression.Column);
                    else if (expression.Op == XJScriptUnaryOp::Negate)
                        Emit(*type == XJScriptValueType::Float ? XJScriptOpCode::FNegate
                                                               : XJScriptOpCode::INegate,
                             0, 0, expression.Line, expression.Column);
                    return true;
                }

                bool CompileUpdate(const XJScriptUnaryExpr& expression)
                {
                    const auto* identifier = dynamic_cast<const XJScriptIdentifierExpr*>(expression.Operand.get());
                    if (!identifier)
                    {
                        Error(expression.Line, expression.Column, "Unsupported increment target");
                        return false;
                    }
                    const auto binding = mSemantic.Bindings.find(identifier);
                    if (binding == mSemantic.Bindings.end())
                        return false;

                    const bool prefix = expression.Op == XJScriptUnaryOp::PreIncrement ||
                                        expression.Op == XJScriptUnaryOp::PreDecrement;
                    const bool increment = expression.Op == XJScriptUnaryOp::PreIncrement ||
                                           expression.Op == XJScriptUnaryOp::PostIncrement;
                    EmitLoad(binding->second, expression.Line, expression.Column);
                    if (!prefix)
                        Emit(XJScriptOpCode::Dup, 0, 0, expression.Line, expression.Column);

                    const bool floating = binding->second.Type == XJScriptValueType::Float;
                    Emit(XJScriptOpCode::PushConstant,
                         AddConstant(floating ? XJScriptConstant{1.0} : XJScriptConstant{int64_t{1}}),
                         0, expression.Line, expression.Column);
                    Emit(floating
                             ? (increment ? XJScriptOpCode::FAdd : XJScriptOpCode::FSubtract)
                             : (increment ? XJScriptOpCode::IAdd : XJScriptOpCode::ISubtract),
                         0, 0, expression.Line, expression.Column);

                    if (prefix)
                        Emit(XJScriptOpCode::Dup, 0, 0, expression.Line, expression.Column);
                    EmitStore(binding->second, expression.Line, expression.Column);
                    return true;
                }

                bool CompileBinary(const XJScriptBinaryExpr& expression)
                {
                    if (!expression.Left || !expression.Right)
                        return false;

                    if (expression.Op == XJScriptBinaryOp::LogicalAnd ||
                        expression.Op == XJScriptBinaryOp::LogicalOr)
                        return CompileShortCircuit(expression);

                    if (!CompileExpression(*expression.Left) || !CompileExpression(*expression.Right))
                        return false;
                    const auto leftType = EffectiveType(*expression.Left);
                    const auto rightType = EffectiveType(*expression.Right);
                    if (!leftType || !rightType)
                        return false;

                    const bool floating = *leftType == XJScriptValueType::Float ||
                                          *rightType == XJScriptValueType::Float;
                    XJScriptOpCode op = XJScriptOpCode::IAdd;
                    switch (expression.Op)
                    {
                        case XJScriptBinaryOp::Add:
                            op = *leftType == XJScriptValueType::String
                                ? XJScriptOpCode::StringConcat
                                : (floating ? XJScriptOpCode::FAdd : XJScriptOpCode::IAdd); break;
                        case XJScriptBinaryOp::Subtract:
                            op = floating ? XJScriptOpCode::FSubtract : XJScriptOpCode::ISubtract; break;
                        case XJScriptBinaryOp::Multiply:
                            op = floating ? XJScriptOpCode::FMultiply : XJScriptOpCode::IMultiply; break;
                        case XJScriptBinaryOp::Divide:
                            op = floating ? XJScriptOpCode::FDivide : XJScriptOpCode::IDivide; break;
                        case XJScriptBinaryOp::Equal:
                            op = EqualityOp(*leftType, false); break;
                        case XJScriptBinaryOp::NotEqual:
                            op = EqualityOp(*leftType, true); break;
                        case XJScriptBinaryOp::Less:
                            op = floating ? XJScriptOpCode::FLess : XJScriptOpCode::ILess; break;
                        case XJScriptBinaryOp::LessEqual:
                            op = floating ? XJScriptOpCode::FLessEqual : XJScriptOpCode::ILessEqual; break;
                        case XJScriptBinaryOp::Greater:
                            op = floating ? XJScriptOpCode::FGreater : XJScriptOpCode::IGreater; break;
                        case XJScriptBinaryOp::GreaterEqual:
                            op = floating ? XJScriptOpCode::FGreaterEqual : XJScriptOpCode::IGreaterEqual; break;
                        default: break;
                    }
                    Emit(op, 0, 0, expression.Line, expression.Column);
                    return true;
                }

                bool CompileShortCircuit(const XJScriptBinaryExpr& expression)
                {
                    if (!CompileExpression(*expression.Left))
                        return false;
                    const bool logicalAnd = expression.Op == XJScriptBinaryOp::LogicalAnd;
                    const size_t branch = Emit(logicalAnd ? XJScriptOpCode::JumpIfFalse
                                                          : XJScriptOpCode::JumpIfTrue,
                                               0, 0, expression.Line, expression.Column);
                    if (!CompileExpression(*expression.Right))
                        return false;
                    const size_t endJump = Emit(XJScriptOpCode::Jump, 0, 0,
                                                expression.Line, expression.Column);
                    PatchJump(branch, CurrentOffset());
                    Emit(XJScriptOpCode::PushConstant, AddConstant(!logicalAnd), 0,
                         expression.Line, expression.Column);
                    const size_t endLabel = EmitLabel(expression.Line, expression.Column);
                    PatchJump(endJump, endLabel);
                    return true;
                }

                bool CompileAssignment(const XJScriptAssignmentExpr& expression)
                {
                    const auto* identifier = dynamic_cast<const XJScriptIdentifierExpr*>(expression.Target.get());
                    if (!identifier || !expression.Value)
                    {
                        Error(expression.Line, expression.Column, "Unsupported assignment target");
                        return false;
                    }
                    const auto binding = mSemantic.Bindings.find(identifier);
                    if (binding == mSemantic.Bindings.end())
                        return false;

                    if (expression.Op == XJScriptAssignmentOp::Assign)
                    {
                        if (!CompileExpression(*expression.Value))
                            return false;
                    }
                    else
                    {
                        EmitLoad(binding->second, expression.Line, expression.Column);
                        if (!CompileExpression(*expression.Value))
                            return false;
                        const bool floating = binding->second.Type == XJScriptValueType::Float;
                        XJScriptOpCode op = XJScriptOpCode::IAdd;
                        if (expression.Op == XJScriptAssignmentOp::AddAssign)
                            op = binding->second.Type == XJScriptValueType::String
                                ? XJScriptOpCode::StringConcat
                                : (floating ? XJScriptOpCode::FAdd : XJScriptOpCode::IAdd);
                        else if (expression.Op == XJScriptAssignmentOp::SubtractAssign)
                            op = floating ? XJScriptOpCode::FSubtract : XJScriptOpCode::ISubtract;
                        else if (expression.Op == XJScriptAssignmentOp::MultiplyAssign)
                            op = floating ? XJScriptOpCode::FMultiply : XJScriptOpCode::IMultiply;
                        else if (expression.Op == XJScriptAssignmentOp::DivideAssign)
                            op = floating ? XJScriptOpCode::FDivide : XJScriptOpCode::IDivide;
                        Emit(op, 0, 0, expression.Line, expression.Column);
                    }

                    // 赋值本身也是表达式；复制一份给 Store 消费，原值留在栈顶。
                    Emit(XJScriptOpCode::Dup, 0, 0, expression.Line, expression.Column);
                    EmitStore(binding->second, expression.Line, expression.Column);
                    return true;
                }

                bool CompileCall(const XJScriptCallExpr& expression)
                {
                    for (const auto& argument : expression.Arguments)
                    {
                        if (!argument || !CompileExpression(*argument))
                            return false;
                    }
                    const auto call = mSemantic.Calls.find(&expression);
                    if (call == mSemantic.Calls.end())
                    {
                        Error(expression.Line, expression.Column, "Missing resolved call binding");
                        return false;
                    }
                    if (call->second.Kind == XJScriptCallKind::ScriptMethod)
                        Emit(XJScriptOpCode::CallScript, ToOperand(call->second.MethodIndex),
                             ToOperand(expression.Arguments.size()), expression.Line, expression.Column);
                else
                    Emit(XJScriptOpCode::CallNative,
                         static_cast<int32_t>(XJScriptNativeFunctionId::TransformRotateY),
                         ToOperand(expression.Arguments.size()), expression.Line, expression.Column);
                    return true;
                }

                void EmitLoad(const XJScriptResolvedSymbol& symbol, size_t line, size_t column)
                {
                    XJScriptOpCode op = XJScriptOpCode::LoadLocal;
                    if (symbol.Kind == XJScriptSymbolKind::Field) op = XJScriptOpCode::LoadField;
                    else if (symbol.Kind == XJScriptSymbolKind::Parameter) op = XJScriptOpCode::LoadArgument;
                    Emit(op, ToOperand(symbol.Index), 0, line, column);
                }

                void EmitStore(const XJScriptResolvedSymbol& symbol, size_t line, size_t column)
                {
                    XJScriptOpCode op = XJScriptOpCode::StoreLocal;
                    if (symbol.Kind == XJScriptSymbolKind::Field) op = XJScriptOpCode::StoreField;
                    else if (symbol.Kind == XJScriptSymbolKind::Parameter) op = XJScriptOpCode::StoreArgument;
                    Emit(op, ToOperand(symbol.Index), 0, line, column);
                }

                XJScriptOpCode EqualityOp(XJScriptValueType type, bool notEqual) const
                {
                    if (type == XJScriptValueType::Float)
                        return notEqual ? XJScriptOpCode::FNotEqual : XJScriptOpCode::FEqual;
                    if (type == XJScriptValueType::Bool)
                        return notEqual ? XJScriptOpCode::BoolNotEqual : XJScriptOpCode::BoolEqual;
                    if (type == XJScriptValueType::String)
                        return notEqual ? XJScriptOpCode::StringNotEqual : XJScriptOpCode::StringEqual;
                    return notEqual ? XJScriptOpCode::INotEqual : XJScriptOpCode::IEqual;
                }

                std::optional<XJScriptValueType> TypeOf(const XJScriptExpr& expression) const
                {
                    const auto type = mSemantic.ExpressionTypes.find(&expression);
                    return type == mSemantic.ExpressionTypes.end()
                        ? std::nullopt : std::optional<XJScriptValueType>{type->second};
                }

                std::optional<XJScriptValueType> EffectiveType(const XJScriptExpr& expression) const
                {
                    if (mSemantic.Conversions.contains(&expression))
                        return XJScriptValueType::Float;
                    return TypeOf(expression);
                }

                int32_t AddConstant(XJScriptConstant constant)
                {
                    for (size_t index = 0; index < mModule.Constants.size(); ++index)
                    {
                        if (mModule.Constants[index] == constant)
                            return ToOperand(index);
                    }
                    mModule.Constants.push_back(std::move(constant));
                    return ToOperand(mModule.Constants.size() - 1);
                }

                size_t Emit(XJScriptOpCode op, int32_t a, int32_t b, size_t line, size_t column)
                {
                    mCurrentFunction->Code.push_back({
                        op, a, b, static_cast<uint32_t>(line), static_cast<uint32_t>(column)
                    });
                    return mCurrentFunction->Code.size() - 1;
                }

                size_t EmitLabel(size_t line, size_t column)
                {
                    return Emit(XJScriptOpCode::Nop, 0, 0, line, column);
                }

                void PatchJump(size_t instructionIndex, size_t target)
                {
                    if (instructionIndex >= mCurrentFunction->Code.size())
                        return;
                    mCurrentFunction->Code[instructionIndex].A = ToOperand(target);
                }

                size_t CurrentOffset() const { return mCurrentFunction->Code.size(); }

                static bool IsLiteralTrue(const XJScriptExpr& expression)
                {
                    const auto* literal = dynamic_cast<const XJScriptLiteralExpr*>(&expression);
                    if (!literal)
                        return false;
                    const auto* value = std::get_if<bool>(&literal->Value);
                    return value && *value;
                }

                static int32_t ToOperand(size_t value)
                {
                    return value > static_cast<size_t>(std::numeric_limits<int32_t>::max())
                        ? std::numeric_limits<int32_t>::max()
                        : static_cast<int32_t>(value);
                }

                const XJScriptClassDecl& mDeclaration;
                const XJScriptSemanticResult& mSemantic;
                XJScriptCompileResult mResult;
                XJScriptBytecodeModule mModule;
                XJScriptBytecodeFunction* mCurrentFunction = nullptr;
            };
    }

    XJScriptCompileResult XJScriptCompiler::Compile(
        const XJScriptClassDecl& declaration,
        const XJScriptSemanticResult& semantic)
    {
        Compiler compiler(declaration, semantic);
        return compiler.Run();
    }
}
