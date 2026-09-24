#ifndef XJ_SCRIPT_PARSER_H
#define XJ_SCRIPT_PARSER_H

#include "Script/Language/XJScriptAst.h"

#include <string_view>
#include <vector>

namespace XJ
{
    class XJScriptParser
    {
        public:
            // 常规入口：先运行 Lexer，再把 Token 组织成声明、语句和表达式 AST。
            // Lexer 失败时不继续解析，避免一个坏 Token 产生大量级联语法错误。
            static XJScriptParseResult Parse(std::string_view source);
        
            // Token 入口：供单元测试及未来的增量编译缓存使用。
            // 调用方必须保证 Token 流以 End 结束。
            static XJScriptParseResult ParseTokens(
                const std::vector<XJScriptToken>& tokens);
    };
}

#endif
