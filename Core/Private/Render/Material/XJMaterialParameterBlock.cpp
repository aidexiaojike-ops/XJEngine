#include "Render/Material/XJMaterialParameterBlock.h"
#include <algorithm>
#include <cstring>

#include <glm/gtc/type_ptr.hpp>

namespace XJ
{
    // —— GPU std140 布局相关 ——
    // 块内数据严格按 GLSL std140 排布：float/int/bool -> 4 字节；vec2 -> 8 字节；
    // vec3 载荷 -> 12 字节；vec4 -> 16 字节。
    // 以下断言确保 GLM 默认（未对齐）类型尺寸符合预期，防止 GLM 对齐配置意外改变字节布局。
    static_assert(sizeof(glm::vec2) == 8,  "GLM vec2 尺寸异常（std140 应为 8 字节）");
    static_assert(sizeof(glm::vec3) == 12, "GLM vec3 尺寸异常（std140 载荷应为 12 字节，勿开启对齐 gentypes）");
    static_assert(sizeof(glm::vec4) == 16, "GLM vec4 尺寸异常（std140 应为 16 字节）");

    XJMaterialParameterBlock::XJMaterialParameterBlock(uint32_t size)
    {
        Resize(size);
    }

    void XJMaterialParameterBlock::Resize(uint32_t size)
    {
        mData.resize(size);
    }

    void XJMaterialParameterBlock::Clear()
    {
        std::fill(mData.begin(), mData.end(), static_cast<uint8_t>(0));//将数据清零
    }

    uint32_t XJMaterialParameterBlock::GetSize() const
    {
        return static_cast<uint32_t>(mData.size());
    }

     bool XJMaterialParameterBlock::Empty() const
    {
        return mData.empty();
    }

    bool XJMaterialParameterBlock::SetBytes(uint32_t offset, const void* data, uint32_t size)
    {
        if (!data || !CanWrite(offset, size))
            return false;

        std::memcpy(mData.data() + offset, data, size);
        return true;
    }


    //设置材质参数块中的各种类型数据
    bool XJMaterialParameterBlock::SetFloat(uint32_t offset, float value)
    {
        return SetBytes(offset, &value, sizeof(value));
    }

    bool XJMaterialParameterBlock::SetInt(uint32_t offset, int value)
    {
        return SetBytes(offset, &value, sizeof(value));
    }

    bool XJMaterialParameterBlock::SetBool(uint32_t offset, bool value)
    {
        // GLSL bool in std140 is represented as a 32-bit scalar.
        int encodedValue = value ? 1 : 0;
        return SetBytes(offset, &encodedValue, sizeof(encodedValue));
    }

    bool XJMaterialParameterBlock::SetVec2(uint32_t offset, const glm::vec2& value)
    {
        // std140：vec2 对齐 8、载荷 8 字节，sizeof 不受 GLM 对齐配置影响，可整段写入。
        return SetBytes(offset, &value, sizeof(value));
    }

    bool XJMaterialParameterBlock::SetVec3(uint32_t offset, const glm::vec3& value)
    {
        // std140：vec3 对齐 16、载荷仅 12 字节，末 4 字节是填充，由调用方 offset（shader 反射）保证对齐。
        // 若 GLM 启用了 GLM_FORCE_DEFAULT_ALIGNED_GENTYPES，sizeof(glm::vec3)==16，
        // 直接写 sizeof(value) 会把 4 字节 padding 垃圾写进紧邻的下一个成员，导致 shader 读到脏数据。
        // 这里固定只写 12 字节载荷，使布局不依赖 GLM 的编译配置（详见 Mathinclude.h 对齐策略说明）。
        const uint32_t std140Vec3PayloadSize = 12u;
        return SetBytes(offset, glm::value_ptr(value), std140Vec3PayloadSize);
    }

    bool XJMaterialParameterBlock::SetVec4(uint32_t offset, const glm::vec4& value)
    {
        // std140：vec4 对齐 16、载荷 16 字节，sizeof 不受 GLM 对齐配置影响，可整段写入。
        return SetBytes(offset, &value, sizeof(value));
    }

    
    const uint8_t* XJMaterialParameterBlock::GetDataPtr() const
    {
        return mData.empty() ? nullptr : mData.data();
    }
    bool XJMaterialParameterBlock::CanWrite(uint32_t offset, uint32_t size) const
    {
        if (size == 0)
            return true;

        if (offset > mData.size())
            return false;

        return size <= mData.size() - offset;
    }

}