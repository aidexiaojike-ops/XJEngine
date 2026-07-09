#ifndef XJ_MATERIAL_PARAMETER_BLOCK_H
#define XJ_MATERIAL_PARAMETER_BLOCK_H

#include "Edit/Mathinclude.h"

#include <cstdint>
#include <vector>

namespace XJ
{
    class XJMaterialParameterBlock//材质参数块
    {
        private:
            bool CanWrite(uint32_t offset, uint32_t size) const;//检查是否可以写入数据

            std::vector<uint8_t> mData;
            /* data */
        public:
            XJMaterialParameterBlock() = default;
            explicit XJMaterialParameterBlock(uint32_t size);//explicit 关键字用于构造函数，防止隐式类型转换

            void Resize(uint32_t size);//重新分配内存
            void Clear();//清空数据

            uint32_t GetSize() const;//获取数据大小
            bool Empty() const;//判断数据是否为空

            bool SetBytes(uint32_t offset, const void* data, uint32_t size);//设置数据

            bool SetFloat(uint32_t offset, float value);
            bool SetInt(uint32_t offset, int value);
            bool SetBool(uint32_t offset, bool value);

            bool SetVec2(uint32_t offset, const glm::vec2& value);
            bool SetVec3(uint32_t offset, const glm::vec3& value);
            bool SetVec4(uint32_t offset, const glm::vec4& value);

            const std::vector<uint8_t>& GetData() const {return mData;}//获取数据;
            const uint8_t* GetDataPtr() const;

    };
 
}


#endif // XJ_MATERIAL_PARAMETER_BLOCK_H