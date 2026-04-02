#include "ECS/System/XJBaseMaterialSystem.h"//获取基础材质系统信息
#include "Graphic/XJVulkanPipeline.h"//获取管线信息
#include "Edit/FileUtil.h"//获取文件工具类
#include "Graphic/XJVulkanGeometryUtil.h"//获取几何体工具类
#include "Graphic/XJVulkanDescriptorSet.h"//获取描述符集信息
#include "XJApplication.h"//获取应用程序上下文信息

#include "Graphic/XJVulkanFrameBuffer.h"//获取帧缓冲信息
#include "Render/XJRenderTarget.h"//获取渲染目标信息

#include "ECS/XJEntity.h"//获取实体信息

#include "Graphic/VulkanImageView.h"//获取图像视图信息
#include "Graphic/VulkanPhysicalDevices.h"//获取物理设备信息

namespace XJ
{
 
    void XJBaseMaterialSystem::OnInit(XJVulkanRenderPass *renderPass)
    {

        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;//获取渲染上下文信息
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();//获取逻辑设备信息
        XJ::VulkanPhysicalDevices *kPhysicalDevices = kRenderContext->XJGetPhysicalDevices();//获取物理设备信息

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


        std::vector<VkDescriptorPoolSize> poolSizes = 
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


        mDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(kDevice, 1, poolSizes);
        mDescriptorSets = mDescriptorPool->AllocateDescriptorSet(mDescriptorSetLayout.get(), 1);
        //buffer的资源准备
        mGlobalBuffer = std::make_shared<XJ::XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(mGlobalUbo),nullptr,true);
        mInstanceBuffer = std::make_shared<XJ::XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, MAX_ENTITIES * mDynamicAlignment, nullptr, true);
        //贴图
        mTextureA = std::make_shared<XJ::XJTexture>(XJ_RES_TEXTURE_DIR"R.png");
        mTextureB = std::make_shared<XJ::XJTexture>(XJ_RES_TEXTURE_DIR"R-C.jpeg");


        XJ::ShaderLayout mShaderLayout;
        mShaderLayout.descriptorSetLayouts = {mDescriptorSetLayout->XJGetDescriptorSet()};

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
        XJAppContext *kAppContext = XJApplication::XJGetAppContext();
        XJScene *kScene = kAppContext->scene;

        if(!kScene){return;}//如果场景不存在，直接返回

        entt::registry &kReg =  kScene->XJGetEcsRegistry();//拿到注射器
        auto kView = kReg.view<XJTransformComponent, XJMeshComponent, XJBaseMaterialComponent>();//获取视图，包含有变换组件、网格组件和基础材质组件的实体

        if (kView.end() == kView.begin()) 
        {
            return;   // 视图确实为空
        }
      

         //bind pipeline
        mPipeline->BindPipeline(cmdBuffer);//绑定管线
        //vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(), 0, 1,  mDescriptorSets.data(), 0, nullptr);
        XJ::XJVulkanFrameBuffer *kFrameBuffer = renderTarget->XJGetCurrentFrameBuffer();
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

        // 更新全局UBO
        mGlobalBuffer->WriteData(&mGlobalUbo);
        
        //steup global params
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        //更新推送常量  旋转矩阵
        //透视投影矩阵   CameraCompionent 里设置投影矩阵和视图矩阵
       mGlobalUbo.projMat = glm::perspective(glm::radians(65.0f), static_cast<float>(kFrameBuffer->XJGetWidth()) * 1.0f / static_cast<float>(kFrameBuffer->XJGetHeight()), 0.1f, 100.0f);
        mGlobalUbo.projMat[1][1] *= -1;  // 取消注释，启用 Y 轴翻转
        mGlobalUbo.viewMat = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        
         // 在循环外更新描述符集（只更新一次）
        UpdateDescriptorSets(cmdBuffer);

        uint32_t kEntityIndex = 0; // 实体索引，用于动态UBO偏移计算
        //setup custiom params
        kView.each([this, &cmdBuffer, &kEntityIndex](const auto &entity, const XJTransformComponent& transComp, XJMeshComponent& meshComp,const XJBaseMaterialComponent& matComp)
        {
            
            if(meshComp.mMesh && kEntityIndex < MAX_ENTITIES)
            {
                mInstanceUbo.modelMat = transComp.modelMatrix;//设置实例UBO的模型矩阵

                //计算动态UBO偏移
                uint32_t kOffset = kEntityIndex * mDynamicAlignment;
                //更新实例UBO数据到动态统一缓冲区
                mInstanceBuffer->WriteDataOffset(&mInstanceUbo, kOffset, sizeof(InstanceUbo));//UBO写入数据偏移

                //使用动态偏移绑定描述符集并绘制网格
                uint32_t kDynamicOffset = kOffset; // 计算动态偏移
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(), 0, 1,  mDescriptorSets.data(), 1, &kDynamicOffset);
                meshComp.mMesh->Draw(cmdBuffer);
                kEntityIndex++; // 增加实体索引
            }
           
        });


    }

    void XJBaseMaterialSystem::OnDestroy()
    {
        mPipeline.reset();
        mPipelineLayout.reset();
        mDescriptorSetLayout.reset();
        mDescriptorPool.reset();
    }
    void XJBaseMaterialSystem::UpdateDescriptorSets(VkCommandBuffer cmdBuffer)
    {
        XJ::XJRenderContext *kRenderContext = XJ::XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();

        VkDescriptorBufferInfo globalBufferInfo{};
        globalBufferInfo.buffer = mGlobalBuffer->XJGetBuffer();
        globalBufferInfo.offset = 0;
        globalBufferInfo.range = sizeof(mGlobalUbo);

        VkDescriptorBufferInfo instanceBufferInfo{};
        instanceBufferInfo.buffer = mInstanceBuffer->XJGetBuffer();
        instanceBufferInfo.offset = 0;
        instanceBufferInfo.range = sizeof(InstanceUbo);//// 单个实例大小

        VkDescriptorImageInfo textureAImageBufferInfo{};
        textureAImageBufferInfo.sampler = mTextureA->XJGetSampler();
        textureAImageBufferInfo.imageView = mTextureA->XJGetImageView()->XJGetImageView();
        textureAImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo textureBImageBufferInfo{};
        textureBImageBufferInfo.sampler = mTextureB->XJGetSampler();
        textureBImageBufferInfo.imageView = mTextureB->XJGetImageView()->XJGetImageView();
        textureBImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorSet descriptorSet = mDescriptorSets[0];

        std::vector<VkWriteDescriptorSet> writeDescriptorSet = 
        {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &globalBufferInfo
                
            },//全局参数
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .pBufferInfo = &instanceBufferInfo
                
            },//实例参数
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo  = &textureAImageBufferInfo
                
            },//贴图A
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo  = &textureBImageBufferInfo
                
            }//贴图B
        };

        vkUpdateDescriptorSets(kDevice->XJGetDevice(),
        writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);

    }
}