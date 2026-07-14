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

    void XJUnlitMaterialSystem::OnInit(XJVulkanRenderPass *renderPass) 
    {//添加内容查看shader Uniform  UBO

        XJVulkanDevice *kDevice = XJGetDevice();

        auto shaderAsset = XJShaderAssetSerializer::LoadFromFile("Resource/Shader/Unlit.xjshader");
        if (!shaderAsset || !XJMaterialShaderRuntimeLayoutBuilder::BuildFromShaderAsset(*shaderAsset, mShaderRuntimeLayout))
        {
            spdlog::error("Unlit material system failed to build shader runtime layout.");
            return;
        }

        mShaderReflection = mShaderRuntimeLayout.Reflection;

        //Frame Ubo
        {
            const std::vector<VkDescriptorSetLayoutBinding> kBindings = 
            {
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,//UNIFORM_BUFFER
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,//顶点着色  片源作色
                    // kBindings.pImmutableSamplers = 
                }
            };
            mFrameUboDescSetLayout = std::make_shared<XJVulkanDescriptorSetLayout>(kDevice, kBindings);
        }

        //材质参数
        {
            mMaterialParamDescSetLayout = std::make_shared<XJVulkanDescriptorSetLayout>(
                kDevice,
                mShaderRuntimeLayout.MaterialParameterBindings);
        }

        {
            mMaterialResourceDescSetLayout = std::make_shared<XJVulkanDescriptorSetLayout>(
                kDevice,
                mShaderRuntimeLayout.MaterialResourceBindings);
        }
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
            .descriptorSetLayouts = { mFrameUboDescSetLayout->XJGetDescriptorSet(), 
                                      mMaterialParamDescSetLayout->XJGetDescriptorSet(),
                                      mMaterialResourceDescSetLayout->XJGetDescriptorSet()},
            .pushConstantRanges = { kModelPC}
        };
        
        //资源
        mPipelineLayout = std::make_shared<XJVulkanPipelineLayout>(kDevice,
                                                                   ToPipelineShaderSourcePath(mShaderRuntimeLayout.VertexPath),
                                                                   ToPipelineShaderSourcePath(mShaderRuntimeLayout.FragmentPath), kShaderLayout);

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

        mPipeline = std::make_shared<XJVulkanPipeline>(kDevice, renderPass, mPipelineLayout.get());
        // 仅当 render pass 有深度附件时才启用深度测试
        {
            bool hasDepth = false;
            const auto& attachments = renderPass->XJGetAttachments();
            for (const auto& att : attachments)
            {
                if (IsDepthStencilFormat(att.format)) { hasDepth = true; break; }
            }
            if (hasDepth)
                mPipeline->EnableDepthTest(VK_TRUE);
        }
        mPipeline->SetVertexInputState(kVertexBindings, kVertexAttributes);//设置顶点输入状态
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);//设置输入装配状态 三角形列表
        mPipeline->SetDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
        mPipeline->SetMultisampleState(mSampleCount, VK_FALSE, 0.2f);//设置多重采样状态  4倍采样  不启用样本着色
        mPipeline->Create();

        //描述符 池子
        std::vector<VkDescriptorPoolSize> framePoolSizes =
        {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1
            },
        };

        mDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(kDevice, 1, framePoolSizes);
        mFrameUboDescSet = mDescriptorPool->AllocateDescriptorSet(mFrameUboDescSetLayout.get(), 1)[0];
        mFrameUboBuffer = std::make_shared<XJ::XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(FrameUbo), nullptr, true);
        //重新创建材质
        ReCreateMaterialDescPool(NUM_MATERIAL_BATCH);
    }
    void XJUnlitMaterialSystem::OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) 
    {
        XJScene *kScene = XJGetScene();

        if(!kScene){return;}//如果场景不存在，直接返回

        entt::registry &kReg =  kScene->XJGetEcsRegistry();//拿到注射器
        auto kView = kReg.view<XJTransformComponent, XJUnlitMaterialComponent>();//获取视图，包含有变换组件、网格组件和基础材质组件的实体

        if (kView.end() == kView.begin()) 
        {
           return;   // 视图确实为空
        }
      

         //bind pipeline
        mPipeline->BindPipeline(cmdBuffer);//绑定管线
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

        //每一帧渲染开始
        UpdateFrameUboDescSet(renderTarget);

        bool bShouldForceUpdateMaterial = false;//是否动态扩容材质
        uint32_t kMaterialCount = XJMaterialFactory::GetInstance()->GetMaterialSize<XJUnlitMaterial>();//材质数量

        if(kMaterialCount > mLastDescriptorSetCount)//做扩容
        {
            spdlog::info("Unlit: pool resize, count={}→{}", mLastDescriptorSetCount, kMaterialCount);
            ReCreateMaterialDescPool(kMaterialCount);
            bShouldForceUpdateMaterial = true;
        }

         //更新推送常量  旋转矩阵
        //透视投影矩阵   CameraCompionent 里设置投影矩阵和视图矩阵
        //glm::mat4 kProjMat = XJGetProjMat(renderTarget);
        //glm::mat4 kViewMat = XJGetViewMat(renderTarget);
        // 将投影和视图矩阵赋值给全局UBO
        //mGlobalUbo.projMat = projMat;
        //mGlobalUbo.viewMat = viewMat;


        uint32_t kEntityIndex = 0; // 实体索引，用于动态UBO偏移计算
        //材质是否更新
        std::vector<bool> kUpdateFlags(kMaterialCount);
        kView.each([this, &cmdBuffer, &kUpdateFlags, bShouldForceUpdateMaterial, &kEntityIndex](const auto &entity, const XJTransformComponent& transComp, const XJUnlitMaterialComponent& matComp)
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

                if (!kMaterial->GetShaderPath().empty() &&
                    !IsSameShaderPath(kMaterial->GetShaderPath(), mShaderRuntimeLayout.ShaderPath))
                {
                    spdlog::warn("Skip material {}: shader path '{}' does not match Unlit pipeline shader '{}'.",
                        kMaterial->GetIndex(),
                        kMaterial->GetShaderPath().generic_string(),
                        mShaderRuntimeLayout.ShaderPath.generic_string());
                    continue;
                }
                 
                VkDescriptorSet kParamsDescSet = mMaterialDescSets[kMaterialIndex];
                VkDescriptorSet kResourceDescSet = mMaterialResourceDescSets[kMaterialIndex];

                if(!kUpdateFlags[kMaterialIndex])
                {
                    UpdateMaterialParamsDescSet(kParamsDescSet, kMaterial);
                    kMaterial->FinishFlushParams();
                    
                     // 无条件更新 texture/sampler descriptor（防止首次创建后从未更新）
                    UpdateMaterialResourceDescSet(kResourceDescSet, kMaterial);
                    kMaterial -> FinishFlushResoure();
                    kUpdateFlags[kMaterialIndex] = true;
                }

                VkDescriptorSet kDescriptorSet[] = { mFrameUboDescSet, kParamsDescSet, kResourceDescSet};
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(),
                                        0, ARRAY_SIZE(kDescriptorSet), kDescriptorSet, 0, nullptr);
                
                ModelPC kPC = {transComp.GetModelMatrix()};
                //推送常量
                vkCmdPushConstants(cmdBuffer, mPipelineLayout->XJGetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(kPC), &kPC);
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

    }
    void XJUnlitMaterialSystem::ReCreateMaterialDescPool(uint32_t materialCount) //动态扩容
    {
        XJVulkanDevice *kDevice = XJGetDevice();

        uint32_t kNewDescriptorSetCount = mLastDescriptorSetCount;//最新池子需要放多少个
        if(mLastDescriptorSetCount == 0)
        {
            kNewDescriptorSetCount = NUM_MATERIAL_BATCH;
        }

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
        mMaterialDescSets.clear();
        mMaterialResourceDescSets.clear();
        if(mMaterialDescriptorPool){mMaterialDescriptorPool.reset();}
        //从新申请池子
        std::vector<VkDescriptorPoolSize> kPoolSizes;

        auto paramPoolSizes = BuildDescriptorPoolSizes(mShaderReflection, mShaderRuntimeLayout.MaterialParameterSet, kNewDescriptorSetCount);
        auto resourcePoolSizes = BuildDescriptorPoolSizes(mShaderReflection, mShaderRuntimeLayout.MaterialResourceSet, kNewDescriptorSetCount);

        kPoolSizes.insert(kPoolSizes.end(), paramPoolSizes.begin(), paramPoolSizes.end());

        for (const auto& poolSize : resourcePoolSizes)
            AddDescriptorPoolSize(kPoolSizes, poolSize.type, poolSize.descriptorCount);

        mMaterialDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(kDevice, kNewDescriptorSetCount *2, kPoolSizes);
    

        //申请材质
        mMaterialDescSets = mMaterialDescriptorPool->AllocateDescriptorSet(mMaterialParamDescSetLayout.get(), kNewDescriptorSetCount);
        mMaterialResourceDescSets =  mMaterialDescriptorPool->AllocateDescriptorSet(mMaterialResourceDescSetLayout.get(), kNewDescriptorSetCount);
        assert(mMaterialDescSets.size() == kNewDescriptorSetCount && "Failed to allocateDescriptorset");
        assert(mMaterialResourceDescSets.size() == kNewDescriptorSetCount && "Failed to allocateDescriptorSet");

        //差值用来创建uinform buffer
        mMaterialBuffers.resize(kNewDescriptorSetCount);
        mMaterialBufferSizes.resize(kNewDescriptorSetCount, 0);
        //更新上一次的数量
        mLastDescriptorSetCount = kNewDescriptorSetCount;
    
    }

    void XJUnlitMaterialSystem::EnsureMaterialBuffer(uint32_t materialIndex, uint32_t requiredSize)//确保材质缓冲区的大小足够
    {
        if(requiredSize == 0) { return; }

        if(materialIndex >= mMaterialBuffers.size())
        {
            spdlog::error("Material index {} is out of bounds (max {}).", materialIndex, mMaterialBuffers.size());
            return;
        }

        if(mMaterialBuffers[materialIndex] && mMaterialBufferSizes[materialIndex] == requiredSize)
            return; 
        //
        XJVulkanDevice* kDevice = XJGetDevice();
        mMaterialBuffers[materialIndex] = std::make_shared<XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, requiredSize, nullptr, true);
        mMaterialBufferSizes[materialIndex] = requiredSize;
    }


    void XJUnlitMaterialSystem::UpdateFrameUboDescSet(XJRenderTarget *renderTarget)//更新UBO的结构
    {
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

        mFrameUboBuffer->WriteData(&kFrameUbo);//写数据Ubo

        VkDescriptorBufferInfo bufferInfo = DescriptorSetWriter::BuildBufferInfo(mFrameUboBuffer->XJGetBuffer(), 0, sizeof(kFrameUbo));//ubobuffer
        VkWriteDescriptorSet bufferWrite = DescriptorSetWriter::WriteBuffer(mFrameUboDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo);//写buffer
        DescriptorSetWriter::UpdateDescriptorSets(kDevice->XJGetDevice(), { bufferWrite });
    }

    void XJUnlitMaterialSystem::UpdateMaterialParamsDescSet(VkDescriptorSet descSet, XJUnlitMaterial *material)//更新材质参数
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

        EnsureMaterialBuffer(material->GetIndex(), block.GetSize());

        XJVulkanBuffer* materialBuffer = mMaterialBuffers[material->GetIndex()].get();
        if (!materialBuffer)
            return;
        materialBuffer->WriteData(const_cast<uint8_t*>(block.GetDataPtr()));
        //spdlog::info("Upload material block: material={}, blockSize={}", material->GetIndex(), block.GetSize());
        const XJMaterialUboMemberBinding* uboBinding =
            material->GetParameterLayout().FindFirstUboBinding(material->GetParameterLayout().GetUboName());        

        if (!uboBinding)
        {
            spdlog::warn("Skip material params update: material {} has no UBO binding.", material->GetIndex());
            return;
        }

        VkDescriptorBufferInfo kBufferInfo = DescriptorSetWriter::BuildBufferInfo(materialBuffer->XJGetBuffer(), 0, block.GetSize());
        VkWriteDescriptorSet kBufferWrite = DescriptorSetWriter::WriteBuffer(descSet, uboBinding->Binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &kBufferInfo);
        DescriptorSetWriter::UpdateDescriptorSets(device->XJGetDevice(), { kBufferWrite });
    }

    void XJUnlitMaterialSystem::UpdateMaterialResourceDescSet(VkDescriptorSet descSet, XJUnlitMaterial *material)//材质资源更新
    {
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
