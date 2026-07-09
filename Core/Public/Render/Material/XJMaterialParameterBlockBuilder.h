//负责遍历 layout + material，调用 BlockWriter 填充 block
#ifndef XJ_MATERIAL_PARAMETER_BLOCK_BUILDER_H
#define XJ_MATERIAL_PARAMETER_BLOCK_BUILDER_H

#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Material/XJMaterialParameterBlock.h"   

#include "Asset/XJMaterialAsset.h"

namespace XJ
{
    struct XJMaterialParameterBlockBuildResult
    {
        bool Valid = false;
        std::vector<std::string> Errors;
        std::vector<std::string> Warnings;

        bool HasIssues() const
        {
            return !Errors.empty() || !Warnings.empty();
        }
    };

    class XJMaterialParameterBlockBuilder
    {
        public:
            static XJMaterialParameterBlockBuildResult Build(const XJMaterialAsset& material, const XJMaterialParameterLayout& layout, XJMaterialParameterBlock& outBlock);
    };
}

#endif