#include "Script/Language/XJScriptLexer.h"

#include <charconv>
#include <cctype>
#include <cmath>
#include <string>
#include <system_error>
#include <unordered_map>

namespace XJ
{
    XJScriptLexResult XJScriptLexer::Scan(std::string_view source)
    {
        // Lexer 只负责把字符流切成 Token，并记录准确位置；不判断语法和类型。
        XJScriptLexResult result;
        size_t index = 0;
        size_t line = 1;
        size_t column = 1;

        const std::unordered_map<std::string_view, XJScriptTokenType> keywords{
            {"class", XJScriptTokenType::Class},
            {"public", XJScriptTokenType::Public},
            {"private", XJScriptTokenType::Private},
            {"protected", XJScriptTokenType::Protected},
            {"void", XJScriptTokenType::Void},
            {"bool", XJScriptTokenType::Bool},
            {"int", XJScriptTokenType::Int},
            {"float", XJScriptTokenType::Float},
            {"string", XJScriptTokenType::StringType},
            {"if", XJScriptTokenType::If},
            {"else", XJScriptTokenType::Else},
            {"while", XJScriptTokenType::While},
            {"return", XJScriptTokenType::Return},
            {"true", XJScriptTokenType::True},
            {"false", XJScriptTokenType::False}
        };

        auto digit = [](char ch) {
            return std::isdigit(static_cast<unsigned char>(ch)) != 0;
        };
        auto alpha = [](char ch) {
            return std::isalpha(static_cast<unsigned char>(ch)) != 0;
        };
        auto alnum = [](char ch) {
            return std::isalnum(static_cast<unsigned char>(ch)) != 0;
        };

        auto advance = [&]() -> char {
            const char ch = source[index++];
            if (ch == '\n')
            {
                ++line;
                column = 1;
            }
            else
            {
                ++column;
            }
            return ch;
        };

        auto emit = [&](XJScriptTokenType type,
                        size_t start, size_t startLine,
                        size_t startColumn) {
            result.Tokens.push_back({
                type,
                std::string(source.substr(start, index - start)),
                startLine,
                startColumn
            });
        };

        auto error = [&](size_t atLine, size_t atColumn,
                         std::string message) {
            result.Diagnostics.push_back({
                atLine, atColumn, std::move(message)
            });
        };

        while (index < source.size())
        {
            const size_t start = index;
            const size_t startLine = line;
            const size_t startColumn = column;
            const char ch = source[index];

            if (ch == ' ' || ch == '\t' ||
                ch == '\r' || ch == '\n')
            {
                advance();
                continue;
            }

            if (ch == '/' && index + 1 < source.size() &&
                source[index + 1] == '/')
            {
                while (index < source.size() &&
                       source[index] != '\n')
                    advance();
                continue;
            }

            if (alpha(ch) || ch == '_')
            {
                advance();
                while (index < source.size() &&
                       (alnum(source[index]) || source[index] == '_'))
                    advance();

                const auto text = source.substr(start, index - start);
                const auto it = keywords.find(text);
                emit(it == keywords.end()
                         ? XJScriptTokenType::Identifier
                         : it->second,
                     start, startLine, startColumn);
                continue;
            }

            if (digit(ch))
            {
                // 数字是否合法在这里一次性确认，避免把 12abc 拆成两个合法 Token。
                advance();
                while (index < source.size() && digit(source[index]))
                    advance();

                if (index + 1 < source.size() &&
                    source[index] == '.' && digit(source[index + 1]))
                {
                    advance();
                    while (index < source.size() && digit(source[index]))
                        advance();
                }

                if (index < source.size() &&
                    (source[index] == 'e' || source[index] == 'E'))
                {
                    advance();
                    if (index < source.size() &&
                        (source[index] == '+' || source[index] == '-'))
                        advance();

                    while (index < source.size() && digit(source[index]))
                        advance();
                }

                // C++ 式 float 后缀；是否允许给整数加 f 由 Parser 决定。
                if (index < source.size() &&
                    (source[index] == 'f' || source[index] == 'F'))
                    advance();

                // 不把 12abc、1e+foo、1.2.3 拆成合法 token。
                bool invalidTail = false;
                while (index < source.size() &&
                       (alnum(source[index]) ||
                        source[index] == '_' || source[index] == '.'))
                {
                    invalidTail = true;
                    advance();
                }

                const auto text = source.substr(start, index - start);
                const size_t numericLength =
                    (text.back() == 'f' || text.back() == 'F')
                        ? text.size() - 1 : text.size();

                double value = 0.0;
                const char* begin = text.data();
                const char* end = begin + numericLength;
                const auto parsed = std::from_chars(
                    begin, end, value, std::chars_format::general);

                if (invalidTail || parsed.ec != std::errc{} ||
                    parsed.ptr != end || !std::isfinite(value))
                {
                    error(startLine, startColumn,
                          "Invalid numeric literal");
                }
                else
                {
                    emit(XJScriptTokenType::Number,
                         start, startLine, startColumn);
                }
                continue;
            }

            if (ch == '"')
            {
                // 字符串保留源码文本，实际转义解码由 Parser 构建字面量时完成。
                advance();
                bool closed = false;
                bool valid = true;

                while (index < source.size())
                {
                    if (source[index] == '"')
                    {
                        advance();
                        closed = true;
                        break;
                    }

                    if (source[index] == '\n' || source[index] == '\r')
                        break;

                    if (source[index] == '\\')
                    {
                        const size_t escapeLine = line;
                        const size_t escapeColumn = column;
                        advance();

                        if (index >= source.size() ||
                            source[index] == '\n' ||
                            source[index] == '\r')
                            break;

                        const char escaped = source[index];
                        if (escaped != '\\' && escaped != '"' &&
                            escaped != 'n' && escaped != 'r' &&
                            escaped != 't')
                        {
                            error(escapeLine, escapeColumn,
                                  "Invalid string escape");
                            valid = false;
                        }
                        advance();
                        continue;
                    }

                    advance();
                }

                if (!closed)
                    error(startLine, startColumn,
                          "Unterminated string literal");
                else if (valid)
                    emit(XJScriptTokenType::String,
                         start, startLine, startColumn);
                continue;
            }

            advance();
            XJScriptTokenType type{};
            bool recognized = true;

            auto match = [&](char expected) {
                if (index < source.size() && source[index] == expected)
                {
                    advance();
                    return true;
                }
                return false;
            };

            switch (ch)
            {
                case '(': type = XJScriptTokenType::LeftParen; break;
                case ')': type = XJScriptTokenType::RightParen; break;
                case '{': type = XJScriptTokenType::LeftBrace; break;
                case '}': type = XJScriptTokenType::RightBrace; break;
                case ',': type = XJScriptTokenType::Comma; break;
                case ':': type = XJScriptTokenType::Colon; break;
                case ';': type = XJScriptTokenType::Semicolon; break;
                case '.': type = XJScriptTokenType::Dot; break;
                case '[':type = XJScriptTokenType::LeftBracket;break;
                case ']':type = XJScriptTokenType::RightBracket;break;
                case '+':
                    type = match('=')
                        ? XJScriptTokenType::PlusEqual
                        : match('+')
                            ? XJScriptTokenType::PlusPlus
                            : XJScriptTokenType::Plus;
                    break;
                case '-':
                    type = match('=')
                        ? XJScriptTokenType::MinusEqual
                        : match('-')
                            ? XJScriptTokenType::MinusMinus
                            : XJScriptTokenType::Minus;
                    break;
                case '*':
                    type = match('=')
                        ? XJScriptTokenType::StarEqual
                        : XJScriptTokenType::Star;
                    break;
                case '/':
                    type = match('=')
                        ? XJScriptTokenType::SlashEqual
                        : XJScriptTokenType::Slash;
                    break;
                case '=':
                    type = match('=')
                        ? XJScriptTokenType::EqualEqual
                        : XJScriptTokenType::Equal;
                    break;
                case '!':
                    type = match('=')
                        ? XJScriptTokenType::BangEqual
                        : XJScriptTokenType::Bang;
                    break;
                case '<':
                    type = match('=')
                        ? XJScriptTokenType::LessEqual
                        : XJScriptTokenType::Less;
                    break;
                case '>':
                    type = match('=')
                        ? XJScriptTokenType::GreaterEqual
                        : XJScriptTokenType::Greater;
                    break;
                case '&':
                    type = match('&')
                        ? XJScriptTokenType::AndAnd
                        : XJScriptTokenType{};
                    recognized = source.substr(start, index - start) == "&&";
                    break;
                case '|':
                    type = match('|')
                        ? XJScriptTokenType::OrOr
                        : XJScriptTokenType{};
                    recognized = source.substr(start, index - start) == "||";
                    break;
                default: recognized = false; break;
            }

            if (recognized)
                emit(type, start, startLine, startColumn);
            else
                error(startLine, startColumn,
                      "Unexpected character");
        }

        result.Tokens.push_back({
            XJScriptTokenType::End, "", line, column
        });
        return result;
    }
}
