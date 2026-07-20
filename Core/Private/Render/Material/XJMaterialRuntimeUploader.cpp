#include "Render/Material/XJMaterialRuntimeUploader.h"

#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanDescriptorSet.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/VulkanImageView.h"
#include "Render/Material/XJMaterialPipelineRuntimeDescriptor.h"
#include "Render/Material/XJUnlitMaterialBindingUtils.h"
#include "Render/Resource/XJMaterial.h"


namespace XJ
{
    bool XJMaterialRuntimeUploader::UpdateFrameUboDescSet(
        const XJMaterialRuntimeUploadContext& context,
        XJMaterialPipelineRuntime& runtime)
    {
        if (!context.Device)
        {
            spdlog::warn("Skip frame UBO update: device is null.");
            return false;
        }

        if (!runtime.FrameUboBuffer ||
            runtime.FrameUboDescSet == VK_NULL_HANDLE ||
            !runtime.ShaderLayout.HasPrimaryFrameUbo())
        {
            spdlog::warn("Skip frame UBO update: pipeline runtime frame resources are invalid.");
            return false;
        }

        FrameUbo frameUbo =
        {
            .projMat = context.ProjMat,
            .viewMat = context.ViewMat,
            .resolution = context.Resolution,
            .frameId = context.FrameId,
            .time = context.Time
        };

        runtime.FrameUboBuffer->WriteData(&frameUbo);

        VkDescriptorBufferInfo bufferInfo =
            DescriptorSetWriter::BuildBufferInfo(
                runtime.FrameUboBuffer->XJGetBuffer(),
                0,
                sizeof(frameUbo));

        VkWriteDescriptorSet bufferWrite =
            DescriptorSetWriter::WriteBuffer(
                runtime.FrameUboDescSet,
                runtime.ShaderLayout.PrimaryFrameUboBinding,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                &bufferInfo);

        DescriptorSetWriter::UpdateDescriptorSets(
            context.Device->XJGetDevice(),
            { bufferWrite });

        return true;
    }

    bool XJMaterialRuntimeUploader::UpdateMaterialParamsDescSet(
        XJVulkanDevice* device,
        XJMaterialPipelineRuntime& runtime,
        VkDescriptorSet descSet,
        XJMaterial* material)
    {
        if (!device)
        {
            spdlog::warn("Skip material params update: device is null.");
            return false;
        }

        if (!material)
        {
            spdlog::warn("Skip material params update: material is null.");
            return false;
        }

        XJMaterialParameterBlock& block = material->GetParameterBlock();
        if (block.Empty())
        {
            spdlog::warn(
                "Skip material params update: material {} has empty parameter block.",
                material->GetIndex());
            return false;
        }

        const TextureView* texture = material->GetTextureView(UNLIT_MAT_BASE_COLOR);
        if (texture)
        {
            TextureParam texParam{};
            XJMaterial::UpdateTextureParams(texture, &texParam);
            material->SetPrimaryUboMemberBytes("textureParam", &texParam, sizeof(texParam));
        }

        if (!XJMaterialPipelineRuntimeDescriptor::EnsureMaterialBuffer(
                device,
                runtime,
                material->GetIndex(),
                block.GetSize()))
        {
            return false;
        }

        if (material->GetIndex() >= runtime.MaterialBuffers.size())
        {
            spdlog::warn(
                "Skip material params update: material {} buffer index out of bounds.",
                material->GetIndex());
            return false;
        }

        XJVulkanBuffer* materialBuffer = runtime.MaterialBuffers[material->GetIndex()].get();
        if (!materialBuffer)
            return false;

        materialBuffer->WriteData(const_cast<uint8_t*>(block.GetDataPtr()));

        if (!runtime.ShaderLayout.HasPrimaryMaterialUbo())
        {
            spdlog::warn("Skip material params update: shader runtime layout has no primary material UBO.");
            return false;
        }

        VkDescriptorBufferInfo bufferInfo =
            DescriptorSetWriter::BuildBufferInfo(
                materialBuffer->XJGetBuffer(),
                0,
                block.GetSize());

        VkWriteDescriptorSet bufferWrite =
            DescriptorSetWriter::WriteBuffer(
                descSet,
                runtime.ShaderLayout.PrimaryMaterialUboBinding,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                &bufferInfo);

        DescriptorSetWriter::UpdateDescriptorSets(
            device->XJGetDevice(),
            { bufferWrite });

        return true;
    }

    bool XJMaterialRuntimeUploader::UpdateMaterialResourceDescSet(
        XJVulkanDevice* device,
        XJMaterialPipelineRuntime& runtime,
        VkDescriptorSet descSet,
        XJMaterial* material)
    {
        (void)runtime;

        if (!device)
        {
            spdlog::warn("Skip material resource update: device is null.");
            return false;
        }

        if (!material)
        {
            spdlog::warn("Skip material resource update: material is null.");
            return false;
        }

        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<uint32_t> bindings;

        auto addTextureWrite = [&](uint32_t binding, const TextureView* textureView) -> bool
        {
            if (!textureView || !textureView->texture || !textureView->sampler)
                return false;

            imageInfos.push_back(
                DescriptorSetWriter::BuildImageInfo(
                    textureView->sampler->XJGetSampler(),
                    textureView->texture->XJGetImageView()->XJGetImageView()));

            bindings.push_back(binding);
            return true;
        };

        for (const auto& textureBinding : material->GetTextureBindings())
        {
            const TextureView* textureView =
                material->GetSamplerTextureView(textureBinding.SamplerName);

            if (!textureView)
                textureView = GetUnlitTextureViewForBinding(*material, textureBinding);

            if (!addTextureWrite(textureBinding.Binding, textureView))
            {
                spdlog::warn(
                    "Skip texture descriptor write: material={}, sampler='{}', binding={}",
                    material->GetIndex(),
                    textureBinding.SamplerName,
                    textureBinding.Binding);
            }
        }

        if (imageInfos.empty())
            return false;

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(imageInfos.size());

        for (size_t index = 0; index < imageInfos.size(); ++index)
        {
            writes.push_back(
                DescriptorSetWriter::WriteImage(
                    descSet,
                    bindings[index],
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    &imageInfos[index]));
        }

        DescriptorSetWriter::UpdateDescriptorSets(
            device->XJGetDevice(),
            writes);

        return true;
    }
}
