#include "Script/Language/XJScriptParser.h"

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace XJ
{
    namespace
    {
        using TokenType = XJScriptTokenType;

        struct ParsedAttributes
        {
            uint64_t FieldId = 0;
            bool HasFieldId = false;
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

        std::string BuildMethodSignature(const XJScriptMethodDecl& method)
        {
            // 返回类型不参与重载；Move(int) 与 Move(float) 是两个不同签名。
            std::string signature = method.Name + "(";
            for (const auto& parameter : method.Parameters)
                signature.push_back(TypeCode(parameter.Type));
            signature.push_back(')');
            return signature;
        }

        class Parser
        {
        public:
            explicit Parser(const std::vector<XJScriptToken>& tokens)
                : mTokens(tokens)
            {
            }

            XJScriptParseResult Run()
            {
                // 一个脚本文件当前只允许声明一个 ScriptBehaviour 派生类。
                const XJScriptToken* classToken = Consume(TokenType::Class, "Expected 'class'");
                const XJScriptToken* className = Consume(TokenType::Identifier, "Expected class name");
                Consume(TokenType::Colon, "Expected ':' after class name");
                const XJScriptToken* baseName = Consume(TokenType::Identifier, "Expected base class name");

                if (!classToken || !className || !baseName)
                    return std::move(mResult);

                auto declaration = std::make_unique<XJScriptClassDecl>();
                declaration->Name = className->Text;
                declaration->BaseName = baseName->Text;
                declaration->Line = classToken->Line;
                declaration->Column = classToken->Column;

                if (declaration->BaseName != "ScriptBehaviour")
                    Error(*baseName, "Script class must inherit ScriptBehaviour");

                if (!Consume(TokenType::LeftBrace, "Expected '{' before class body"))
                    return std::move(mResult);

                while (!Check(TokenType::RightBrace) && !Check(TokenType::End))
                {
                    if (ParseAccessLabel())
                        continue;
                    const ParsedAttributes attributes = ParseAttributes();
                    ParseMember(*declaration, attributes);
                }

                Consume(TokenType::RightBrace, "Expected '}' after class body");
                // 脚本文件允许省略最外层 class 后的分号；保留对 C++ 风格 `};` 的兼容。
                Match(TokenType::Semicolon);

                if (!Check(TokenType::End))
                    Error(Current(), "Unexpected token after class declaration");

                mResult.Class = std::move(declaration);
                return std::move(mResult);
            }

        private:
            const XJScriptToken& Current() const
            {
                return mTokens[mIndex < mTokens.size() ? mIndex : mTokens.size() - 1];
            }

            const XJScriptToken& Previous() const
            {
                return mTokens[mIndex == 0 ? 0 : mIndex - 1];
            }

            const XJScriptToken& Advance()
            {
                const XJScriptToken& token = Current();
                if (mIndex < mTokens.size())
                    ++mIndex;
                return token;
            }

            bool Check(TokenType type) const { return Current().Type == type; }

            bool Match(TokenType type)
            {
                if (!Check(type))
                    return false;
                Advance();
                return true;
            }

            const XJScriptToken* Consume(TokenType type, const char* message)
            {
                if (Check(type))
                    return &Advance();
                Error(Current(), message);
                return nullptr;
            }

            void Error(const XJScriptToken& token, std::string message)
            {
                mResult.Diagnostics.push_back({token.Line, token.Column, std::move(message)});
            }

            std::optional<uint64_t> ParseFieldIdString(const XJScriptToken& token)
            {
                if (token.Text.size() < 2)
                {
                    Error(token, "Invalid FieldId");
                    return std::nullopt;
                }
                const std::string_view value(token.Text.data() + 1, token.Text.size() - 2);
                uint64_t id = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), id);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || id == 0)
                {
                    Error(token, "FieldId must be a non-zero uint64 string");
                    return std::nullopt;
                }
                return id;
            }

            ParsedAttributes ParseAttributes()
            {
                ParsedAttributes result;
                while (Check(TokenType::LeftBracket))
                {
                    Consume(TokenType::LeftBracket, "Expected '['");
                    Consume(TokenType::LeftBracket, "Expected second '['");
                    const XJScriptToken* name = Consume(TokenType::Identifier, "Expected attribute name");
                    Consume(TokenType::LeftParen, "Expected '(' after attribute");
                    const XJScriptToken* value = Consume(TokenType::String, "Expected attribute string");
                    Consume(TokenType::RightParen, "Expected ')' after attribute");
                    Consume(TokenType::RightBracket, "Expected first ']'");
                    Consume(TokenType::RightBracket, "Expected second ']'");
                    if (!name || !value)
                        continue;
                    if (name->Text != "FieldId")
                    {
                        Error(*name, "Unsupported script attribute");
                        continue;
                    }
                    if (result.HasFieldId)
                    {
                        Error(*name, "Duplicate FieldId attribute");
                        continue;
                    }
                    const auto id = ParseFieldIdString(*value);
                    if (id)
                    {
                        result.FieldId = *id;
                        result.HasFieldId = true;
                    }
                }
                return result;
            }

            template<typename T>
            std::unique_ptr<T> MakeNode(const XJScriptToken& token)
            {
                auto node = std::make_unique<T>();
                node->Line = token.Line;
                node->Column = token.Column;
                return node;
            }

            bool ParseAccessLabel()
            {
                XJScriptAccess access{};
                if (Match(TokenType::Public))
                    access = XJScriptAccess::Public;
                else if (Match(TokenType::Private))
                    access = XJScriptAccess::Private;
                else if (Match(TokenType::Protected))
                    access = XJScriptAccess::Protected;
                else
                    return false;

                Consume(TokenType::Colon, "Expected ':' after access modifier");
                mCurrentAccess = access;
                return true;
            }

            bool IsValueTypeToken(TokenType type) const
            {
                return type == TokenType::Bool || type == TokenType::Int ||
                       type == TokenType::Float || type == TokenType::StringType;
            }

            std::optional<XJScriptValueType> ParseType(bool allowVoid)
            {
                if (Match(TokenType::Bool)) return XJScriptValueType::Bool;
                if (Match(TokenType::Int)) return XJScriptValueType::Int;
                if (Match(TokenType::Float)) return XJScriptValueType::Float;
                if (Match(TokenType::StringType)) return XJScriptValueType::String;
                if (Match(TokenType::Void))
                {
                    if (allowVoid)
                        return XJScriptValueType::Void;
                    Error(Previous(), "Void is not valid here");
                    return std::nullopt;
                }
                Error(Current(), "Expected type name");
                return std::nullopt;
            }

            void ParseMember(XJScriptClassDecl& declaration, const ParsedAttributes& attributes)
            {
                // 成员共同前缀为“类型 名称”；名称后的 '(' 决定它是方法还是字段。
                const size_t memberStart = mIndex;
                auto type = ParseType(true);
                if (!type)
                {
                    SynchronizeMember();
                    return;
                }

                const XJScriptToken* name = Consume(TokenType::Identifier, "Expected member name");
                if (!name)
                {
                    SynchronizeMember();
                    return;
                }

                if (Match(TokenType::LeftParen))
                {
                    if (attributes.HasFieldId)
                        Error(*name, "FieldId cannot be applied to a method");
                    ParseMethod(declaration, *type, *name);
                    return;
                }

                if (*type == XJScriptValueType::Void)
                    Error(mTokens[memberStart], "Field cannot have type void");
                ParseField(declaration, *type, *name,  attributes);
            }

            void ParseField(XJScriptClassDecl& declaration, XJScriptValueType type, const XJScriptToken& name, const ParsedAttributes& attributes)
            {
                XJScriptFieldDecl field;
                field.Access = mCurrentAccess;
                field.Type = type;
                field.StableId = attributes.FieldId;
                field.Name = name.Text;
                field.Line = name.Line;
                field.Column = name.Column;

                if (field.Access ==
                        XJScriptAccess::Public &&
                    !attributes.HasFieldId)
                {
                    Error(
                        name,
                        "Public field requires "
                        "a FieldId attribute");
                }

                if (attributes.HasFieldId &&
                    !mFieldIds.insert(
                        attributes.FieldId).second)
                {
                    Error(
                        name,
                        "Duplicate FieldId");
                }

                if (Match(TokenType::Equal))
                    field.DefaultValue = ParseFieldLiteral(type);
                else
                    Error(name, "Field requires an initializer");
                Consume(TokenType::Semicolon, "Expected ';' after field declaration");

                if (!mFieldNames.insert(field.Name).second)
                    Error(name, "Duplicate field declaration");
                declaration.Fields.push_back(std::move(field));
            }

            std::optional<XJScriptLiteralValue> ParseFieldLiteral(XJScriptValueType expectedType)
            {
                // 字段默认值在 Parser 阶段限制为字面量，确保未来可直接序列化到场景。
                const bool negative = Match(TokenType::Minus);
                const XJScriptToken token = Current();

                if (expectedType == XJScriptValueType::Bool)
                {
                    if (negative) Error(token, "Boolean literal cannot be negative");
                    if (Match(TokenType::True)) return XJScriptLiteralValue{true};
                    if (Match(TokenType::False)) return XJScriptLiteralValue{false};
                }
                else if (expectedType == XJScriptValueType::String)
                {
                    if (negative) Error(token, "String literal cannot be negative");
                    if (Match(TokenType::String)) return XJScriptLiteralValue{DecodeString(token.Text)};
                }
                else if ((expectedType == XJScriptValueType::Int || expectedType == XJScriptValueType::Float) &&
                         Match(TokenType::Number))
                {
                    auto value = ParseNumber(token);
                    if (!value)
                    {
                        Error(token, "Invalid numeric literal");
                        return std::nullopt;
                    }
                    if (expectedType == XJScriptValueType::Int && !std::holds_alternative<int64_t>(*value))
                    {
                        Error(token, "Integer field requires integer literal");
                        return std::nullopt;
                    }
                    if (negative)
                    {
                        if (auto* integer = std::get_if<int64_t>(&*value)) *integer = -*integer;
                        if (auto* real = std::get_if<double>(&*value)) *real = -*real;
                    }
                    if (expectedType == XJScriptValueType::Float && std::holds_alternative<int64_t>(*value))
                        // 保留 C++ 风格的安全提升：float speed = 1。
                        return XJScriptLiteralValue{static_cast<double>(std::get<int64_t>(*value))};
                    return value;
                }

                Error(token, "Field initializer does not match field type");
                if (!Check(TokenType::Semicolon) && !Check(TokenType::End)) Advance();
                return std::nullopt;
            }

            static std::optional<XJScriptLiteralValue> ParseNumber(const XJScriptToken& token)
            {
                std::string text = token.Text;
                const bool floating = text.find_first_of(".eEfF") != std::string::npos;
                if (!text.empty() && (text.back() == 'f' || text.back() == 'F')) text.pop_back();

                if (!floating)
                {
                    int64_t value = 0;
                    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
                    if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size())
                        return XJScriptLiteralValue{value};
                    return std::nullopt;
                }

                double value = 0.0;
                const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
                                                    std::chars_format::general);
                if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(value))
                    return XJScriptLiteralValue{value};
                return std::nullopt;
            }

            static std::string DecodeString(const std::string& tokenText)
            {
                std::string result;
                for (size_t index = 1; index + 1 < tokenText.size(); ++index)
                {
                    if (tokenText[index] != '\\')
                    {
                        result.push_back(tokenText[index]);
                        continue;
                    }
                    switch (tokenText[++index])
                    {
                        case '\\': result.push_back('\\'); break;
                        case '"': result.push_back('"'); break;
                        case 'n': result.push_back('\n'); break;
                        case 'r': result.push_back('\r'); break;
                        case 't': result.push_back('\t'); break;
                        default: break;
                    }
                }
                return result;
            }

            void ParseMethod(XJScriptClassDecl& declaration, XJScriptValueType returnType,
                             const XJScriptToken& name)
            {
                XJScriptMethodDecl method;
                method.Access = mCurrentAccess;
                method.ReturnType = returnType;
                method.Name = name.Text;
                method.Line = name.Line;
                method.Column = name.Column;

                std::unordered_set<std::string> parameterNames;
                if (!Check(TokenType::RightParen))
                {
                    do
                    {
                        auto parameterType = ParseType(false);
                        const XJScriptToken* parameterName = Consume(TokenType::Identifier,
                                                                    "Expected parameter name");
                        if (!parameterType || !parameterName)
                        {
                            SynchronizeMember();
                            return;
                        }
                        if (!parameterNames.insert(parameterName->Text).second)
                            Error(*parameterName, "Duplicate parameter name");
                        method.Parameters.push_back({*parameterType, parameterName->Text,
                                                     parameterName->Line, parameterName->Column});
                    }
                    while (Match(TokenType::Comma));
                }

                if (!Consume(TokenType::RightParen, "Expected ')' after parameters"))
                {
                    SynchronizeMember();
                    return;
                }

                const XJScriptToken* openingBrace = Consume(TokenType::LeftBrace,
                                                            "Expected '{' before method body");
                if (!openingBrace)
                {
                    SynchronizeMember();
                    return;
                }
                method.Body = ParseBlock(*openingBrace);

                const std::string signature = BuildMethodSignature(method);
                if (!mMethodSignatures.insert(signature).second)
                    Error(name, "Duplicate method declaration");

                ValidateLifecycle(method);
                declaration.Methods.push_back(std::move(method));
            }

            // 调用本函数时，左花括号已经被消费。
            std::unique_ptr<XJScriptBlockStmt> ParseBlock(const XJScriptToken& openingBrace)
            {
                // 左花括号已由调用方消费；本函数负责消费对应的右花括号。
                auto block = MakeNode<XJScriptBlockStmt>(openingBrace);
                while (!Check(TokenType::RightBrace) && !Check(TokenType::End))
                {
                    auto statement = ParseStatement();
                    if (statement)
                        block->Statements.push_back(std::move(statement));
                    else
                        SynchronizeStatement();
                }
                Consume(TokenType::RightBrace, "Expected '}' after block");
                return block;
            }

            XJScriptStmtPtr ParseStatement()
            {
                // 先识别有明确起始关键字的语句，其余内容按表达式语句处理。
                if (Match(TokenType::LeftBrace)) return ParseBlock(Previous());
                if (Match(TokenType::If)) return ParseIfStatement(Previous());
                if (Match(TokenType::While)) return ParseWhileStatement(Previous());
                if (Match(TokenType::Return)) return ParseReturnStatement(Previous());
                if (IsValueTypeToken(Current().Type)) return ParseVariableStatement();
                return ParseExpressionStatement();
            }

            XJScriptStmtPtr ParseVariableStatement()
            {
                const XJScriptToken typeToken = Current();
                auto type = ParseType(false);
                const XJScriptToken* name = Consume(TokenType::Identifier, "Expected local variable name");
                if (!type || !name) return nullptr;

                auto statement = MakeNode<XJScriptVariableStmt>(typeToken);
                statement->Type = *type;
                statement->Name = name->Text;
                if (Match(TokenType::Equal))
                    statement->Initializer = ParseExpression();
                else
                    Error(*name, "Local variable requires an initializer");
                Consume(TokenType::Semicolon, "Expected ';' after local variable");
                return statement;
            }

            XJScriptStmtPtr ParseIfStatement(const XJScriptToken& keyword)
            {
                auto statement = MakeNode<XJScriptIfStmt>(keyword);
                Consume(TokenType::LeftParen, "Expected '(' after 'if'");
                statement->Condition = ParseExpression();
                Consume(TokenType::RightParen, "Expected ')' after if condition");
                statement->ThenBranch = ParseStatement();
                if (Match(TokenType::Else)) statement->ElseBranch = ParseStatement();
                return statement;
            }

            XJScriptStmtPtr ParseWhileStatement(const XJScriptToken& keyword)
            {
                auto statement = MakeNode<XJScriptWhileStmt>(keyword);
                Consume(TokenType::LeftParen, "Expected '(' after 'while'");
                statement->Condition = ParseExpression();
                Consume(TokenType::RightParen, "Expected ')' after while condition");
                statement->Body = ParseStatement();
                return statement;
            }

            XJScriptStmtPtr ParseReturnStatement(const XJScriptToken& keyword)
            {
                auto statement = MakeNode<XJScriptReturnStmt>(keyword);
                if (!Check(TokenType::Semicolon)) statement->Value = ParseExpression();
                Consume(TokenType::Semicolon, "Expected ';' after return statement");
                return statement;
            }

            XJScriptStmtPtr ParseExpressionStatement()
            {
                const XJScriptToken start = Current();
                auto expression = ParseExpression();
                if (!expression) return nullptr;
                auto statement = MakeNode<XJScriptExpressionStmt>(start);
                statement->Expression = std::move(expression);
                Consume(TokenType::Semicolon, "Expected ';' after expression");
                return statement;
            }

            XJScriptExprPtr ParseExpression() { return ParseAssignment(); }

            // 赋值右结合：a = b += c 解析为 a = (b += c)。
            XJScriptExprPtr ParseAssignment()
            {
                // 先解析左侧，再递归解析右侧，使 a = b = c 按 a = (b = c) 结合。
                auto target = ParseLogicalOr();
                if (!target) return nullptr;

                XJScriptAssignmentOp op{};
                bool assignment = true;
                if (Match(TokenType::Equal)) op = XJScriptAssignmentOp::Assign;
                else if (Match(TokenType::PlusEqual)) op = XJScriptAssignmentOp::AddAssign;
                else if (Match(TokenType::MinusEqual)) op = XJScriptAssignmentOp::SubtractAssign;
                else if (Match(TokenType::StarEqual)) op = XJScriptAssignmentOp::MultiplyAssign;
                else if (Match(TokenType::SlashEqual)) op = XJScriptAssignmentOp::DivideAssign;
                else assignment = false;

                if (!assignment) return target;
                const XJScriptToken operatorToken = Previous();
                if (!IsAssignable(*target)) Error(operatorToken, "Invalid assignment target");

                auto expression = MakeNode<XJScriptAssignmentExpr>(operatorToken);
                expression->Op = op;
                expression->Target = std::move(target);
                expression->Value = ParseAssignment();
                return expression;
            }

            XJScriptExprPtr ParseLogicalOr()
            {
                auto expression = ParseLogicalAnd();
                while (Match(TokenType::OrOr))
                    expression = MakeBinary(Previous(), XJScriptBinaryOp::LogicalOr,
                                            std::move(expression), ParseLogicalAnd());
                return expression;
            }

            XJScriptExprPtr ParseLogicalAnd()
            {
                auto expression = ParseEquality();
                while (Match(TokenType::AndAnd))
                    expression = MakeBinary(Previous(), XJScriptBinaryOp::LogicalAnd,
                                            std::move(expression), ParseEquality());
                return expression;
            }

            XJScriptExprPtr ParseEquality()
            {
                auto expression = ParseComparison();
                while (Check(TokenType::EqualEqual) || Check(TokenType::BangEqual))
                {
                    const XJScriptToken token = Advance();
                    expression = MakeBinary(token,
                        token.Type == TokenType::EqualEqual ? XJScriptBinaryOp::Equal
                                                            : XJScriptBinaryOp::NotEqual,
                        std::move(expression), ParseComparison());
                }
                return expression;
            }

            XJScriptExprPtr ParseComparison()
            {
                auto expression = ParseAdditive();
                while (Check(TokenType::Less) || Check(TokenType::LessEqual) ||
                       Check(TokenType::Greater) || Check(TokenType::GreaterEqual))
                {
                    const XJScriptToken token = Advance();
                    XJScriptBinaryOp op = XJScriptBinaryOp::Less;
                    if (token.Type == TokenType::LessEqual) op = XJScriptBinaryOp::LessEqual;
                    else if (token.Type == TokenType::Greater) op = XJScriptBinaryOp::Greater;
                    else if (token.Type == TokenType::GreaterEqual) op = XJScriptBinaryOp::GreaterEqual;
                    expression = MakeBinary(token, op, std::move(expression), ParseAdditive());
                }
                return expression;
            }

            XJScriptExprPtr ParseAdditive()
            {
                auto expression = ParseMultiplicative();
                while (Check(TokenType::Plus) || Check(TokenType::Minus))
                {
                    const XJScriptToken token = Advance();
                    expression = MakeBinary(token,
                        token.Type == TokenType::Plus ? XJScriptBinaryOp::Add
                                                      : XJScriptBinaryOp::Subtract,
                        std::move(expression), ParseMultiplicative());
                }
                return expression;
            }

            XJScriptExprPtr ParseMultiplicative()
            {
                auto expression = ParseUnary();
                while (Check(TokenType::Star) || Check(TokenType::Slash))
                {
                    const XJScriptToken token = Advance();
                    expression = MakeBinary(token,
                        token.Type == TokenType::Star ? XJScriptBinaryOp::Multiply
                                                      : XJScriptBinaryOp::Divide,
                        std::move(expression), ParseUnary());
                }
                return expression;
            }

            XJScriptExprPtr ParseUnary()
            {
                // 前置运算符递归调用自身，因此 -- -value 等组合自然按从右到左解析。
                if (Check(TokenType::Plus) || Check(TokenType::Minus) || Check(TokenType::Bang) ||
                    Check(TokenType::PlusPlus) || Check(TokenType::MinusMinus))
                {
                    const XJScriptToken token = Advance();
                    auto operand = ParseUnary();
                    if (!operand) return nullptr;

                    XJScriptUnaryOp op = XJScriptUnaryOp::Positive;
                    if (token.Type == TokenType::Minus) op = XJScriptUnaryOp::Negate;
                    else if (token.Type == TokenType::Bang) op = XJScriptUnaryOp::Not;
                    else if (token.Type == TokenType::PlusPlus) op = XJScriptUnaryOp::PreIncrement;
                    else if (token.Type == TokenType::MinusMinus) op = XJScriptUnaryOp::PreDecrement;

                    if ((op == XJScriptUnaryOp::PreIncrement || op == XJScriptUnaryOp::PreDecrement) &&
                        !IsAssignable(*operand))
                        Error(token, "Invalid increment target");

                    auto expression = MakeNode<XJScriptUnaryExpr>(token);
                    expression->Op = op;
                    expression->Operand = std::move(operand);
                    return expression;
                }
                return ParsePostfix();
            }

            XJScriptExprPtr ParsePostfix()
            {
                // Postfix 循环支持连续调用链，例如 object.method(a).field。
                auto expression = ParsePrimary();
                if (!expression) return nullptr;

                while (true)
                {
                    if (Match(TokenType::Dot))
                    {
                        const XJScriptToken* member = Consume(TokenType::Identifier,
                                                              "Expected member name after '.'");
                        if (!member) return expression;
                        auto access = MakeNode<XJScriptMemberExpr>(*member);
                        access->Object = std::move(expression);
                        access->Member = member->Text;
                        expression = std::move(access);
                    }
                    else if (Match(TokenType::LeftParen))
                    {
                        const XJScriptToken openParen = Previous();
                        auto call = MakeNode<XJScriptCallExpr>(openParen);
                        call->Callee = std::move(expression);
                        if (!Check(TokenType::RightParen))
                        {
                            do { call->Arguments.push_back(ParseExpression()); }
                            while (Match(TokenType::Comma));
                        }
                        Consume(TokenType::RightParen, "Expected ')' after arguments");
                        expression = std::move(call);
                    }
                    else if (Check(TokenType::PlusPlus) || Check(TokenType::MinusMinus))
                    {
                        const XJScriptToken token = Advance();
                        if (!IsAssignable(*expression)) Error(token, "Invalid increment target");
                        auto update = MakeNode<XJScriptUnaryExpr>(token);
                        update->Op = token.Type == TokenType::PlusPlus
                            ? XJScriptUnaryOp::PostIncrement : XJScriptUnaryOp::PostDecrement;
                        update->Operand = std::move(expression);
                        expression = std::move(update);
                    }
                    else break;
                }
                return expression;
            }

            XJScriptExprPtr ParsePrimary()
            {
                const XJScriptToken token = Current();
                if (Match(TokenType::True) || Match(TokenType::False))
                {
                    auto literal = MakeNode<XJScriptLiteralExpr>(token);
                    literal->Value = token.Type == TokenType::True;
                    return literal;
                }
                if (Match(TokenType::Number))
                {
                    auto value = ParseNumber(token);
                    if (!value)
                    {
                        Error(token, "Invalid numeric literal");
                        return nullptr;
                    }
                    auto literal = MakeNode<XJScriptLiteralExpr>(token);
                    literal->Value = std::move(*value);
                    return literal;
                }
                if (Match(TokenType::String))
                {
                    auto literal = MakeNode<XJScriptLiteralExpr>(token);
                    literal->Value = DecodeString(token.Text);
                    return literal;
                }
                if (Match(TokenType::Identifier))
                {
                    auto identifier = MakeNode<XJScriptIdentifierExpr>(token);
                    identifier->Name = token.Text;
                    return identifier;
                }
                if (Match(TokenType::LeftParen))
                {
                    auto expression = ParseExpression();
                    Consume(TokenType::RightParen, "Expected ')' after expression");
                    return expression;
                }

                Error(token, "Expected expression");
                if (!Check(TokenType::End)) Advance();
                return nullptr;
            }

            XJScriptExprPtr MakeBinary(const XJScriptToken& token, XJScriptBinaryOp op,
                                       XJScriptExprPtr left, XJScriptExprPtr right)
            {
                auto expression = MakeNode<XJScriptBinaryExpr>(token);
                expression->Op = op;
                expression->Left = std::move(left);
                expression->Right = std::move(right);
                return expression;
            }

            static bool IsAssignable(const XJScriptExpr& expression)
            {
                return dynamic_cast<const XJScriptIdentifierExpr*>(&expression) != nullptr ||
                       dynamic_cast<const XJScriptMemberExpr*>(&expression) != nullptr;
            }

            void ValidateLifecycle(const XJScriptMethodDecl& method)
            {
                const bool noArgumentLifecycle = method.Name == "OnCreate" || method.Name == "OnDestroy";
                const bool deltaLifecycle = method.Name == "OnUpdate" || method.Name == "OnFixedUpdate";
                if (!noArgumentLifecycle && !deltaLifecycle) return;

                if (method.Access != XJScriptAccess::Public)
                    ErrorAt(method, "Lifecycle method must be public");
                if (method.ReturnType != XJScriptValueType::Void)
                    ErrorAt(method, "Lifecycle method must return void");

                const size_t expected = deltaLifecycle ? 1 : 0;
                if (method.Parameters.size() != expected)
                    ErrorAt(method, "Lifecycle method has invalid parameter count");
                else if (deltaLifecycle && method.Parameters[0].Type != XJScriptValueType::Float)
                    ErrorAt(method, "Lifecycle parameter must have type float");
            }

            void ErrorAt(const XJScriptMethodDecl& method, std::string message)
            {
                mResult.Diagnostics.push_back({method.Line, method.Column, std::move(message)});
            }

            void SynchronizeStatement()
            {
                // 当前语句失败后跳到下一个可靠边界，避免同一错误产生整页级联诊断。
                while (!Check(TokenType::End) && !Check(TokenType::RightBrace))
                {
                    if (Match(TokenType::Semicolon)) return;
                    if (Check(TokenType::If) || Check(TokenType::While) || Check(TokenType::Return) ||
                        Check(TokenType::LeftBrace) || IsValueTypeToken(Current().Type)) return;
                    Advance();
                }
            }

            void SynchronizeMember()
            {
                while (!Check(TokenType::End) && !Check(TokenType::RightBrace))
                {
                    if (Match(TokenType::Semicolon)) return;
                    if (Check(TokenType::Public) || Check(TokenType::Private) ||
                        Check(TokenType::Protected) || Check(TokenType::Void) ||
                        IsValueTypeToken(Current().Type)) return;
                    Advance();
                }
            }

            const std::vector<XJScriptToken>& mTokens;
            size_t mIndex = 0;
            XJScriptAccess mCurrentAccess = XJScriptAccess::Private;
            std::unordered_set<std::string> mFieldNames;
            std::unordered_set<uint64_t> mFieldIds;
            std::unordered_set<std::string> mMethodSignatures;
            XJScriptParseResult mResult;
        };
    }

    XJScriptParseResult XJScriptParser::Parse(std::string_view source)
    {
        XJScriptLexResult lexResult = XJScriptLexer::Scan(source);
        if (!lexResult.IsValid())
        {
            XJScriptParseResult result;
            result.Diagnostics = std::move(lexResult.Diagnostics);
            return result;
        }
        return ParseTokens(lexResult.Tokens);
    }

    XJScriptParseResult XJScriptParser::ParseTokens(const std::vector<XJScriptToken>& tokens)
    {
        if (tokens.empty() || tokens.back().Type != XJScriptTokenType::End)
        {
            XJScriptParseResult result;
            result.Diagnostics.push_back({1, 1, "Token stream must end with End token"});
            return result;
        }
        Parser parser(tokens);
        return parser.Run();
    }
}
