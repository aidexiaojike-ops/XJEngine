#ifndef XJ_SCRIPT_AST_H
#define XJ_SCRIPT_AST_H

#include "Script/Language/XJScriptLexer.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace XJ
{
    // 首版脚本的值类型集合。对象类型暂不允许用户声明，只通过内建绑定访问。
    enum class XJScriptValueType
    {
        Void, Bool, Int, Float, String
    };

    enum class XJScriptAccess
    {
        Public, Private, Protected
    };

    // int 固定为 int64_t，float 在编译前端使用 double 保存，避免解析阶段精度损失。
    using XJScriptLiteralValue = std::variant<bool, int64_t, double, std::string>;

    enum class XJScriptUnaryOp
    {
        Positive, Negate, Not,
        PreIncrement, PreDecrement,
        PostIncrement, PostDecrement
    };

    enum class XJScriptBinaryOp
    {
        Add, Subtract, Multiply, Divide,
        Less, LessEqual, Greater, GreaterEqual,
        Equal, NotEqual, LogicalAnd, LogicalOr
    };

    enum class XJScriptAssignmentOp
    {
        Assign, AddAssign, SubtractAssign,
        MultiplyAssign, DivideAssign
    };

    // 表达式 AST 使用 unique_ptr 形成单一所有权树。
    // 后续 Compiler 通过节点动态类型和 Semantic 旁表生成字节码。
    struct XJScriptExpr
    {
        virtual ~XJScriptExpr() = default;
        size_t Line = 1;
        size_t Column = 1;
    };

    using XJScriptExprPtr = std::unique_ptr<XJScriptExpr>;

    struct XJScriptLiteralExpr final : XJScriptExpr
    {
        XJScriptLiteralValue Value;
    };

    struct XJScriptIdentifierExpr final : XJScriptExpr
    {
        std::string Name;
    };

    struct XJScriptUnaryExpr final : XJScriptExpr
    {
        XJScriptUnaryOp Op;
        XJScriptExprPtr Operand;
    };

    struct XJScriptBinaryExpr final : XJScriptExpr
    {
        XJScriptBinaryOp Op;
        XJScriptExprPtr Left;
        XJScriptExprPtr Right;
    };

    struct XJScriptMemberExpr final : XJScriptExpr
    {
        // 例如 Transform.RotateY：Object=Transform，Member=RotateY。
        XJScriptExprPtr Object;
        std::string Member;
    };

    struct XJScriptCallExpr final : XJScriptExpr
    {
        // Callee 可以是普通方法名，也可以是 MemberExpr 形成的内建调用。
        XJScriptExprPtr Callee;
        std::vector<XJScriptExprPtr> Arguments;
    };

    struct XJScriptAssignmentExpr final : XJScriptExpr
    {
        // Target 语法上只能是 Identifier/Member，最终可写性由 Semantic 决定。
        XJScriptAssignmentOp Op;
        XJScriptExprPtr Target;
        XJScriptExprPtr Value;
    };

    // 语句 AST。Block 拥有子语句；if/while 分支自身拥有独立作用域。
    struct XJScriptStmt
    {
        virtual ~XJScriptStmt() = default;
        size_t Line = 1;
        size_t Column = 1;
    };

    using XJScriptStmtPtr = std::unique_ptr<XJScriptStmt>;

    struct XJScriptBlockStmt final : XJScriptStmt
    {
        std::vector<XJScriptStmtPtr> Statements;
    };

    struct XJScriptExpressionStmt final : XJScriptStmt
    {
        XJScriptExprPtr Expression;
    };

    struct XJScriptVariableStmt final : XJScriptStmt
    {
        // 当前语言要求局部变量必须有 Initializer。
        XJScriptValueType Type = XJScriptValueType::Int;
        std::string Name;
        XJScriptExprPtr Initializer;
    };

    struct XJScriptIfStmt final : XJScriptStmt
    {
        XJScriptExprPtr Condition;
        XJScriptStmtPtr ThenBranch;
        XJScriptStmtPtr ElseBranch;
    };

    struct XJScriptWhileStmt final : XJScriptStmt
    {
        XJScriptExprPtr Condition;
        XJScriptStmtPtr Body;
    };

    struct XJScriptReturnStmt final : XJScriptStmt
    {
        XJScriptExprPtr Value;
    };

    struct XJScriptParameter
    {
        XJScriptValueType Type = XJScriptValueType::Void;
        std::string Name;
        size_t Line = 1;
        size_t Column = 1;
    };

    struct XJScriptFieldDecl
    {
        // public 字段以后暴露到 Inspector；首版默认值只允许可序列化字面量。
        XJScriptAccess Access = XJScriptAccess::Private;
        XJScriptValueType Type = XJScriptValueType::Int;
        // public 字段必须非 0。
        uint64_t StableId = 0;
        std::string Name;
        std::optional<XJScriptLiteralValue> DefaultValue;
        size_t Line = 1;
        size_t Column = 1;
    };

    struct XJScriptMethodDecl
    {
        // 方法体已经是语句 AST，不再保留原始 Body Token。
        XJScriptAccess Access = XJScriptAccess::Private;
        XJScriptValueType ReturnType = XJScriptValueType::Void;
        std::string Name;
        std::vector<XJScriptParameter> Parameters;
        std::unique_ptr<XJScriptBlockStmt> Body;
        size_t Line = 1;
        size_t Column = 1;
    };

    struct XJScriptClassDecl
    {
        std::string Name;
        std::string BaseName;
        std::vector<XJScriptFieldDecl> Fields;
        std::vector<XJScriptMethodDecl> Methods;
        size_t Line = 1;
        size_t Column = 1;
    };

    struct XJScriptParseResult
    {
        // 即使存在部分 Parser 诊断，也可能保留 Class 供编辑器展示；IsValid 才可进入 Semantic。
        std::unique_ptr<XJScriptClassDecl> Class;
        std::vector<XJScriptDiagnostic> Diagnostics;

        bool IsValid() const
        {
            return Class != nullptr && Diagnostics.empty();
        }
    };
}

#endif
