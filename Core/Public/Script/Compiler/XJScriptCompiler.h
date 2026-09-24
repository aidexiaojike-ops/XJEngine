#ifndef XJ_SCRIPT_COMPILER_H
#define XJ_SCRIPT_COMPILER_H

#include "Script/Bytecode/XJScriptBytecode.h"
#include "Script/Language/XJScriptSemanticAnalyzer.h"

#include <optional>
#include <vector>

namespace XJ
{
    struct XJScriptCompileResult
    {
        std::optional<XJScriptBytecodeModule> Module;
        std::vector<XJScriptDiagnostic> Diagnostics;

        bool IsValid() const
        {
            return Module.has_value() && Diagnostics.empty();
        }
    };

    class XJScriptCompiler
    {
        public:
            static XJScriptCompileResult Compile(const XJScriptClassDecl& declaration, const XJScriptSemanticResult& semantic);
    };
}

#endif