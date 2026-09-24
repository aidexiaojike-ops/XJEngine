#include "Script/Language/XJScriptLexer.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void CheckTypes(
        std::string_view source,
        std::vector<XJ::XJScriptTokenType> expected)
    {
        const auto result = XJ::XJScriptLexer::Scan(source);
        Check(result.IsValid(), "valid source must lex");
        Check(result.Tokens.size() == expected.size(),
              "token count");

        for (size_t i = 0; i < expected.size(); ++i)
            Check(result.Tokens[i].Type == expected[i],
                  "token type");
    }

    void CheckError(
        std::string_view source,
        size_t line,
        size_t column,
        std::string_view message)
    {
        const auto result = XJ::XJScriptLexer::Scan(source);
        Check(!result.IsValid(), "invalid source must fail");
        Check(!result.Diagnostics.empty(), "diagnostic present");
        Check(result.Diagnostics[0].Line == line,
              "diagnostic line");
        Check(result.Diagnostics[0].Column == column,
              "diagnostic column");
        Check(result.Diagnostics[0].Message == message,
              "diagnostic message");
        Check(result.Tokens.back().Type ==
                  XJ::XJScriptTokenType::End,
              "End token always present");
    }
}

int main()
{
    using T = XJ::XJScriptTokenType;

    CheckTypes(
        "class Rotate : ScriptBehaviour {\n"
        "public: float speed = 45.0f;\n"
        "void OnUpdate(float dt) { speed += dt; }\n"
        "private: bool enabled = true;\n"
        "};",
        {T::Class, T::Identifier, T::Colon, T::Identifier,
         T::LeftBrace, T::Public, T::Colon, T::Float,
         T::Identifier, T::Equal, T::Number, T::Semicolon,
         T::Void, T::Identifier, T::LeftParen, T::Float,
         T::Identifier, T::RightParen, T::LeftBrace,
         T::Identifier, T::PlusEqual, T::Identifier,
         T::Semicolon, T::RightBrace, T::Private,
         T::Colon, T::Bool, T::Identifier, T::Equal,
         T::True, T::Semicolon, T::RightBrace,
         T::Semicolon, T::End});

    CheckTypes(
        "float x = 1e-3f; x++; x -= 2; // comment\n",
        {T::Float, T::Identifier, T::Equal, T::Number,
         T::Semicolon, T::Identifier, T::PlusPlus,
         T::Semicolon, T::Identifier, T::MinusEqual,
         T::Number, T::Semicolon, T::End});

    CheckTypes(
        "string text = \"a\\n\\\"b\";",
        {T::StringType, T::Identifier, T::Equal,
         T::String, T::Semicolon, T::End});

    CheckTypes(
        "[[FieldId(\"1001\")]] float speed = 1.0f;",
        {T::LeftBracket, T::LeftBracket, T::Identifier,
         T::LeftParen, T::String, T::RightParen,
         T::RightBracket, T::RightBracket, T::Float,
         T::Identifier, T::Equal, T::Number, T::Semicolon, T::End});

    CheckError("12abc", 1, 1, "Invalid numeric literal");
    CheckError("1e+", 1, 1, "Invalid numeric literal");
    CheckError("1.2.3", 1, 1, "Invalid numeric literal");
    CheckError("1e9999", 1, 1, "Invalid numeric literal");
    CheckError("\"a\\q\"", 1, 3, "Invalid string escape");
    CheckError("\"abc", 1, 1, "Unterminated string literal");
    CheckError("\"a\nb", 1, 1, "Unterminated string literal");
    CheckError("float x;\n  @", 2, 3, "Unexpected character");

    const auto positions =
        XJ::XJScriptLexer::Scan("class A {\n  float x;\n}");
    Check(positions.Tokens[3].Line == 2 &&
              positions.Tokens[3].Column == 3,
          "field token position");

    std::cout << "XJScriptLexer tests passed\n";
    return EXIT_SUCCESS;
}
