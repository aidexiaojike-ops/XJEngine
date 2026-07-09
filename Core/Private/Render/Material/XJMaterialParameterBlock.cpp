#include "Render/Material/XJMaterialParameterBlock.h"
#include <algorithm>
#include <cstring>

namespace XJ
{
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
        return SetBytes(offset, &value, sizeof(value));
    }

    bool XJMaterialParameterBlock::SetVec3(uint32_t offset, const glm::vec3& value)
    {
        return SetBytes(offset, &value, sizeof(value));
    }

    bool XJMaterialParameterBlock::SetVec4(uint32_t offset, const glm::vec4& value)
    {
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