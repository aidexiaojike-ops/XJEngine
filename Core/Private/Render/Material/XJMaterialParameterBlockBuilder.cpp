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
        std::unordered_map<uint64_t, XJMaterialParameterBlock> blocks;
        XJMaterialParameterBlockBuildResult result = BuildBlocks(material, layout, blocks);

        const uint64_t primaryKey = XJMakeMaterialUboKey(layout.GetUboSet(), layout.GetUboBinding());
        auto it = blocks.find(primaryKey);
        if (it != blocks.end())
            outBlock = it->second;
        else
        {
            outBlock.Resize(layout.GetUboSize());
            outBlock.Clear();
        }

        return result;
    }

    XJMaterialParameterBlockBuildResult XJMaterialParameterBlockBuilder::BuildBlocks(
        const XJMaterialAsset& material,
        const XJMaterialParameterLayout& layout,
        std::unordered_map<uint64_t, XJMaterialParameterBlock>& outBlocks)
    {
        XJMaterialParameterBlockBuildResult result;

        outBlocks.clear();

        if (!layout.IsValid())
        {
            AddMaterialBuildError(result, "Cannot build material parameter block: layout is invalid.");
            result.Valid = false;
            return result;
        }

        if (layout.GetUboLayouts().empty())
        {
            AddMaterialBuildWarning(result, "Material parameter layout has no UBO data.");
            result.Valid = result.Errors.empty();
            return result;
        }

        for (const auto& uboLayout : layout.GetUboLayouts())
        {
            XJMaterialParameterBlock block(uboLayout.Size);
            block.Clear();
            outBlocks[XJMakeMaterialUboKey(uboLayout.Set, uboLayout.Binding)] = std::move(block);
        }

        for (const auto& binding : layout.GetParameterBindings())
        {
            const XJMaterialParameterValue* value = material.FindParameter(binding.ParameterName);
            if (!value)
            {
                AddMaterialBuildWarning(result, "Material parameter value not found: " + binding.ParameterName);
                continue;
            }

            auto blockIt = outBlocks.find(XJMakeMaterialUboKey(binding.Set, binding.Binding));
            if (blockIt == outBlocks.end())
            {
                AddMaterialBuildError(result, "Material parameter UBO block not found: " + binding.ParameterName);
                continue;
            }

            if (!WriteMaterialParameterValueToBlock(blockIt->second, binding, *value))
            {
                AddMaterialBuildError(result, "Failed to write material parameter value: " + binding.ParameterName);
                continue;
            }
        }

        result.Valid = result.Errors.empty();
        return result;
    }
}
