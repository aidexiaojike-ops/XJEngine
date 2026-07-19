#ifndef XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_H
#define XJ_MATERIAL_SHADER_RUNTIME_LAYOUT_H

#include "Graphic/VulkanCommon.h"
#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Shader/XJShaderReflection.h"

#include <cstdint>
#include <filesystem>
#include <vector>
#include <string>

namespace XJ
{
    /**
     * @brief 材质着色器运行时布局
     *
     * 存储一组着色器（顶点/片元）经反射后得到的资源绑定信息，
     * 用于在运行时创建 Vulkan 描述符集布局。
     */
    struct XJMaterialShaderRuntimeLayout
    {
        std::filesystem::path ShaderPath;
        std::filesystem::path VertexPath;
        std::filesystem::path FragmentPath;
        /// 着色器反射结果，包含所有 uniform、UBO、采样器等信息
        XJShaderReflectionResult Reflection;
        // ---------- 描述符集索引 ----------
        uint32_t FrameSet = 0;
        uint32_t MaterialParameterSet = 1;
        uint32_t MaterialResourceSet = 2;
        // ---------- 主帧 UBO 信息 ----------
        std::string PrimaryFrameUboName;
        uint32_t PrimaryFrameUboSet = 0;
        uint32_t PrimaryFrameUboBinding = 0;
        uint32_t PrimaryFrameUboSize = 0;
            // ---------- 主材质 UBO 信息 ----------
        std::string PrimaryMaterialUboName;
        uint32_t PrimaryMaterialUboSet = 0;
        uint32_t PrimaryMaterialUboBinding = 0;
        uint32_t PrimaryMaterialUboSize = 0;
        
        std::vector<XJMaterialTextureBinding> MaterialSamplerBindings;/// 材质采样器绑定列表（对应到着色器中的每个 sampler）

        std::vector<VkDescriptorSetLayoutBinding> FrameBindings;
        std::vector<VkDescriptorSetLayoutBinding> MaterialParameterBindings;
        std::vector<VkDescriptorSetLayoutBinding> MaterialResourceBindings;
        // ---------- 状态查询辅助函数 ----------
        bool HasPrimaryFrameUbo() const// 是否存在有效的主帧
        {
            return !PrimaryFrameUboName.empty() && PrimaryFrameUboSize > 0;
        }

        bool HasFrameSet() const// 是否存在帧描述符集
        {
            return !FrameBindings.empty();
        }

        bool HasPrimaryMaterialUbo() const // 是否存在有效的主材质
        {
            return !PrimaryMaterialUboName.empty() && PrimaryMaterialUboSize > 0;
        }

        bool HasMaterialParameterSet() const// 是否存在材质参数描述符集
        {
            return !MaterialParameterBindings.empty();
        }

        bool HasMaterialResourceSet() const// 是否存在材质资源描述符集
        {
            return !MaterialResourceBindings.empty();
        }
    };
}

#endif
