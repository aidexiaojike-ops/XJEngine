#include "Render/System/XJBaseMaterialSystem.h"//获取基础材质系统信息
#include "Graphic/XJVulkanPipeline.h"//获取管线信息
#include "Edit/FileUtil.h"//获取文件工具类
#include "Graphic/XJVulkanVertex.h"//获取几何体工具类
#include "Graphic/XJVulkanDescriptorSet.h"//获取描述符集信息
#include "XJApplication.h"//获取应用程序上下文信息

#include "Graphic/XJVulkanFrameBuffer.h"//获取帧缓冲信息
#include "Render/XJRenderTarget.h"//获取渲染目标信息

#include "ECS/XJEntity.h"//获取实体信息

#include "Graphic/XJVulkanImageView.h"//获取图像视图信息
#include "Graphic/XJVulkanPhysicalDevices.h"//获取物理设备信息
#include "ECS/Component/XJCameraComponent.h"//获取摄像机组件信息

#include "Asset/Importer/XJTextureImporter.h"
#include "Render/Resource/XJTextureFactory.h"
#include "Render/Resource/XJTexture.h"

namespace XJ
{
 
    void XJBaseMaterialSystem::OnInit(XJVulkanRenderPass *renderPass)
    {

        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;//获取渲染上下文信息
        XJ::XJVulkanPhysicalDevices *kPhysicalDevices = kRenderContext->XJGetPhysicalDevices();//获取物理设备信息
        XJ::XJVulkanDevice* kDevice = XJGetDevice();//获取逻辑设备信息

        VkPhysicalDevice physicalDevice  = kPhysicalDevices->XJGetPhysicalDevice();
        VkPhysicalDeviceProperties properties;//获取物理设备属性
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);//获取物理设备属性信息
        
        size_t minUboAlignment = properties.limits.minUniformBufferOffsetAlignment;//获取最小统一缓冲区偏移对齐要求
        mDynamicAlignment = sizeof(InstanceUbo);
        if(minUboAlignment > 0)
        {
            mDynamicAlignment = (mDynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);//计算动态对齐大小
        }
        spdlog::info("动态统一缓冲区对齐: {} bytes (InstanceUbo: {} bytes)", mDynamicAlignment, sizeof(InstanceUbo));

        //descriptor set   绑定shader
        std::vector<VkDescriptorSetLayoutBinding> kDesctLayoutBindings
        {
            
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,//使用动态统一缓冲区
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
            {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
            {
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            }
        };

        mDescriptorSetLayout = std::make_shared<XJ::XJVulkanDescriptorSetLayout>(kDevice, kDesctLayoutBindings);


        std::vector<VkDescriptorPoolSize> kPoolSizes = 
        {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = RENDERER_NUM_BUFFER
            },
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = RENDERER_NUM_BUFFER
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = RENDERER_NUM_BUFFER * 2
            },
        };

        mDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(
            kDevice,
            RENDERER_NUM_BUFFER,
            kPoolSizes);
        
        mDescriptorSets = mDescriptorPool->AllocateDescriptorSet(
            mDescriptorSetLayout.get(),
            RENDERER_NUM_BUFFER);
        
        if (mDescriptorSets.size() != RENDERER_NUM_BUFFER)
        {
            spdlog::error("XJBaseMaterialSystem descriptor set allocation failed");
            return;
        }
        if (mDescriptorSets.empty())
        {
            spdlog::error("XJBaseMaterialSystem descriptor set allocation failed");//如果默认会用 mDescriptorSets[0]，这里马上检查
            return;
        }
        //buffer的资源准备
       for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
        {
            // 每个帧槽位一套 UBO buffer，防止当前帧 WriteData 覆盖 GPU 正在读的上一帧。
            mGlobalBuffers[frameSlot] = std::make_shared<XJ::XJVulkanBuffer>(
                kDevice,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                sizeof(GlobalUbo),
                nullptr,
                true);
            
            mInstanceBuffers[frameSlot] = std::make_shared<XJ::XJVulkanBuffer>(
                kDevice,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                MAX_ENTITIES * mDynamicAlignment,
                nullptr,
                true);
        }
        //贴图
        auto kAssetA = XJTextureImporter::ImportTexture(XJ_RES_TEXTURE_DIR"R.png");
        if (kAssetA)
        {
            mTextureA = XJTextureFactory::CreateTextureFromAsset(*kAssetA);
        }

        auto kAssetB = XJTextureImporter::ImportTexture(XJ_RES_TEXTURE_DIR"R-C.jpeg");
        if (kAssetB)
        {
            mTextureB = XJTextureFactory::CreateTextureFromAsset(*kAssetB);
        }

        // 硬编码资源缺失时使用 1x1 占位纹理，避免后续 descriptor 写入时解引用空纹理。
        if (!mTextureA)
        {
            RGBAColor whitePixel{255, 255, 255, 255};
            mTextureA = std::make_shared<XJTexture>(1, 1, &whitePixel);
            spdlog::warn("Texture R.png missing, using 1x1 white fallback.");
        }

        if (!mTextureB)
        {
            RGBAColor blackPixel{0, 0, 0, 255};
            mTextureB = std::make_shared<XJTexture>(1, 1, &blackPixel);
            spdlog::warn("Texture R-C.jpeg missing, using 1x1 black fallback.");
        }
        // 新增：初始化采样器
        mSamplerA = std::make_shared<XJ::XJSampler>(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        mSamplerB = std::make_shared<XJ::XJSampler>(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

        // descriptor 绑定的 buffer/image/sampler 句柄不变，只需要初始化时写一次。
        // 每帧重写正在被 GPU 使用的 descriptor set 会造成 host 写/GPU 读竞争。
        UpdateDescriptorSets();
        
        XJ::ShaderLayout mShaderLayout;
        mShaderLayout.descriptorSetLayouts = {mDescriptorSetLayout->XJGetDescriptorSet()};

        // 添加推送常量范围配置
        //mShaderLayout.pushConstantRanges.resize(1);
        //mShaderLayout.pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        //mShaderLayout.pushConstantRanges[0].offset = 0;
        //mShaderLayout.pushConstantRanges[0].size = sizeof(PushConstants);  // 68字节 (glm::mat4 + uint32_t)

        mPipelineLayout = std::make_shared<XJ::XJVulkanPipelineLayout>(kDevice, 
                                                XJ_RES_SHADER_DIR"Descriptor.vert",
                                                XJ_RES_SHADER_DIR"Descriptor.frag", mShaderLayout);//顶点着色器路径  片元着色器路径
       
        /* 下面是要迁移到其他地方的*/
        // 设置顶点输入状态 - 由于使用gl_VertexIndex，不需要顶点属性
        std::vector<VkVertexInputBindingDescription> vertexBindings{};
        vertexBindings.resize(1);
        vertexBindings[0].binding = 0;//绑定点0
        vertexBindings[0].stride = sizeof(XJ::XJVulkanVertex);//步幅为顶点结构体大小
        vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//每个顶点一个数据

        std::vector<VkVertexInputAttributeDescription> vertexAttributes{};
        vertexAttributes.resize(3);
        //位置属性
        vertexAttributes[0].location = 0;//位置位置0
        vertexAttributes[0].binding = 0;//绑定点0
        vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;//格式  三个32位浮点数
        vertexAttributes[0].offset = offsetof(XJ::XJVulkanVertex, position);//位置偏移    
        //纹理坐标属性
        vertexAttributes[1].location = 1;//纹理坐标位置1
        vertexAttributes[1].binding = 0;//绑定点0
        vertexAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;//格式  两个32位浮点数
        vertexAttributes[1].offset = offsetof(XJ::XJVulkanVertex, texcoord0);//纹理坐标偏移    
        //法线属性
        vertexAttributes[2].location = 2;//法线位置2
        vertexAttributes[2].binding = 0;//绑定点0
        vertexAttributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;//格式  三个32位浮点数
        vertexAttributes[2].offset = offsetof(XJ::XJVulkanVertex, normal);//法线偏移  
        /* */

        mPipeline = std::make_shared<XJ::XJVulkanPipeline>(kDevice, renderPass, mPipelineLayout.get());//创建管线对象
        mPipeline->EnableDepthTest(VK_TRUE);//启用深度测试
        mPipeline->SetVertexInputState(vertexBindings, vertexAttributes);//设置顶点输入状态
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);//设置输入装配状态 三角形列表
        mPipeline->SetDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
        mPipeline->SetMultisampleState(mSampleCount, VK_FALSE, 0.2f);//设置多重采样状态  4倍采样  不启用样本着色
        mPipeline->Create();
        spdlog::info("管线创建成功");
    }

    void XJBaseMaterialSystem::OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget *renderTarget)
    {
        //entt::each
        // XJAppContext *kAppContext = XJApplication::XJGetAppContext();
        XJScene *kScene = XJGetScene();

        if(!kScene){return;}//如果场景不存在，直接返回

        entt::registry &kReg =  kScene->XJGetEcsRegistry();//拿到注射器
        auto kView = kReg.view<XJTransformComponent, XJBaseMaterialComponent>();//获取视图，包含有变换组件、网格组件和基础材质组件的实体

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
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(kFrameBuffer->XJGetWidth());
        viewport.height = static_cast<float>(kFrameBuffer->XJGetHeight());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        //设置裁剪矩形
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {kFrameBuffer->XJGetWidth(), kFrameBuffer->XJGetHeight()};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

      
        //steup global params
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        //更新推送常量  旋转矩阵
        //透视投影矩阵   CameraCompionent 里设置投影矩阵和视图矩阵
        glm::mat4 projMat = XJGetProjMat(renderTarget);
        glm::mat4 viewMat = XJGetViewMat(renderTarget);
        
        // 将投影和视图矩阵赋值给全局UBO
        const uint32_t frameSlot = XJApplication::XJGetAppContext()->renderFrameSlot;
            
        // 先更新当前帧槽位的 CPU UBO，再上传到对应 GPU buffer。
        // 每个 in-flight 帧槽位一份 buffer，避免覆盖 GPU 仍在读取的上一帧数据。
        mGlobalUbo[frameSlot].projMat = projMat;
        mGlobalUbo[frameSlot].viewMat = viewMat;
        mGlobalBuffers[frameSlot]->WriteData(&mGlobalUbo[frameSlot]);
     
        // descriptor set 在 OnInit 写入一次；每帧只更新 UBO buffer 内容。

        uint32_t kEntityIndex = 0; // 实体索引，用于动态UBO偏移计算
        //setup custiom params
        kView.each([this, &cmdBuffer, &kEntityIndex, frameSlot](const auto &entity, const XJTransformComponent& transComp, const XJBaseMaterialComponent& matComp)
        {
            auto kMeshMaterials = matComp.XJGetMeshMaterials();
            for(const auto&entry :kMeshMaterials)//要是没有材质酒放弃渲染
            {
                XJBaseMaterial *kMaterial = entry.first;
                if(!kMaterial) 
                {
                    spdlog::error("TODO: Default material of error material ?");
                    continue;
                }
                //PushConstants pushConstants //具体某个材质
                //{
                //        .matrix = projMat * viewMat * transComp.GetModelMatrix(),
                //        .colorType = kMaterial->colorType 
                //};
                //vkCmdPushConstants(cmdBuffer, mPipelineLayout->XJGetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);
                
                for(const auto&kMeshIndex : entry.second)
                {
                    XJMesh *kMesh = matComp.XJGetMesh(kMeshIndex);
                    //if(kMesh)
                    //{
                    //    kMesh->Draw(cmdBuffer);
                    //}

                    if(kMesh && kEntityIndex < MAX_ENTITIES)
                    {
                        mInstanceUbo[frameSlot].modelMat = transComp.modelMatrix;//设置实例UBO的模型矩阵

                        //计算动态UBO偏移
                        uint32_t kOffset = kEntityIndex * mDynamicAlignment;
                        // 当前帧槽位写入自己的实例缓冲，避免覆盖 GPU 正在读取的其他帧数据。
                        mInstanceBuffers[frameSlot]->WriteDataOffset(&mInstanceUbo[frameSlot], kOffset, sizeof(InstanceUbo));//UBO写入数据偏移

                        //使用动态偏移绑定描述符集并绘制网格
                        uint32_t kDynamicOffset = kOffset; // 计算动态偏移
                        VkDescriptorSet descriptorSet = mDescriptorSets[frameSlot];
                        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(), 0, 1,  &descriptorSet, 1, &kDynamicOffset);
                        kMesh->Draw(cmdBuffer);
                        kEntityIndex++; // 增加实体索引
                    }
                }

              
            }
            
           
           
        });


    }

    void XJBaseMaterialSystem::OnDestroy()
    {
        mPipeline.reset();
        mPipelineLayout.reset();

        mDescriptorSets.clear();
        mDescriptorPool.reset();
        mDescriptorSetLayout.reset();

        for (auto& buffer : mGlobalBuffers)
            buffer.reset();

        for (auto& buffer : mInstanceBuffers)
            buffer.reset();

        mTextureA.reset();
        mTextureB.reset();
        mSamplerA.reset();
        mSamplerB.reset();
    }
    
    void XJBaseMaterialSystem::UpdateDescriptorSets()
    {
        if (!mTextureA || !mTextureA->XJGetImageView() ||
            !mTextureB || !mTextureB->XJGetImageView() ||
            !mSamplerA || !mSamplerB)
        {
            spdlog::error("UpdateDescriptorSets failed: texture or sampler is not ready.");
            return;
        }

        XJ::XJVulkanDevice* kDevice = XJGetDevice();
        if (!kDevice || !kDevice->IsValid())
        {
            spdlog::error("UpdateDescriptorSets failed: device is invalid.");
            return;
        }

        if (mDescriptorSets.size() != RENDERER_NUM_BUFFER)
        {
            spdlog::error("UpdateDescriptorSets failed: descriptor set count is invalid.");
            return;
        }

        for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
        {
            if (mDescriptorSets[frameSlot] == VK_NULL_HANDLE ||
                !mGlobalBuffers[frameSlot] ||
                !mInstanceBuffers[frameSlot])
            {
                spdlog::error("UpdateDescriptorSets failed: frame resource {} is invalid.", frameSlot);
                return;
            }
        }
        std::array<VkDescriptorBufferInfo, RENDERER_NUM_BUFFER> globalBufferInfos{};
        std::array<VkDescriptorBufferInfo, RENDERER_NUM_BUFFER> instanceBufferInfos{};
        std::array<VkDescriptorImageInfo, RENDERER_NUM_BUFFER> textureAImageInfos{};
        std::array<VkDescriptorImageInfo, RENDERER_NUM_BUFFER> textureBImageInfos{};

        std::vector<VkWriteDescriptorSet> writeDescriptorSet;
        writeDescriptorSet.reserve(RENDERER_NUM_BUFFER * 4);

        for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
        {
            globalBufferInfos[frameSlot].buffer = mGlobalBuffers[frameSlot]->XJGetBuffer();
            globalBufferInfos[frameSlot].offset = 0;
            globalBufferInfos[frameSlot].range = sizeof(GlobalUbo);

            instanceBufferInfos[frameSlot].buffer = mInstanceBuffers[frameSlot]->XJGetBuffer();
            instanceBufferInfos[frameSlot].offset = 0;
            instanceBufferInfos[frameSlot].range = sizeof(InstanceUbo);

            textureAImageInfos[frameSlot].sampler = mSamplerA->XJGetSampler();
            textureAImageInfos[frameSlot].imageView = mTextureA->XJGetImageView()->XJGetImageView();
            textureAImageInfos[frameSlot].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            textureBImageInfos[frameSlot].sampler = mSamplerB->XJGetSampler();
            textureBImageInfos[frameSlot].imageView = mTextureB->XJGetImageView()->XJGetImageView();
            textureBImageInfos[frameSlot].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorSet descriptorSet = mDescriptorSets[frameSlot];

            // 每个 descriptor set 固定绑定自己的 per-frame buffer，只在初始化时写一次。
            writeDescriptorSet.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = descriptorSet, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &globalBufferInfos[frameSlot]});
            writeDescriptorSet.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = descriptorSet, .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .pBufferInfo = &instanceBufferInfos[frameSlot]});
            writeDescriptorSet.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = descriptorSet, .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &textureAImageInfos[frameSlot]});
            writeDescriptorSet.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = descriptorSet, .dstBinding = 3, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &textureBImageInfos[frameSlot]});
        }

        vkUpdateDescriptorSets(
            kDevice->XJGetDevice(),
            static_cast<uint32_t>(writeDescriptorSet.size()),
            writeDescriptorSet.data(),
            0,
            nullptr);

    }
}
