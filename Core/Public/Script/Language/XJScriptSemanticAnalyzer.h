#ifndef XJ_SCRIPT_SEMANTIC_ANALYZER_H
#define XJ_SCRIPT_SEMANTIC_ANALYZER_H

#include "Script/Language/XJScriptAst.h"

#include <cstddef>
#include <unordered_map>
#include <vector>
// 脚本前端第三层：在 AST 结构正确后解析符号、类型、重载和内建调用。
// Analyzer 不改写 AST，而是把结果保存到以 AST 节点指针为键的旁表中。

namespace XJ
{
    enum class XJScriptSymbolKind
    {
        // 三种用户变量分别使用独立索引空间，BuiltinObject 表示 Transform 等引擎对象。
        Field,
        Parameter,
        Local,
        BuiltinObject
    };

    enum class XJScriptImplicitConversion
    {
        None,
        // 当前唯一隐式转换；float -> int 始终不允许。
        IntToFloat
    };

    enum class XJScriptCallKind
    {
        ScriptMethod,
        TransformRotateY
    };

    struct XJScriptResolvedSymbol
    {
        XJScriptSymbolKind Kind;
        XJScriptValueType Type;
        size_t Index = 0;
        bool Writable = true;
    };

    struct XJScriptResolvedCall
    {
        XJScriptCallKind Kind;
        XJScriptValueType ReturnType;
        size_t MethodIndex = 0;
    };

    struct XJScriptSemanticResult
    {
        std::vector<XJScriptDiagnostic>
            Diagnostics;

        std::unordered_map<
            const XJScriptExpr*,
            XJScriptValueType>
            ExpressionTypes;

        std::unordered_map<
            const XJScriptVariableStmt*,
            XJScriptResolvedSymbol> LocalDeclarations;

        std::unordered_map<
            const XJScriptMethodDecl*,
            size_t> MethodLocalCounts;
        
        std::unordered_map<const XJScriptMethodDecl*, std::vector<XJScriptValueType>> MethodLocalTypes;

        // Compiler 遇到被标记的源表达式时，应在其结果后发出 IntToFloat 指令。
        std::unordered_map<
            const XJScriptExpr*,
            XJScriptImplicitConversion>
            Conversions;

        // Identifier 到字段/参数/局部 slot 的稳定绑定，Compiler 不再按名称查找。
        std::unordered_map<
            const XJScriptIdentifierExpr*,
            XJScriptResolvedSymbol>
            Bindings;

        // Call 到具体脚本方法或内建函数的绑定，重载选择在本阶段完成。
        std::unordered_map<
            const XJScriptCallExpr*,
            XJScriptResolvedCall>
            Calls;


        bool IsValid() const
        {
            return Diagnostics.empty();
        }
    };

    class XJScriptSemanticAnalyzer
    {
        public:
            // 前置条件：declaration 来自无 Parser 诊断的 AST。
            // AST 必须保持存活且不可修改，直到所有旁表被 Compiler 消费完毕。
            static XJScriptSemanticResult Analyze(
                const XJScriptClassDecl& declaration);
    };
}

#endif
