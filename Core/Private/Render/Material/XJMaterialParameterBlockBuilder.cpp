#include "Render/Material/XJMaterialParameterBlockBuilder.h"
#include "Render/Material/XJMaterialParameterBlockWriter.h"
#include "Render/Material/XJMaterialBuildResultUtils.h"

namespace XJ
{


    XJMaterialParameterBlockBuildResult XJMaterialParameterBlockBuilder::Build(
        const XJMaterialAsset& material,
        const XJMaterialParameterLayout& layout,
        XJMaterialParameterBlock& outBlock)
    {
        XJMaterialParameterBlockBuildResult result;

        outBlock.Resize(layout.GetUboSize());
        outBlock.Clear();

        if (!layout.IsValid())
        {
            AddMaterialBuildError(result, "Cannot build material parameter block: layout is invalid.");
            result.Valid = false;
            return result;
        }

        if (layout.GetUboSize() == 0)
        {
            AddMaterialBuildWarning(result, "Material parameter layout has no UBO data.");
            result.Valid = result.Errors.empty();
            return result;
        }

        for (const auto& binding : layout.GetParameterBindings())
        {
            const XJMaterialParameterValue* value = material.FindParameter(binding.ParameterName);
            if (!value)
            {
                AddMaterialBuildWarning(result, "Material parameter value not found: " + binding.ParameterName);
                continue;
            }

            if (!WriteMaterialParameterValueToBlock(outBlock, binding, *value))
            {
                AddMaterialBuildError(result, "Failed to write material parameter value: " + binding.ParameterName);
                continue;
            }
        }

        result.Valid = result.Errors.empty();
        return result;
    }
}