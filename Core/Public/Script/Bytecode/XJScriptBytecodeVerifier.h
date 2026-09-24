#ifndef XJ_SCRIPT_BYTECODE_VERIFIER_H
#define XJ_SCRIPT_BYTECODE_VERIFIER_H

#include "Script/Bytecode/XJScriptBytecode.h"

#include <optional>
#include <string>
#include <vector>

namespace XJ
{
    struct XJScriptBytecodeVerifyDiagnostic
    {
        std::optional<uint32_t> FunctionIndex;
        std::optional<uint32_t> InstructionIndex;
        uint32_t Line = 1;
        uint32_t Column = 1;
        std::string Message;

    };

    struct XJScriptBytecodeVerifyResult
    {
        std::vector<XJScriptBytecodeVerifyDiagnostic> Diagnostics;

        std::vector<uint32_t> FunctionMaxStackDepths;

        bool IsValid() const
        {
            return Diagnostics.empty();
        }
    };

    class XJScriptBytecodeVerifier
    {
        public:
            static XJScriptBytecodeVerifyResult Verify(const XJScriptBytecodeModule& module);
    };
}

#endif