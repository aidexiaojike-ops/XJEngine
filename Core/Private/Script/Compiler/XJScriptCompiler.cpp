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
                    mModule.NativeCatalogue = semantic.NativeCatalogue;
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
                    else if (const auto* member = dynamic_cast<const XJScriptMemberExpr*>(&expression))
                        compiled = CompileMember(*member);
                    else
                        Error(expression.Line, expression.Column, "Unsupported expression in compiler");

                    if (compiled && mSemantic.Conversions.contains(&expression))
                        Emit(XJScriptOpCode::IntToFloat, 0, 0, expression.Line, expression.Column);
                    return compiled;
                }

                bool CompileUnary(const XJScriptUnaryExpr& expression)//编译单目运算符
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

                    if (*type == XJScriptValueType::Vec3)
                    {
                        if (expression.Op == XJScriptUnaryOp::Negate)
                            Emit(XJScriptOpCode::Vec3Negate, 0, 0,
                                 expression.Line, expression.Column);
                            
                        return true;
                    }
                    
                    if (expression.Op == XJScriptUnaryOp::Not)
                        Emit(XJScriptOpCode::LogicalNot, 0, 0, expression.Line, expression.Column);
                    else if (expression.Op == XJScriptUnaryOp::Negate)
                        Emit(*type == XJScriptValueType::Float ? XJScriptOpCode::FNegate
                                                               : XJScriptOpCode::INegate,
                             0, 0, expression.Line, expression.Column);
                    return true;
                    
                }

                bool CompileUpdate(const XJScriptUnaryExpr& expression)//编译更新
                {
                    if (!expression.Operand)
                        return false;
                
                    const auto target = ResolveLValue(*expression.Operand);
                
                    if (!target)
                    {
                        Error(expression.Line, expression.Column,
                              "Unsupported increment target");
                        return false;
                    }
                
                    const bool prefix =
                        expression.Op == XJScriptUnaryOp::PreIncrement ||
                        expression.Op == XJScriptUnaryOp::PreDecrement;
                
                    const bool increment =
                        expression.Op == XJScriptUnaryOp::PreIncrement ||
                        expression.Op == XJScriptUnaryOp::PostIncrement;
                
                    const bool floating =
                        target->GetType() == XJScriptValueType::Float;
                
                    EmitLoadLValue(*target, expression.Line, expression.Column);
                
                    if (!prefix)
                        Emit(XJScriptOpCode::Dup, 0, 0,
                             expression.Line, expression.Column);
                        
                    Emit(
                        XJScriptOpCode::PushConstant,
                        AddConstant(floating
                            ? XJScriptConstant{1.0}
                            : XJScriptConstant{int64_t{1}}),
                        0, expression.Line, expression.Column);
                        
                    Emit(
                        floating
                            ? (increment ? XJScriptOpCode::FAdd
                                         : XJScriptOpCode::FSubtract)
                            : (increment ? XJScriptOpCode::IAdd
                                         : XJScriptOpCode::ISubtract),
                        0, 0, expression.Line, expression.Column);
                            
                    if (prefix)
                        Emit(XJScriptOpCode::Dup, 0, 0,
                             expression.Line, expression.Column);
                        
                    EmitStoreLValue(*target, expression.Line, expression.Column);
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
                    
                    if (*leftType == XJScriptValueType::Vec3 ||
                        *rightType == XJScriptValueType::Vec3)
                    {
                        if (expression.Op == XJScriptBinaryOp::Equal ||
                            expression.Op == XJScriptBinaryOp::NotEqual)
                        {
                            Emit(
                                EqualityOp(
                                    XJScriptValueType::Vec3,
                                    expression.Op == XJScriptBinaryOp::NotEqual),
                                0, 0, expression.Line, expression.Column);
                            return true;
                        }
                    
                        const auto op = ArithmeticOp(
                            expression.Op, *leftType, *rightType);
                        
                        if (!op)
                            return false;
                        
                        Emit(*op, 0, 0, expression.Line, expression.Column);
                        return true;
                    }

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

                bool CompileAssignment(const XJScriptAssignmentExpr& expression)//编译作业
                {
                    if (!expression.Target || !expression.Value)
                        return false;
                
                    const auto target = ResolveLValue(*expression.Target);
                
                    if (!target)
                    {
                        Error(expression.Line, expression.Column,
                              "Unsupported assignment target");
                        return false;
                    }
                
                    if (expression.Op == XJScriptAssignmentOp::Assign)
                    {
                        if (!CompileExpression(*expression.Value))
                            return false;
                    }
                    else
                    {
                        EmitLoadLValue(*target, expression.Line, expression.Column);
                    
                        if (!CompileExpression(*expression.Value))
                            return false;
                    
                        const auto right = EffectiveType(*expression.Value);
                        if (!right)
                            return false;
                    
                        XJScriptBinaryOp operation = XJScriptBinaryOp::Add;
                    
                        switch (expression.Op)
                        {
                            case XJScriptAssignmentOp::AddAssign:
                                operation = XJScriptBinaryOp::Add; break;
                            case XJScriptAssignmentOp::SubtractAssign:
                                operation = XJScriptBinaryOp::Subtract; break;
                            case XJScriptAssignmentOp::MultiplyAssign:
                                operation = XJScriptBinaryOp::Multiply; break;
                            case XJScriptAssignmentOp::DivideAssign:
                                operation = XJScriptBinaryOp::Divide; break;
                            default:
                                return false;
                        }
                    
                        const auto op = ArithmeticOp(
                            operation, target->GetType(), *right);
                        
                        if (!op)
                            return false;
                        
                        Emit(*op, 0, 0, expression.Line, expression.Column);
                    }
                
                    Emit(XJScriptOpCode::Dup, 0, 0,
                         expression.Line, expression.Column);
                    
                    EmitStoreLValue(*target, expression.Line, expression.Column);
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
                            static_cast<int32_t>(call->second.NativeId),
                            ToOperand(expression.Arguments.size()),
                            expression.Line,
                            expression.Column);
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

                    if (type == XJScriptValueType::Vec3)
                        return notEqual
                            ? XJScriptOpCode::Vec3NotEqual
                            : XJScriptOpCode::Vec3Equal;

                    if (type == XJScriptValueType::EntityRef)
                        return notEqual
                            ? XJScriptOpCode::EntityRefNotEqual
                            : XJScriptOpCode::EntityRefEqual;

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

                size_t Emit(XJScriptOpCode op, int32_t a, int32_t b, size_t line, size_t column)//发射
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

                struct LValue//左值
                {
                    XJScriptResolvedSymbol Symbol;
                    std::optional<uint32_t> Component;

                    XJScriptValueType GetType() const
                    {
                        return Component ? XJScriptValueType::Float:Symbol.Type;
                    }
                };

                std::optional<LValue> ResolveLValue(const XJScriptExpr& expression)//解析左值
                {
                    const XJScriptIdentifierExpr* identifier = dynamic_cast<const XJScriptIdentifierExpr*>(&expression);

                    std::optional<uint32_t> component;

                    if(const auto* member = dynamic_cast<const XJScriptMemberExpr*>(&expression))
                    {
                        component = XJGetVec3ComponentIndex(member->Member);
                        identifier = dynamic_cast<const XJScriptIdentifierExpr*>(member->Object.get());

                        if(!component)
                            return std::nullopt;
                    }

                    if(!identifier)
                        return std::nullopt;

                    const auto found = mSemantic.Bindings.find(identifier);

                    if(found == mSemantic.Bindings.end() ||
                        !found->second.Writable||
                        found->second.Kind == XJScriptSymbolKind::BuiltinObject)
                    {
                        return std::nullopt;
                    }

                    if(component && found->second.Type != XJScriptValueType::Vec3)
                        return std::nullopt;

                    return LValue(found->second, component);
                }

                void EmitLoadLValue(const LValue& value, size_t line, size_t colum)//加载存储左值
                {
                    EmitLoad(value.Symbol, line, colum);

                    if(value.Component)
                    {
                        Emit(XJScriptOpCode::Vec3GetComponent,
                             static_cast<int32_t>(*value.Component), 0, line, colum);
                    }
                }

                void EmitStoreLValue(const LValue& value, size_t line, size_t column)//发射存储左值
                {
                    if (!value.Component)
                    {
                        EmitStore(value.Symbol, line, column);
                        return;
                    }
                
                    XJScriptOpCode op = XJScriptOpCode::StoreLocalVec3Component;
                
                    if (value.Symbol.Kind == XJScriptSymbolKind::Field)
                        op = XJScriptOpCode::StoreFieldVec3Component;
                    else if (value.Symbol.Kind == XJScriptSymbolKind::Parameter)
                        op = XJScriptOpCode::StoreArgumentVec3Component;
                
                    Emit(
                        op,
                        ToOperand(value.Symbol.Index),
                        static_cast<int32_t>(*value.Component),
                        line, column);
                }

                bool CompileMember(const XJScriptMemberExpr& EXPRESSION)//分量读取
                {
                    const auto component = XJGetVec3ComponentIndex(EXPRESSION.Member);

                    if(!component || !EXPRESSION.Object || !CompileExpression(*EXPRESSION.Object))
                    {
                        Error(EXPRESSION.Line, EXPRESSION.Column,
                              "Invalid Vec3 member access");
                        return false;
                    }

                    Emit(
                        XJScriptOpCode::Vec3GetComponent,
                        static_cast<int32_t>(*component),
                        0,
                        EXPRESSION.Line,
                        EXPRESSION.Column);

                    return true;
                }
                std::optional<XJScriptOpCode> ArithmeticOp(XJScriptBinaryOp operation, XJScriptValueType left, XJScriptValueType right) const//通用算术指令选择
                {
                    using Op = XJScriptOpCode;
                    using Type = XJScriptValueType;

                    if (left == Type::Vec3 || right == Type::Vec3)
                    {
                        if (left == Type::Vec3 && right == Type::Vec3)
                        {
                            if (operation == XJScriptBinaryOp::Add)
                                return Op::Vec3Add;
                            if (operation == XJScriptBinaryOp::Subtract)
                                return Op::Vec3Subtract;
                        }
                    
                        if (operation == XJScriptBinaryOp::Multiply)
                        {
                            if (left == Type::Vec3 && right == Type::Float)
                                return Op::Vec3MultiplyFloat;
                            if (left == Type::Float && right == Type::Vec3)
                                return Op::FloatMultiplyVec3;
                        }
                    
                        if (operation == XJScriptBinaryOp::Divide &&
                            left == Type::Vec3 && right == Type::Float)
                        {
                            return Op::Vec3DivideFloat;
                        }
                    
                        return std::nullopt;
                    }
                
                    if (operation == XJScriptBinaryOp::Add &&
                        left == Type::String && right == Type::String)
                    {
                        return Op::StringConcat;
                    }
                
                    const bool floating = left == Type::Float || right == Type::Float;
                
                    switch (operation)
                    {
                        case XJScriptBinaryOp::Add:
                            return floating ? Op::FAdd : Op::IAdd;
                        case XJScriptBinaryOp::Subtract:
                            return floating ? Op::FSubtract : Op::ISubtract;
                        case XJScriptBinaryOp::Multiply:
                            return floating ? Op::FMultiply : Op::IMultiply;
                        case XJScriptBinaryOp::Divide:
                            return floating ? Op::FDivide : Op::IDivide;
                        default:
                            return std::nullopt;
                    }
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
