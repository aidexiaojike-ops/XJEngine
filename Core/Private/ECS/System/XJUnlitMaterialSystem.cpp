#include "ECS/System/XJUnlitMaterialSystem.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanDescriptorSet.h"
#include "Graphic/VulkanImageView.h"
#include "Graphic/XJVulkanFrameBuffer.h"//获取帧缓冲信息
#include "Render/XJRenderTarget.h"//获取渲染目标信息
#include "Graphic/XJVulkanFrameBuffer.h"
#include "XJApplication.h"

#include "Edit/FileUtil.h"//获取文件工具类

#include "ECS/Component/XJTransformComponent.h"


namespace XJ
{
    void XJUnlitMaterialSystem::OnInit(XJVulkanRenderPass *renderPass) 
    {//添加内容查看shader Uniform  UBO

        XJVulkanDevice *kDevice = XJGetDevice();
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
            const std::vector<VkDescriptorSetLayoutBinding> kBindings = 
            {
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,//UNIFORM_BUFFER
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,//  片源作色
                    // kBindings.pImmutableSamplers = 
                }

            };
            mMaterialParamDescSetLayout = std::make_shared<XJVulkanDescriptorSetLayout>(kDevice, kBindings);
        }

        //材质纹理
        {
            const std::vector<VkDescriptorSetLayoutBinding> kBindings = 
            {
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,//纹理采样
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,//顶点着色  片源作色
                },
                {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,//纹理采样
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,//顶点着色  片源作色
                },
            };
            mMaterialResourceDescSetLayout = std::make_shared<XJVulkanDescriptorSetLayout>(kDevice, kBindings);
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
                                      mMaterialParamDescSetLayout->XJGetDescriptorSet()},
            .pushConstantRanges = { kModelPC}
        };
        //资源
        mPipelineLayout = std::make_shared<XJVulkanPipelineLayout>(kDevice,
                                                                   XJ_RES_SHADER_DIR"Unlit.vert",
                                                                   XJ_RES_SHADER_DIR"Unlit.frag", kShaderLayout);

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
        mPipeline->EnableDepthTest(VK_TRUE);//启用深度测试
        mPipeline->SetVertexInputState(kVertexBindings, kVertexAttributes);//设置顶点输入状态
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);//设置输入装配状态 三角形列表
        mPipeline->SetDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
        mPipeline->SetMultisampleState(mSampleCount, VK_FALSE, 0.2f);//设置多重采样状态  4倍采样  不启用样本着色
        mPipeline->Create();

        //描述符 池子
        std::vector<VkDescriptorPoolSize> kPoolSizes = 
        {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = 2
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 2
            },
        };


        mDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(kDevice, 1, kPoolSizes);
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
        kView.each([this, &cmdBuffer, kUpdateFlags, bShouldForceUpdateMaterial, &kEntityIndex](const auto &entity, const XJTransformComponent& transComp, const XJUnlitMaterialComponent& matComp)
        {
            auto kMeshMaterials = matComp.XJGetMeshMaterials();
            for(const auto&entry :kMeshMaterials)//要是没有材质酒放弃渲染
            {
                XJBaseMaterial *kMaterial = entry.first;
                uint32_t kMaterialIndex = kMaterial->XJGetIndex();
                if(!kMaterial || kMaterialIndex < 0) 
                {
                    spdlog::error("TODO: Default material of error material ?");
                    continue;
                }
                
                VkDescriptorSet kParamsDescSet = mMaterialDescSets[kMaterialIndex];
                VkDescriptorSet kResourceDescSet = mMaterialResourceDescSets[kMaterialIndex];

                if(!kUpdateFlags[kMaterialIndex])
                {
                    if(kMaterial->ShouldFlushParams() || bShouldForceUpdateMaterial)
                    {
                        UpdateMaterialParamsDescSet(kParamsDescSet, kMaterial);
                        kMaterial -> FinishFlushParams();
                    }
                    if(kMaterial->ShouldFlushResoure() || bShouldForceUpdateMaterial)
                    {
                        UpdateMaterialResourceDescSet(kResourceDescSet, kMaterial);
                        kMaterial -> FinishFlushResoure();
                    }
                }

                VkDescriptorSet kDescriptorSet[] = { mFrameUboDescSet, kParamsDescSet, kResourceDescSet};
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(),
                                        0, ARRAY_SIZE(kDescriptorSet), kDescriptorSet, 0, nullptr);
                
                ModelPC kPC = {transComp.GetTransform()};
                //推送常量
                vkCmdPushConstants(cmdBuffer, mPipelineLayout->XJGetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(kPC), &kPC);
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
        std::vector<VkDescriptorPoolSize> kPoolSizes = 
        {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = kNewDescriptorSetCount
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = kNewDescriptorSetCount * 2//因为有两个贴图
            },
        };
        mMaterialDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(kDevice, kNewDescriptorSetCount *2, kPoolSizes);
    

        //申请材质
        mMaterialDescSets = mMaterialDescriptorPool->AllocateDescriptorSet(mMaterialParamDescSetLayout.get(), kNewDescriptorSetCount);
        mMaterialResourceDescSets =  mMaterialDescriptorPool->AllocateDescriptorSet(mMaterialResourceDescSetLayout.get(), kNewDescriptorSetCount);
        assert(mMaterialDescSets.size() == kNewDescriptorSetCount && "Failed to allocateDescriptorset");
        assert(mMaterialResourceDescSets.size() == kNewDescriptorSetCount && "Failed to allocateDescriptorSet");

        //差值用来创建uinform buffer
        uint32_t kDiffCount = kNewDescriptorSetCount - mLastDescriptorSetCount;
        for(int i = 0; i < kDiffCount; i++)
        {
            mMaterialBuffers.push_back(std::make_shared<XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UnlitMaterialUbo), nullptr, true));
        }
        //更新上一次的数量
        mLastDescriptorSetCount = kNewDescriptorSetCount;
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
        XJVulkanDevice *kDevice = XJGetDevice();//逻辑设备
        XJVulkanBuffer *kMaterialBuffer = mMaterialBuffers[material->XJGetIndex()].get();

        UnlitMaterialUbo kParams = material->XJGetParams();

        //纹理参数更新
        const TextureView *kTextureA = material->XJGetTextureView(UNLIT_MAT_BASE_COLOR_A);
        if(kTextureA)
        {
            XJMaterial::UpdateTextureParams(kTextureA, &kParams.textureParamA);
        }

        const TextureView *kTextureB = material->XJGetTextureView(UNLIT_MAT_BASE_COLOR_B);
        if(kTextureB)
        {
            XJMaterial::UpdateTextureParams(kTextureB, &kParams.textureParamB);
        }

        kMaterialBuffer->WriteData(&kParams);
        VkDescriptorBufferInfo kBufferInfo = DescriptorSetWriter::BuildBufferInfo(kMaterialBuffer->XJGetBuffer(), 0, sizeof(kParams));
        VkWriteDescriptorSet kBufferWrite = DescriptorSetWriter::WriteBuffer(descSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &kBufferInfo);
        DescriptorSetWriter::UpdateDescriptorSets(kDevice->XJGetDevice(), { kBufferWrite });
    }
    void XJUnlitMaterialSystem::UpdateMaterialResourceDescSet(VkDescriptorSet descSet, XJUnlitMaterial *material)//材质资源更新
    {
        XJVulkanDevice *kDevice = XJGetDevice();//逻辑设备
        //优化
        const TextureView *kTextureA = material->XJGetTextureView(UNLIT_MAT_BASE_COLOR_A);
        const TextureView *kTextureB = material->XJGetTextureView(UNLIT_MAT_BASE_COLOR_B);

        VkDescriptorImageInfo kTextureInfoA = DescriptorSetWriter::BuildImageInfo(kTextureA->sampler->XJGetSampler(), kTextureA->texture->XJGetImageView()->XJGetImageView());
        VkDescriptorImageInfo kTextureInfoB = DescriptorSetWriter::BuildImageInfo(kTextureB->sampler->XJGetSampler(), kTextureB->texture->XJGetImageView()->XJGetImageView());
        //
        VkWriteDescriptorSet kTextureWriteA = DescriptorSetWriter::WriteImage(descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &kTextureInfoA);
        VkWriteDescriptorSet kTextureWriteB = DescriptorSetWriter::WriteImage(descSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &kTextureInfoB);

        DescriptorSetWriter::UpdateDescriptorSets(kDevice->XJGetDevice(), { kTextureWriteA, kTextureWriteB });
    }
}