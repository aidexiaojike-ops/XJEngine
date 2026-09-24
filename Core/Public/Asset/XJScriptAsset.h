#ifndef XJ_SCRIPT_ASSET_H
#define XJ_SCRIPT_ASSET_H

#include "Asset/XJAsset.h"
#include "Script/Bytecode/XJScriptBytecode.h"
#include "Script/Language/XJScriptLexer.h"

#include <memory>
#include <string>
#include <vector>

namespace XJ
{
    class XJScriptAsset final : public XJAsset
    {
        public:
            XJScriptAsset()
            {
                mType =
                    XJAssetType::Script;
            }
        
            std::string Source;
        
            std::shared_ptr<
                const XJScriptBytecodeModule>
                Module;
        
            std::vector<
                XJScriptDiagnostic>
                Diagnostics;
        
            bool IsCompiled() const
            {
                return Module != nullptr &&
                       Diagnostics.empty();
            }
    };
}

#endif