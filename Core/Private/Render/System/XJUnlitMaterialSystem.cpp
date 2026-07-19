#include "Render/System/XJUnlitMaterialSystem.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/VulkanCommon.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanDescriptorSet.h"
#include "Graphic/VulkanImageView.h"
#include "Graphic/XJVulkanFrameBuffer.h"//获取帧缓冲信息
#include "Render/XJRenderTarget.h"//获取渲染目标信息
#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Render/Material/XJMaterialShaderRuntimeLayoutBuilder.h"
#include "Render/Shader/XJShaderDescriptorLayoutBuilder.h"
#include "Render/Resource/XJMaterialFactory.h"
#include "Render/Material/XJUnlitMaterialBindingUtils.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "XJApplication.h"

#include "Edit/FileUtil.h"//获取文件工具类

#include "ECS/Component/XJTransformComponent.h"
#include "ECS/Component/Material/XJBaseMaterialComponent.h"//获取材质信息


namespace XJ
{
    namespace
    {
        std::string ToPipelineShaderSourcePath(const std::filesystem::path& shaderPath)
        {
            std::filesystem::path path = shaderPath;
            if (path.extension() == ".spv")
                path.replace_extension();

            return path.generic_string();
        }

        bool IsSameShaderPath(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
        {
            if (lhs.empty() || rhs.empty())
                return false;

            std::error_code ec;
            if (std::filesystem::equivalent(lhs, rhs, ec))
                return true;

            return lhs.lexically_normal().generic_string() == rhs.lexically_normal().generic_string() ||
                   lhs.filename() == rhs.filename();
        }
    }

    XJMaterialPipelineRuntime* XJUnlitMaterialSystem::GetOrCreatePipelineRuntime(const std::filesystem::path& shaderPath, XJVulkanRenderPass* renderPass)
    {
        const std::string runtimeKey = shaderPath.lexically_normal().generic_string();

        auto runtimeIt = mPipelineRuntimes.find(runtimeKey);
        if (runtimeIt != mPipelineRuntimes.end())
            return &runtimeIt->second;

        auto shaderAsset = XJShaderAssetSerializer::LoadFromFile(shaderPath);
        if (!shaderAsset)
        {
            spdlog::error("Material pipeline runtime failed to load shader asset: {}", shaderPath.generic_string());
            return nullptr;
        }

        XJMaterialPipelineRuntime runtime{};

        if (!XJMaterialShaderRuntimeLayoutBuilder::BuildFromShaderAsset(*shaderAsset, runtime.ShaderLayout))
        {
            spdlog::error("Material pipeline runtime failed to build shader runtime layout: {}", shaderPath.generic_string());
            return nullptr;
        }

        if (!runtime.ShaderLayout.HasPrimaryFrameUbo())
        {
            spdlog::error("Material pipeline runtime has no primary frame UBO: {}", shaderPath.generic_string());
            return nullptr;
        }

        if (!runtime.ShaderLayout.HasFrameSet())
        {
            spdlog::error("Material pipeline runtime has no frame descriptor set: {}", shaderPath.generic_string());
            return nullptr;
        }

        if (!runtime.ShaderLayout.HasPrimaryMaterialUbo())
        {
            spdlog::error("Material pipeline runtime has no primary material UBO: {}", shaderPath.generic_string());
            return nullptr;
        }

        if (!runtime.ShaderLayout.HasMaterialParameterSet())
        {
            spdlog::error("Material pipeline runtime has no material parameter descriptor set: {}", shaderPath.generic_string());
            return nullptr;
        }

        if (!runtime.ShaderLayout.HasMaterialResourceSet())
        {
            spdlog::error("Material pipeline runtime has no material resource descriptor set: {}", shaderPath.generic_string());
            return nullptr;
        }

        XJVulkanDevice *kDevice = XJGetDevice();
         //Frame Ubo
        runtime.FrameUboDescSetLayout = std::make_shared<XJVulkanDescriptorSetLayout>(kDevice, runtime.ShaderLayout.FrameBindings);
        //描述符集
        std::vector<VkDescriptorPoolSize> framePoolSizes =
        {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1
            },
        };

        runtime.FrameDescriptorPool =
            std::make_shared<XJ::XJVulkanDescriptorPool>(
                kDevice,
                1,
                framePoolSizes);
            
        runtime.FrameUboDescSet =
            runtime.FrameDescriptorPool->AllocateDescriptorSet(
                runtime.FrameUboDescSetLayout.get(),
                1)[0];
            
        runtime.FrameUboBuffer =
            std::make_shared<XJ::XJVulkanBuffer>(
                kDevice,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                sizeof(FrameUbo),
                nullptr,
                true);

        //材质参数
        runtime.MaterialParamDescSetLayout =
            std::make_shared<XJVulkanDescriptorSetLayout>(
                kDevice,
                runtime.ShaderLayout.MaterialParameterBindings);

        runtime.MaterialResourceDescSetLayout =
            std::make_shared<XJVulkanDescriptorSetLayout>(
                kDevice,
                runtime.ShaderLayout.MaterialResourceBindings);
        //常量
        VkPushConstantRange kModelPC =
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(ModelPC)
        };
         //shader
        ShaderLayout kShaderLayout =
        {
            .descriptorSetLayouts = {
                runtime.FrameUboDescSetLayout->XJGetDescriptorSet(),
                runtime.MaterialParamDescSetLayout->XJGetDescriptorSet(),
                runtime.MaterialResourceDescSetLayout->XJGetDescriptorSet()
            },
            .pushConstantRanges = { kModelPC }
        };
        //资源
        runtime.PipelineLayout = std::make_shared<XJVulkanPipelineLayout>(
            kDevice,
            ToPipelineShaderSourcePath(runtime.ShaderLayout.VertexPath),
            ToPipelineShaderSourcePath(runtime.ShaderLayout.FragmentPath),
            kShaderLayout);

        std::vector<VkVertexInputBindingDescription> kVertexBindings{};
        kVertexBindings.resize(1);
        kVertexBindings[0].binding = 0;//绑定点0
        kVertexBindings[0].stride = sizeof(XJ::XJVulkanVertex);//步幅为顶点结构体大小
        kVertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//每个顶点一个数据
        //顶点输入的状态
        std::vector<VkVertexInputAttributeDescription> kVertexAttributes =
        {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(XJVulkanVertex, position)
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(XJVulkanVertex, texcoord0)
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(XJVulkanVertex, normal)
            }
        };

        runtime.Pipeline =
            std::make_shared<XJVulkanPipeline>(kDevice, renderPass, runtime.PipelineLayout.get());
        // 仅当 render pass 有深度附件时才启用深度测试
        {
            bool hasDepth = false;
            const auto& attachments = renderPass->XJGetAttachments();
            for (const auto& att : attachments)
            {
                if (IsDepthStencilFormat(att.format))
                {
                    hasDepth = true;
                    break;
                }
            }

            if (hasDepth)
                runtime.Pipeline->EnableDepthTest(VK_TRUE);
        }

        runtime.Pipeline->SetVertexInputState(kVertexBindings, kVertexAttributes);//设置顶点输入状态
        runtime.Pipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);//设置输入装配状态 三角形列表
        runtime.Pipeline->SetDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
        runtime.Pipeline->SetMultisampleState(mSampleCount, VK_FALSE, 0.2f);//设置多重采样状态  4倍采样  不启用样本着色
        runtime.Pipeline->Create();

        spdlog::info(
            "Material pipeline runtime created: shader='{}', materialUbo='{}', set={}, binding={}, size={}, samplers={}",
            runtime.ShaderLayout.ShaderPath.generic_string(),
            runtime.ShaderLayout.PrimaryFrameUboName,
            runtime.ShaderLayout.PrimaryFrameUboSet,
            runtime.ShaderLayout.PrimaryFrameUboBinding,
            runtime.ShaderLayout.PrimaryMaterialUboName,
            runtime.ShaderLayout.PrimaryMaterialUboSet,
            runtime.ShaderLayout.PrimaryMaterialUboBinding,
            runtime.ShaderLayout.PrimaryMaterialUboSize,
            runtime.ShaderLayout.MaterialSamplerBindings.size());

        auto [insertIt, inserted] = mPipelineRuntimes.emplace(runtimeKey, std::move(runtime));
        return &insertIt->second;
    }

    XJMaterialPipelineRuntime* XJUnlitMaterialSystem::ResolveMaterialPipelineRuntime(const XJUnlitMaterial* material)
    {
        if (!mDefaultPipelineRuntime || !mDefaultPipelineRuntime->IsValid())
            return nullptr;

        if (!material || material->GetShaderPath().empty())
            return mDefaultPipelineRuntime;

        if (!mRenderPass)
        {
            spdlog::warn("Resolve material pipeline runtime fallback: render pass is null.");
            return mDefaultPipelineRuntime;
        }

        XJMaterialPipelineRuntime* runtime =
            GetOrCreatePipelineRuntime(material->GetShaderPath(), mRenderPass);

        if (!runtime || !runtime->IsValid())
        {
            const std::string shaderPath = material->GetShaderPath().lexically_normal().generic_string();

            if (mWarnedFallbackShaderPaths.insert(shaderPath).second)
            {
                spdlog::warn(
                    "Resolve material pipeline runtime fallback: shader '{}' failed to create runtime.",
                    shaderPath);
            }

            return mDefaultPipelineRuntime;
        }

        return runtime;
    }

    void XJUnlitMaterialSystem::OnInit(XJVulkanRenderPass *renderPass) 
    {//添加内容查看shader Uniform  UBO
        mRenderPass = renderPass;
        XJVulkanDevice *kDevice = XJGetDevice();

        mDefaultPipelineRuntime = GetOrCreatePipelineRuntime(
            "Resource/Shader/Unlit.xjshader",
            renderPass);

        if (!mDefaultPipelineRuntime || !mDefaultPipelineRuntime->IsValid())
        {
            spdlog::error("Unlit material system failed to create default pipeline runtime.");
            return;
        }

        //重新创建材质
        ReCreateMaterialDescPool(*mDefaultPipelineRuntime, NUM_MATERIAL_BATCH);
    }
    void XJUnlitMaterialSystem::OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) 
    {
        XJScene *kScene = XJGetScene();

        if(!kScene){return;}//如果场景不存在，直接返回

        if (!mDefaultPipelineRuntime || !mDefaultPipelineRuntime->IsValid())
        {
            spdlog::error("Unlit material system has no valid default pipeline runtime.");
            return;
        }

        entt::registry &kReg =  kScene->XJGetEcsRegistry();//拿到注射器
        auto kView = kReg.view<XJTransformComponent, XJUnlitMaterialComponent>();//获取视图，包含有变换组件、网格组件和基础材质组件的实体

        if (kView.end() == kView.begin()) 
        {
           return;   // 视图确实为空
        }
      
         //bind pipeline
        //runtime->Pipeline->BindPipeline(cmdBuffer);//绑定管线
        //vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(), 0, 1,  mDescriptorSets.data(), 0, nullptr);
        XJ::XJVulkanFrameBuffer *kFrameBuffer = renderTarget->XJGetCurrentFrameBuffer();
        if (!kFrameBuffer) 
        {
            spdlog::error("FrameBuffer is null, skipping render");
            return;
        }
        //设置视口和裁剪矩形
        VkViewport kViewport{};
        kViewport.x = 0.0f;
        kViewport.y = 0.0f;
        kViewport.width = static_cast<float>(kFrameBuffer->XJGetWidth());
        kViewport.height = static_cast<float>(kFrameBuffer->XJGetHeight());
        kViewport.minDepth = 0.0f;
        kViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &kViewport);
        //设置裁剪矩形
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {kFrameBuffer->XJGetWidth(), kFrameBuffer->XJGetHeight()};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        uint32_t kMaterialCount = XJMaterialFactory::GetInstance()->GetMaterialSize<XJUnlitMaterial>();//材质数量
        std::unordered_set<XJMaterialPipelineRuntime*> forceUpdateRuntimes;

        if(kMaterialCount > mDefaultPipelineRuntime->LastDescriptorSetCount)
        {
            spdlog::info("Unlit: pool resize, count={} -> {}", mDefaultPipelineRuntime->LastDescriptorSetCount, kMaterialCount);

            ReCreateMaterialDescPool(*mDefaultPipelineRuntime, kMaterialCount);
            forceUpdateRuntimes.insert(mDefaultPipelineRuntime);
        }

        uint32_t kEntityIndex = 0; // 实体索引，用于动态UBO偏移计算
        XJMaterialPipelineRuntime* boundRuntime = nullptr;
        std::unordered_set<XJMaterialPipelineRuntime*> updatedFrameRuntimes;
        //材质是否更新
        std::unordered_map<XJMaterialPipelineRuntime*, std::vector<bool>> updateFlagsByRuntime;
        kView.each([this, &cmdBuffer, &updateFlagsByRuntime, &forceUpdateRuntimes, &kEntityIndex, &boundRuntime, &updatedFrameRuntimes, renderTarget, kMaterialCount](const auto &entity, const XJTransformComponent& transComp, const XJUnlitMaterialComponent& matComp)
        {
            auto kMeshMaterials = matComp.XJGetMeshMaterials();
            for(const auto&entry :kMeshMaterials)//要是没有材质酒放弃渲染
            {
                XJUnlitMaterial *kMaterial = entry.first;
                uint32_t kMaterialIndex = kMaterial->XJGetIndex();
                if(!kMaterial || kMaterialIndex < 0) 
                {
                    spdlog::error("TODO: Default material of error material ?");
                    continue;
                }
                //runtime resolve 
                XJMaterialPipelineRuntime* runtime = ResolveMaterialPipelineRuntime(kMaterial);
                if (!runtime || !runtime->IsValid())
                {
                    spdlog::warn("Skip material {}: failed to resolve valid pipeline runtime.", kMaterial->GetIndex());
                    continue;
                }
                
                if (updatedFrameRuntimes.insert(runtime).second)
                    UpdateFrameUboDescSet(*runtime, renderTarget);
                //bind pipeline 
                if (kMaterialIndex >= runtime->LastDescriptorSetCount)
                {
                    spdlog::info(
                        "Unlit: runtime pool resize, shader='{}', count={} -> {}",
                        runtime->ShaderLayout.ShaderPath.generic_string(),
                        runtime->LastDescriptorSetCount,
                        kMaterialCount);
                    
                    ReCreateMaterialDescPool(*runtime, kMaterialCount);
                    forceUpdateRuntimes.insert(runtime);
                }
                
                if (boundRuntime != runtime)
                {
                    runtime->Pipeline->BindPipeline(cmdBuffer);//绑定管线
                    boundRuntime = runtime;
                }
                 
                if (kMaterialIndex >= runtime->MaterialParamDescSets.size() ||
                    kMaterialIndex >= runtime->MaterialResourceDescSets.size())
                {
                    spdlog::warn(
                        "Skip material {}: descriptor set index is out of bounds.",
                        kMaterial->GetIndex());
                    continue;
                }

                auto& runtimeUpdateFlags = updateFlagsByRuntime[runtime];
                if (runtimeUpdateFlags.size() < kMaterialCount)
                    runtimeUpdateFlags.resize(kMaterialCount, false);

                const bool forceUpdateRuntime =
                    forceUpdateRuntimes.find(runtime) != forceUpdateRuntimes.end();

                VkDescriptorSet kParamsDescSet = runtime->MaterialParamDescSets[kMaterialIndex];
                VkDescriptorSet kResourceDescSet = runtime->MaterialResourceDescSets[kMaterialIndex];


                if(!runtimeUpdateFlags[kMaterialIndex] || forceUpdateRuntime)
                {
                    UpdateMaterialParamsDescSet(*runtime, kParamsDescSet, kMaterial);
                    kMaterial->FinishFlushParams();
                    
                     // 无条件更新 texture/sampler descriptor（防止首次创建后从未更新）
                    UpdateMaterialResourceDescSet(*runtime, kResourceDescSet, kMaterial);
                    kMaterial -> FinishFlushResoure();
                    
                    runtimeUpdateFlags[kMaterialIndex] = true;
                }

                VkDescriptorSet kDescriptorSet[] = { runtime->FrameUboDescSet, kParamsDescSet, kResourceDescSet};
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, runtime->PipelineLayout->XJGetPipelineLayout(),
                                        0, ARRAY_SIZE(kDescriptorSet), kDescriptorSet, 0, nullptr);
                
                ModelPC kPC = {transComp.GetModelMatrix()};
                //推送常量
                vkCmdPushConstants(cmdBuffer, runtime->PipelineLayout->XJGetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(kPC), &kPC);
                //spdlog::warn("Draw: inst={} matIdx={} meshCnt={}",(void*)this, kMaterialIndex, entry.second.size());
                for(const auto&kMeshIndex : entry.second)
                {
                    matComp.XJGetMesh(kMeshIndex)->Draw(cmdBuffer);
                }

            }
            
        });

    }

    void XJUnlitMaterialSystem::OnDestroy() 
    {

        mDefaultPipelineRuntime = nullptr;

        for (auto& entry : mPipelineRuntimes)
            entry.second.Clear();

        mPipelineRuntimes.clear();
        mWarnedFallbackShaderPaths.clear();

        mRenderPass = nullptr;

    }

    void XJUnlitMaterialSystem::ReCreateMaterialDescPool(XJMaterialPipelineRuntime& runtime, uint32_t materialCount) //动态扩容
    {
        XJVulkanDevice *kDevice = XJGetDevice();

        if (!runtime.IsValid())
        {
            spdlog::error("ReCreateMaterialDescPool failed: pipeline runtime is invalid.");
            return;
        }

        //最新池子需要放多少个
        uint32_t kNewDescriptorSetCount = runtime.LastDescriptorSetCount;
        if (runtime.LastDescriptorSetCount == 0)
            kNewDescriptorSetCount = NUM_MATERIAL_BATCH;

        while(kNewDescriptorSetCount < materialCount)
        {
            kNewDescriptorSetCount *= 2;//2倍数增长  直到大于材质数量
            spdlog::info("ReCreateMaterialDescPool, new Descriptor Set count: {0}", kNewDescriptorSetCount);
        }

        if(kNewDescriptorSetCount > NUM_MATERIAL_BATCH_MAX)//大于最大的数量就报错
        {
            spdlog::error("Descriptor Set max count is:{0},but request:{1}", NUM_MATERIAL_BATCH_MAX, kNewDescriptorSetCount);
            return;
        }

        //销毁老参数
        runtime.MaterialParamDescSets.clear();
        runtime.MaterialResourceDescSets.clear();
        runtime.MaterialDescriptorPool.reset();
        //从新申请池子
        std::vector<VkDescriptorPoolSize> kPoolSizes;

        auto paramPoolSizes = BuildDescriptorPoolSizes(runtime.ShaderLayout.Reflection, runtime.ShaderLayout.MaterialParameterSet, kNewDescriptorSetCount);
        auto resourcePoolSizes = BuildDescriptorPoolSizes(runtime.ShaderLayout.Reflection, runtime.ShaderLayout.MaterialResourceSet, kNewDescriptorSetCount);

        kPoolSizes.insert(kPoolSizes.end(), paramPoolSizes.begin(), paramPoolSizes.end());

        for (const auto& poolSize : resourcePoolSizes)
            AddDescriptorPoolSize(kPoolSizes, poolSize.type, poolSize.descriptorCount);

        runtime.MaterialDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(kDevice, kNewDescriptorSetCount *2, kPoolSizes);
    

        //申请材质
        runtime.MaterialParamDescSets = runtime.MaterialDescriptorPool->AllocateDescriptorSet(runtime.MaterialParamDescSetLayout.get(), kNewDescriptorSetCount);
        runtime.MaterialResourceDescSets =  runtime.MaterialDescriptorPool->AllocateDescriptorSet(runtime.MaterialResourceDescSetLayout.get(), kNewDescriptorSetCount);
        assert(runtime.MaterialParamDescSets.size() == kNewDescriptorSetCount && "Failed to allocateDescriptorset");
        assert(runtime.MaterialResourceDescSets.size() == kNewDescriptorSetCount && "Failed to allocateDescriptorSet");

        //差值用来创建uinform buffer
        runtime.MaterialBuffers.resize(kNewDescriptorSetCount);
        runtime.MaterialBufferSizes.resize(kNewDescriptorSetCount, 0);
        //更新上一次的数量
        runtime.LastDescriptorSetCount = kNewDescriptorSetCount;
    
    }

    void XJUnlitMaterialSystem::EnsureMaterialBuffer(XJMaterialPipelineRuntime& runtime, uint32_t materialIndex, uint32_t requiredSize)//确保材质缓冲区的大小足够
    {
        if(requiredSize == 0) { return; }

        if(materialIndex >= runtime.MaterialBuffers.size())
        {
            spdlog::error("Material index {} is out of bounds (max {}).", materialIndex, runtime.MaterialBuffers.size());
            return;
        }

        if(runtime.MaterialBuffers[materialIndex] && runtime.MaterialBufferSizes[materialIndex] == requiredSize)
            return; 
        //
        XJVulkanDevice* kDevice = XJGetDevice();
        runtime.MaterialBuffers[materialIndex] = std::make_shared<XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, requiredSize, nullptr, true);
        runtime.MaterialBufferSizes[materialIndex] = requiredSize;
    }


    void XJUnlitMaterialSystem::UpdateFrameUboDescSet(XJMaterialPipelineRuntime& runtime, XJRenderTarget *renderTarget)//更新UBO的结构
    {

        if (!runtime.FrameUboBuffer || runtime.FrameUboDescSet == VK_NULL_HANDLE || !runtime.ShaderLayout.HasPrimaryFrameUbo())
        {
            spdlog::warn("Skip frame UBO update: pipeline runtime frame resources are invalid.");
            return;
        }

        XJApplication *kApp = XJGetApp();
        XJVulkanDevice *kDevice = XJGetDevice();//逻辑设备

        XJVulkanFrameBuffer *kFrameBuffer =  renderTarget->XJGetCurrentFrameBuffer();
        glm::ivec2 kResolution = { kFrameBuffer->XJGetWidth(), kFrameBuffer->XJGetHeight() };

        FrameUbo kFrameUbo = {
            .projMat = XJGetProjMat(renderTarget),//图形矩阵
            .viewMat = XJGetViewMat(renderTarget),//视图矩阵
            .resolution = kResolution,
            .frameId = static_cast<uint32_t>(kApp->XJGetFrameIndex()),//帧
            .time = kApp->XJGetStartTimeSecond()//时间
        };

        runtime.FrameUboBuffer->WriteData(&kFrameUbo);//写数据Ubo

        VkDescriptorBufferInfo bufferInfo = DescriptorSetWriter::BuildBufferInfo(runtime.FrameUboBuffer->XJGetBuffer(), 0, sizeof(kFrameUbo));//ubobuffer
        VkWriteDescriptorSet bufferWrite = DescriptorSetWriter::WriteBuffer(runtime.FrameUboDescSet, runtime.ShaderLayout.PrimaryFrameUboBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo);//写buffer
        DescriptorSetWriter::UpdateDescriptorSets(kDevice->XJGetDevice(), { bufferWrite });
    }

    void XJUnlitMaterialSystem::UpdateMaterialParamsDescSet(XJMaterialPipelineRuntime& runtime, VkDescriptorSet descSet, XJUnlitMaterial *material)//更新材质参数
    {
        XJVulkanDevice *device  = XJGetDevice();
        XJMaterialParameterBlock& block = material->GetParameterBlock();
        if (block.Empty())// block 为空就直接 return，会导致 descriptor 没更新
        {
            spdlog::warn("Skip material params update: material {} has empty parameter block.", material->GetIndex());
            return;
        }

        //纹理参数更新
        const TextureView *textureA  = material->GetTextureView(UNLIT_MAT_BASE_COLOR_A);
        if(textureA)
        {
            //spdlog::info("TextureA: valid={}, enable={}", textureA->IsValid(), textureA->bEnable);
            TextureParam texParamA{};
            XJMaterial::UpdateTextureParams(textureA, &texParamA);
            material->SetTextureParamA(texParamA);
        }

        const TextureView* textureB = material->GetTextureView(UNLIT_MAT_BASE_COLOR_B);
        if (textureB)
        {
            TextureParam paramB{};
            XJMaterial::UpdateTextureParams(textureB, &paramB);
            material->SetTextureParamB(paramB);
        }

        EnsureMaterialBuffer(runtime, material->GetIndex(), block.GetSize());

        XJVulkanBuffer* materialBuffer = runtime.MaterialBuffers[material->GetIndex()].get();
        if (!materialBuffer)
            return;
        materialBuffer->WriteData(const_cast<uint8_t*>(block.GetDataPtr()));
        //spdlog::info("Upload material block: material={}, blockSize={}", material->GetIndex(), block.GetSize());
        if (!runtime.ShaderLayout.HasPrimaryMaterialUbo())
        {
            spdlog::warn("Skip material params update: shader runtime layout has no primary material UBO.");
            return;
        }


        VkDescriptorBufferInfo kBufferInfo = DescriptorSetWriter::BuildBufferInfo(materialBuffer->XJGetBuffer(), 0, block.GetSize());
        VkWriteDescriptorSet kBufferWrite = DescriptorSetWriter::WriteBuffer(descSet, runtime.ShaderLayout.PrimaryMaterialUboBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &kBufferInfo);
        DescriptorSetWriter::UpdateDescriptorSets(device->XJGetDevice(), { kBufferWrite });
    }

    void XJUnlitMaterialSystem::UpdateMaterialResourceDescSet(XJMaterialPipelineRuntime& runtime, VkDescriptorSet descSet, XJUnlitMaterial *material)//材质资源更新
    {
        (void)runtime;

        XJVulkanDevice *device = XJGetDevice();//逻辑设备
        //优化

        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<uint32_t> bindings;
        //只有真的 push 了 descriptor 才标记 wroteBindingX = true
        auto addTextureWrite = [&](uint32_t binding, const TextureView* textureView) -> bool
        {
            if (!textureView || !textureView->texture || !textureView->sampler)
                return false;
        
            imageInfos.push_back(DescriptorSetWriter::BuildImageInfo(
                textureView->sampler->XJGetSampler(),
                textureView->texture->XJGetImageView()->XJGetImageView()));
            
            bindings.push_back(binding);
            return true;
        };

        for (const auto& textureBinding : material->GetTextureBindings())
        {
            const TextureView* textureView = material->GetSamplerTextureView(textureBinding.SamplerName);

            if (!textureView)
                textureView = GetUnlitTextureViewForBinding(*material, textureBinding);

            if (!addTextureWrite(textureBinding.Binding, textureView))
            {
               spdlog::warn("Skip texture descriptor write: material={}, sampler='{}', binding={}", material->GetIndex(), textureBinding.SamplerName, textureBinding.Binding);
            }
        }

        // Unlit.frag statically declares both textureA and textureB, so both descriptors must be valid.
        if (imageInfos.empty())
            return;

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(imageInfos.size());

        for (size_t index = 0; index < imageInfos.size(); ++index)
        {
            writes.push_back(DescriptorSetWriter::WriteImage(
                descSet,
                bindings[index],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                &imageInfos[index]));
        }

        DescriptorSetWriter::UpdateDescriptorSets(device->XJGetDevice(), writes);
    }
}
