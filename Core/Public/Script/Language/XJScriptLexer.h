#ifndef XJ_SCRIPT_LEXER_H
#define XJ_SCRIPT_LEXER_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
// 脚本前端的第一层：把源码字符流转换成带行列信息的 Token 流。
// Lexer 不理解类、作用域或表达式，只负责识别最小语法单元并报告词法错误。

namespace XJ
{

    enum class XJScriptTokenType
    {
        // 字面量与标识符。
        End, Identifier, Number, String,
        // 类声明、访问控制和内建类型关键字。
        Class, Public, Private, Protected,
        Void, Bool, Int, Float, StringType,
        // 控制流和布尔字面量。
        If, Else, While, Return, True, False,
        // 分隔符。
        LeftParen, RightParen, LeftBrace, RightBrace,
        Comma, Colon, Semicolon, Dot,
        LeftBracket,
        RightBracket,
        // 算术、赋值、比较和逻辑运算符。
        Plus, Minus, Star, Slash,
        PlusEqual, MinusEqual, StarEqual, SlashEqual,
        PlusPlus, MinusMinus,
        Equal, EqualEqual, Bang, BangEqual,
        Less, LessEqual, Greater, GreaterEqual,
        AndAnd, OrOr
    };

    struct XJScriptToken
    {
        XJScriptTokenType Type;
        // 保留源码原始拼写，例如 1.0f 或 "hello"，具体值由 Parser 解码。
        std::string Text;
        // Token 起始位置，供 Parser、Semantic 和编辑器诊断复用。
        size_t Line;
        size_t Column;
    };

    struct XJScriptDiagnostic
    {
        size_t Line;
        size_t Column;
        std::string Message;
    };

    struct XJScriptLexResult
    {
        // 即使发生错误也保证最后包含 End，方便后续阶段安全停止。
        std::vector<XJScriptToken> Tokens;
        std::vector<XJScriptDiagnostic> Diagnostics;

        bool IsValid() const{return Diagnostics.empty();}
    };

    class XJScriptLexer
    {
        public: 
            // 输入不需要以 '\0' 结尾；string_view 的长度是唯一边界。
            static XJScriptLexResult Scan(std::string_view source);
    };
}
#endif
