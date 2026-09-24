#include "Script/Language/XJScriptSemanticAnalyzer.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace XJ
{
    namespace
    {
        struct ExpressionInfo
        {
            // AnalyzeExpression 的内部返回值；Writable 用于赋值和 ++/-- 检查。
            XJScriptValueType Type = XJScriptValueType::Void;
            bool Valid = false;
            bool Writable = false;
        };

        char TypeCode(XJScriptValueType type)
        {
            switch (type)
            {
                case XJScriptValueType::Void: return 'v';
                case XJScriptValueType::Bool: return 'b';
                case XJScriptValueType::Int: return 'i';
                case XJScriptValueType::Float: return 'f';
                case XJScriptValueType::String: return 's';
            }
            return '?';
        }

        std::string MethodSignature(const XJScriptMethodDecl& method)
        {
            std::string signature = method.Name + "(";
            for (const auto& parameter : method.Parameters)
                signature.push_back(TypeCode(parameter.Type));
            signature.push_back(')');
            return signature;
        }

        XJScriptValueType LiteralType(const XJScriptLiteralValue& value)
        {
            if (std::holds_alternative<bool>(value)) return XJScriptValueType::Bool;
            if (std::holds_alternative<int64_t>(value)) return XJScriptValueType::Int;
            if (std::holds_alternative<double>(value)) return XJScriptValueType::Float;
            return XJScriptValueType::String;
        }

        bool IsNumeric(XJScriptValueType type)
        {
            return type == XJScriptValueType::Int || type == XJScriptValueType::Float;
        }

        class Analyzer
        {
            public:
                explicit Analyzer(const XJScriptClassDecl& declaration)
                    : mDeclaration(declaration)
                {
                }

                XJScriptSemanticResult Run()
                {
                    // 先收集类级符号，使方法可以调用后声明方法并参与重载解析。
                    CollectFields();
                    CollectMethods();

                    for (size_t index = 0; index < mDeclaration.Methods.size(); ++index)
                        AnalyzeMethod(mDeclaration.Methods[index]);

                    return std::move(mResult);
                }

            private:
                using Scope = std::unordered_map<std::string, XJScriptResolvedSymbol>;
                
                void Error(size_t line, size_t column, std::string message)
                {
                    mResult.Diagnostics.push_back({line, column, std::move(message)});
                }
            
            void CollectFields()
            {
                // 字段先全部进入类作用域，使方法声明顺序不影响字段可见性。
                for (size_t index = 0; index < mDeclaration.Fields.size(); ++index)
                    {
                        const auto& field = mDeclaration.Fields[index];
                        XJScriptResolvedSymbol symbol{
                            .Kind = XJScriptSymbolKind::Field,
                            .Type = field.Type,
                            .Index = index,
                            .Writable = true
                        };
                        if (!mFields.emplace(field.Name, symbol).second)
                            Error(field.Line, field.Column, "Duplicate field declaration");
                    
                        if (!field.DefaultValue)
                        {
                            Error(field.Line, field.Column, "Field requires an initializer");
                            continue;
                        }
                    
                        if (LiteralType(*field.DefaultValue) != field.Type)
                            Error(field.Line, field.Column, "Field initializer type does not match field type");
                    }
                }
            
            void CollectMethods()
            {
                // 方法按名称分组保存重载集合；完全相同的参数签名仍属于重复声明。
                std::unordered_set<std::string> signatures;
                    for (size_t index = 0; index < mDeclaration.Methods.size(); ++index)
                    {
                        const auto& method = mDeclaration.Methods[index];
                        const std::string signature = MethodSignature(method);
                        if (!signatures.insert(signature).second)
                            Error(method.Line, method.Column, "Duplicate method overload");
                        mMethodOverloads[method.Name].push_back(index);
                    }
                }
            
                void AnalyzeMethod(const XJScriptMethodDecl& method)
                {
                    // 参数和方法最外层局部变量共享一个作用域，防止同层重名。
                    mCurrentMethod = &method;
                    mScopes.clear();
                    mScopes.emplace_back();
                    mNextLocalIndex = 0;
                    mCurrentLocalTypes.clear();
                
                    for (size_t index = 0; index < method.Parameters.size(); ++index)
                    {
                        const auto& parameter = method.Parameters[index];
                        XJScriptResolvedSymbol symbol{
                            .Kind = XJScriptSymbolKind::Parameter,
                            .Type = parameter.Type,
                            .Index = index,
                            .Writable = true
                        };

                        if (!mScopes.back().emplace(parameter.Name, symbol).second)
                            Error(parameter.Line, parameter.Column, "Duplicate parameter name");
                    }
                
                    const bool alwaysReturns = method.Body && AnalyzeBlock(*method.Body, false);
                    mResult.MethodLocalCounts[&method] = mNextLocalIndex;
                    mResult.MethodLocalTypes[&method] = mCurrentLocalTypes;
                    if (method.ReturnType != XJScriptValueType::Void && !alwaysReturns)
                        Error(method.Line, method.Column, "Non-void method does not return on every path");
                }
            
            bool AnalyzeBlock(const XJScriptBlockStmt& block, bool createScope)
            {
                // 方法最外层复用参数作用域；普通花括号则 push/pop 新作用域。
                if (createScope)
                        mScopes.emplace_back();
                
                    bool alwaysReturns = false;
                    for (const auto& statement : block.Statements)
                    {
                        if (!statement)
                            continue;
                        const bool statementReturns = AnalyzeStatement(*statement);
                        if (!alwaysReturns)
                            alwaysReturns = statementReturns;
                    }
                
                    if (createScope)
                        mScopes.pop_back();
                    return alwaysReturns;
                }
            
                bool AnalyzeScopedStatement(const XJScriptStmt& statement)
                {
                    mScopes.emplace_back();
                    const bool result = AnalyzeStatement(statement);
                    mScopes.pop_back();
                    return result;
                }
            
                bool AnalyzeStatement(const XJScriptStmt& statement)
                {
                    if (const auto* block = dynamic_cast<const XJScriptBlockStmt*>(&statement))
                        return AnalyzeBlock(*block, true);
                
                    if (const auto* variable = dynamic_cast<const XJScriptVariableStmt*>(&statement))
                    {
                        if (!variable->Initializer)
                        {
                            Error(variable->Line, variable->Column, "Local variable requires an initializer");
                        }
                        else
                        {
                            const ExpressionInfo initializer = AnalyzeExpression(*variable->Initializer);
                            RequireConversion(*variable->Initializer, initializer, variable->Type,
                                              "Local initializer type does not match variable type");
                        }
                    
                        XJScriptResolvedSymbol symbol{
                            .Kind = XJScriptSymbolKind::Local,
                            .Type = variable->Type,
                            .Index = mNextLocalIndex++,
                            .Writable = true
                        };
                        if (symbol.Index >= mCurrentLocalTypes.size())
                            mCurrentLocalTypes.resize(symbol.Index + 1);
                        mCurrentLocalTypes[symbol.Index] = variable->Type;
                        mResult.LocalDeclarations[variable] = symbol;
                        if (!mScopes.back().emplace(variable->Name, symbol).second)
                            Error(variable->Line, variable->Column, "Duplicate local variable");
                        return false;
                    }
                
                    if (const auto* expression = dynamic_cast<const XJScriptExpressionStmt*>(&statement))
                    {
                        if (expression->Expression)
                            AnalyzeExpression(*expression->Expression);
                        return false;
                    }
                
                    if (const auto* ifStatement = dynamic_cast<const XJScriptIfStmt*>(&statement))
                    {
                        if (ifStatement->Condition)
                        {
                            const ExpressionInfo condition = AnalyzeExpression(*ifStatement->Condition);
                            RequireExact(*ifStatement->Condition, condition, XJScriptValueType::Bool,
                                         "If condition must have type bool");
                        }
                        const bool thenReturns = ifStatement->ThenBranch &&
                            AnalyzeScopedStatement(*ifStatement->ThenBranch);
                        const bool elseReturns = ifStatement->ElseBranch &&
                            AnalyzeScopedStatement(*ifStatement->ElseBranch);
                        return thenReturns && elseReturns;
                    }
                
                    if (const auto* whileStatement = dynamic_cast<const XJScriptWhileStmt*>(&statement))
                    {
                        bool conditionAlwaysTrue = false;
                        if (whileStatement->Condition)
                        {
                            const ExpressionInfo condition = AnalyzeExpression(*whileStatement->Condition);
                            RequireExact(*whileStatement->Condition, condition, XJScriptValueType::Bool,
                                         "While condition must have type bool");
                            if (const auto* literal =
                                    dynamic_cast<const XJScriptLiteralExpr*>(whileStatement->Condition.get()))
                            {
                                const auto* value = std::get_if<bool>(&literal->Value);
                                conditionAlwaysTrue = value && *value;
                            }
                        }
                        bool bodyReturns = false;
                        if (whileStatement->Body)
                            bodyReturns = AnalyzeScopedStatement(*whileStatement->Body);
                        // 首版没有 break；恒真循环且循环体必返时，方法不会落到底部。
                        return conditionAlwaysTrue && bodyReturns;
                    }
                
                    if (const auto* returnStatement = dynamic_cast<const XJScriptReturnStmt*>(&statement))
                    {
                        if (!mCurrentMethod)
                            return true;
                        if (mCurrentMethod->ReturnType == XJScriptValueType::Void)
                        {
                            if (returnStatement->Value)
                                Error(returnStatement->Line, returnStatement->Column,
                                      "Void method cannot return a value");
                        }
                        else if (!returnStatement->Value)
                        {
                            Error(returnStatement->Line, returnStatement->Column,
                                  "Non-void method must return a value");
                        }
                        else
                        {
                            const ExpressionInfo value = AnalyzeExpression(*returnStatement->Value);
                            RequireConversion(*returnStatement->Value, value, mCurrentMethod->ReturnType,
                                              "Return value type does not match method return type");
                        }
                        return true;
                    }
                
                    return false;
                }
            
                ExpressionInfo AnalyzeExpression(const XJScriptExpr& expression)
                {
                    // 语义结果保存在旁表中，不修改 AST，后续 Compiler 可复用同一棵树。
                    if (const auto* literal = dynamic_cast<const XJScriptLiteralExpr*>(&expression))
                        return Record(expression, LiteralType(literal->Value), false);
                
                    if (const auto* identifier = dynamic_cast<const XJScriptIdentifierExpr*>(&expression))
                    {
                        const auto symbol = ResolveSymbol(identifier->Name);
                        if (!symbol)
                        {
                            Error(identifier->Line, identifier->Column, "Unknown identifier: " + identifier->Name);
                            return {};
                        }
                        mResult.Bindings[identifier] = *symbol;
                        return Record(expression, symbol->Type, symbol->Writable);
                    }
                
                    if (const auto* unary = dynamic_cast<const XJScriptUnaryExpr*>(&expression))
                        return AnalyzeUnary(*unary);
                    if (const auto* binary = dynamic_cast<const XJScriptBinaryExpr*>(&expression))
                        return AnalyzeBinary(*binary);
                    if (const auto* assignment = dynamic_cast<const XJScriptAssignmentExpr*>(&expression))
                        return AnalyzeAssignment(*assignment);
                    if (const auto* call = dynamic_cast<const XJScriptCallExpr*>(&expression))
                        return AnalyzeCall(*call);
                
                    if (dynamic_cast<const XJScriptMemberExpr*>(&expression))
                    {
                        Error(expression.Line, expression.Column, "Member access must be part of a supported call");
                        return {};
                    }
                
                    Error(expression.Line, expression.Column, "Unsupported expression");
                    return {};
                }
            
                ExpressionInfo AnalyzeUnary(const XJScriptUnaryExpr& expression)
                {
                    if (!expression.Operand)
                        return {};
                    const ExpressionInfo operand = AnalyzeExpression(*expression.Operand);
                    if (!operand.Valid)
                        return {};
                
                    if (expression.Op == XJScriptUnaryOp::Not)
                    {
                        RequireExact(*expression.Operand, operand, XJScriptValueType::Bool,
                                     "Logical not requires bool operand");
                        return Record(expression, XJScriptValueType::Bool, false);
                    }
                
                    const bool update = expression.Op == XJScriptUnaryOp::PreIncrement ||
                                        expression.Op == XJScriptUnaryOp::PreDecrement ||
                                        expression.Op == XJScriptUnaryOp::PostIncrement ||
                                        expression.Op == XJScriptUnaryOp::PostDecrement;
                    if (!IsNumeric(operand.Type))
                        Error(expression.Line, expression.Column, "Unary numeric operator requires int or float");
                    if (update && !operand.Writable)
                        Error(expression.Line, expression.Column, "Increment target is not writable");
                    return Record(expression, operand.Type, false);
                }
            
            ExpressionInfo AnalyzeBinary(const XJScriptBinaryExpr& expression)
            {
                // 两侧先独立推导类型，再根据运算符决定结果类型和需要的数值提升。
                if (!expression.Left || !expression.Right)
                        return {};
                    const ExpressionInfo left = AnalyzeExpression(*expression.Left);
                    const ExpressionInfo right = AnalyzeExpression(*expression.Right);
                    if (!left.Valid || !right.Valid)
                        return {};
                
                    if (expression.Op == XJScriptBinaryOp::LogicalAnd ||
                        expression.Op == XJScriptBinaryOp::LogicalOr)
                    {
                        RequireExact(*expression.Left, left, XJScriptValueType::Bool,
                                     "Logical operator requires bool operands");
                        RequireExact(*expression.Right, right, XJScriptValueType::Bool,
                                     "Logical operator requires bool operands");
                        return Record(expression, XJScriptValueType::Bool, false);
                    }
                
                    if (expression.Op == XJScriptBinaryOp::Equal || expression.Op == XJScriptBinaryOp::NotEqual)
                    {
                        if (left.Type == XJScriptValueType::Void || right.Type == XJScriptValueType::Void)
                        {
                            Error(expression.Line, expression.Column, "Void expressions cannot be compared");
                            return {};
                        }
                        if (left.Type != right.Type)
                        {
                            if (left.Type == XJScriptValueType::Int && right.Type == XJScriptValueType::Float)
                                MarkConversion(*expression.Left);
                            else if (left.Type == XJScriptValueType::Float && right.Type == XJScriptValueType::Int)
                                MarkConversion(*expression.Right);
                            else
                                Error(expression.Line, expression.Column, "Equality operands have incompatible types");
                        }
                        return Record(expression, XJScriptValueType::Bool, false);
                    }
                
                    const bool comparison = expression.Op == XJScriptBinaryOp::Less ||
                                            expression.Op == XJScriptBinaryOp::LessEqual ||
                                            expression.Op == XJScriptBinaryOp::Greater ||
                                            expression.Op == XJScriptBinaryOp::GreaterEqual;
                    const auto numericType = ResolveNumericOperands(*expression.Left, left,
                                                                    *expression.Right, right);
                    if (!numericType)
                    {
                        if (expression.Op == XJScriptBinaryOp::Add &&
                            left.Type == XJScriptValueType::String && right.Type == XJScriptValueType::String)
                            return Record(expression, XJScriptValueType::String, false);
                        Error(expression.Line, expression.Column, "Operator requires compatible numeric operands");
                        return {};
                    }
                    return Record(expression, comparison ? XJScriptValueType::Bool : *numericType, false);
                }
            
            ExpressionInfo AnalyzeAssignment(const XJScriptAssignmentExpr& expression)
            {
                // 普通赋值允许 int -> float；复合赋值还必须保证运算结果可写回目标。
                if (!expression.Target || !expression.Value)
                        return {};
                    const ExpressionInfo target = AnalyzeExpression(*expression.Target);
                    const ExpressionInfo value = AnalyzeExpression(*expression.Value);
                    if (!target.Valid || !value.Valid)
                        return {};
                    if (!target.Writable)
                        Error(expression.Line, expression.Column, "Assignment target is not writable");
                
                    if (expression.Op == XJScriptAssignmentOp::Assign)
                    {
                        RequireConversion(*expression.Value, value, target.Type,
                                          "Assigned value type does not match target type");
                    }
                    else if (target.Type == XJScriptValueType::String &&
                             expression.Op == XJScriptAssignmentOp::AddAssign)
                    {
                        RequireExact(*expression.Value, value, XJScriptValueType::String,
                                     "String += requires string value");
                    }
                    else
                    {
                        if (!IsNumeric(target.Type) || !IsNumeric(value.Type))
                        {
                            Error(expression.Line, expression.Column,
                                  "Compound assignment result cannot be stored in target");
                        }
                        else if (target.Type == XJScriptValueType::Int &&
                                 value.Type == XJScriptValueType::Float)
                        {
                            Error(expression.Line, expression.Column,
                                  "Compound assignment cannot convert float result to int");
                        }
                        else if (target.Type == XJScriptValueType::Float &&
                                 value.Type == XJScriptValueType::Int)
                        {
                            MarkConversion(*expression.Value);
                        }
                    }
                    return Record(expression, target.Type, false);
                }
            
            ExpressionInfo AnalyzeCall(const XJScriptCallExpr& expression)
            {
                // 调用先确定全部实参类型，再执行脚本重载或内建函数的精确匹配。
                std::vector<XJScriptValueType> argumentTypes;
                    for (const auto& argument : expression.Arguments)
                    {
                        const ExpressionInfo info = argument ? AnalyzeExpression(*argument) : ExpressionInfo{};
                        if (!info.Valid)
                            return {};
                        argumentTypes.push_back(info.Type);
                    }
                
                    if (const auto* identifier = dynamic_cast<const XJScriptIdentifierExpr*>(expression.Callee.get()))
                        return ResolveScriptCall(expression, *identifier, argumentTypes);
                
                    if (const auto* member = dynamic_cast<const XJScriptMemberExpr*>(expression.Callee.get()))
                    {
                        const auto* object = dynamic_cast<const XJScriptIdentifierExpr*>(member->Object.get());
                        if (object && object->Name == "Transform" && member->Member == "RotateY")
                        {
                            if (ResolveSymbol(object->Name))
                            {
                                Error(object->Line, object->Column,
                                      "Transform builtin is shadowed by a script variable");
                                return {};
                            }
                            if (argumentTypes.size() != 1 || argumentTypes[0] != XJScriptValueType::Float)
                            {
                                Error(expression.Line, expression.Column,
                                      "Transform.RotateY requires exactly one float argument");
                                return {};
                            }
                            mResult.Bindings[object] = {
                                .Kind = XJScriptSymbolKind::BuiltinObject,
                                .Type = XJScriptValueType::Void,
                                .Index = 0,
                                .Writable = false
                            };
                            mResult.Calls[&expression] = {
                                .Kind = XJScriptCallKind::TransformRotateY,
                                .ReturnType = XJScriptValueType::Void,
                                .MethodIndex = 0
                            };
                            return Record(expression, XJScriptValueType::Void, false);
                        }
                        Error(expression.Line, expression.Column, "Unknown builtin member call");
                        return {};
                    }
                
                    Error(expression.Line, expression.Column, "Expression is not callable");
                    return {};
                }
            
                ExpressionInfo ResolveScriptCall(const XJScriptCallExpr& expression,
                                                 const XJScriptIdentifierExpr& identifier,
                                                 const std::vector<XJScriptValueType>& arguments)
                {
                    // 重载只允许参数类型完全匹配；这里刻意不应用 int -> float 提升。
                    const auto overloads = mMethodOverloads.find(identifier.Name);
                    if (overloads == mMethodOverloads.end())
                    {
                        Error(identifier.Line, identifier.Column, "Unknown method: " + identifier.Name);
                        return {};
                    }
                
                    std::optional<size_t> match;
                    for (size_t methodIndex : overloads->second)
                    {
                        const auto& method = mDeclaration.Methods[methodIndex];
                        if (method.Parameters.size() != arguments.size())
                            continue;
                    
                        bool exact = true;
                        for (size_t index = 0; index < arguments.size(); ++index)
                        {
                            if (method.Parameters[index].Type != arguments[index])
                            {
                                exact = false;
                                break;
                            }
                        }
                        if (!exact)
                            continue;
                        if (match)
                        {
                            Error(expression.Line, expression.Column, "Ambiguous method overload");
                            return {};
                        }
                        match = methodIndex;
                    }
                
                    if (!match)
                    {
                        Error(expression.Line, expression.Column,
                              "No exact overload matches method call: " + identifier.Name);
                        return {};
                    }
                
                    const auto& method = mDeclaration.Methods[*match];
                    mResult.Calls[&expression] = {
                        .Kind = XJScriptCallKind::ScriptMethod,
                        .ReturnType = method.ReturnType,
                        .MethodIndex = *match
                    };
                    return Record(expression, method.ReturnType, false);
                }
            
            std::optional<XJScriptResolvedSymbol> ResolveSymbol(const std::string& name) const
            {
                // 词法作用域从内向外查找；局部/参数可以遮蔽类字段。
                for (auto scope = mScopes.rbegin(); scope != mScopes.rend(); ++scope)
                    {
                        const auto found = scope->find(name);
                        if (found != scope->end())
                            return found->second;
                    }
                    const auto field = mFields.find(name);
                    return field != mFields.end() ? std::optional<XJScriptResolvedSymbol>{field->second}
                                                  : std::nullopt;
                }
            
                ExpressionInfo Record(const XJScriptExpr& expression, XJScriptValueType type, bool writable)
                {
                    mResult.ExpressionTypes[&expression] = type;
                    return {type, true, writable};
                }
            
                bool RequireExact(const XJScriptExpr& expression, const ExpressionInfo& actual,
                                  XJScriptValueType expected, const char* message)
                {
                    if (actual.Valid && actual.Type == expected)
                        return true;
                    Error(expression.Line, expression.Column, message);
                    return false;
                }
            
                bool RequireConversion(const XJScriptExpr& expression, const ExpressionInfo& actual,
                                       XJScriptValueType expected, const char* message)
                {
                    if (!actual.Valid)
                        return false;
                    if (actual.Type == expected)
                        return true;
                    if (actual.Type == XJScriptValueType::Int && expected == XJScriptValueType::Float)
                    {
                        // 转换记录挂在源表达式上，字节码编译器据此发出 IntToFloat。
                        MarkConversion(expression);
                        return true;
                    }
                    Error(expression.Line, expression.Column, message);
                    return false;
                }
            
            std::optional<XJScriptValueType> ResolveNumericOperands(
                    const XJScriptExpr& leftExpression, const ExpressionInfo& left,
                    const XJScriptExpr& rightExpression, const ExpressionInfo& right)
            {
                // 混合数值运算统一提升到 float，并把转换精确记录到 int 子表达式。
                if (!left.Valid || !right.Valid || !IsNumeric(left.Type) || !IsNumeric(right.Type))
                        return std::nullopt;
                    if (left.Type == right.Type)
                        return left.Type;
                    if (left.Type == XJScriptValueType::Int)
                        MarkConversion(leftExpression);
                    if (right.Type == XJScriptValueType::Int)
                        MarkConversion(rightExpression);
                    return XJScriptValueType::Float;
                }

                void MarkConversion(const XJScriptExpr& expression)
                {
                    mResult.Conversions[&expression] = XJScriptImplicitConversion::IntToFloat;
                }

                const XJScriptClassDecl& mDeclaration;
                XJScriptSemanticResult mResult;
                std::unordered_map<std::string, XJScriptResolvedSymbol> mFields;
                std::unordered_map<std::string, std::vector<size_t>> mMethodOverloads;
                std::vector<Scope> mScopes;
                const XJScriptMethodDecl* mCurrentMethod = nullptr;
                size_t mNextLocalIndex = 0;
                std::vector<XJScriptValueType> mCurrentLocalTypes;
            };

        

    }

    XJScriptSemanticResult XJScriptSemanticAnalyzer::Analyze(
        const XJScriptClassDecl& declaration)
    {
        Analyzer analyzer(declaration);
        return analyzer.Run();
    }
}
