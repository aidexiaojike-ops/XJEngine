#ifndef XJ_BOUNDING_BOX_H
#define XJ_BOUNDING_BOX_H

#include <glm/glm.hpp>

#include <limits>

namespace XJ
{
    // Axis-Aligned Bounding Box。
    // 默认构造表示空包围盒，Expand 后才变为有效。
    struct XJBoundingBox
    {
        glm::vec3 Min{std::numeric_limits<float>::max()};
        glm::vec3 Max{std::numeric_limits<float>::lowest()};

        bool IsValid() const
        {
            return Min.x <= Max.x &&
                   Min.y <= Max.y &&
                   Min.z <= Max.z ;
        }

        void Reset()
        {
            Min = glm::vec3(std::numeric_limits<float>::max());
            Max = glm::vec3(std::numeric_limits<float>::lowest());
        }

        void Expand(const glm::vec3 &point)//计算bound
        {
            Min = glm::min(Min, point);
            Max = glm::max(Max, point);
        }

        void Merge(const XJBoundingBox& other)
        {
            if (!other.IsValid())
                return;

            if (!IsValid())
            {
                *this = other;
                return;
            }

            Min = glm::min(Min, other.Min);
            Max = glm::max(Max, other.Max);
        }

        glm::vec3 GetCenter() const//获取中心
        {
            return IsValid() ? (Min + Max) * 0.5f : glm::vec3(0.0f);
        }

        glm::vec3 GetExtents() const//获取范围
        {
            return IsValid() ? (Max - Min) * 0.5f : glm::vec3(0.0f);
        }

        // 把 local-space AABB 转换为 world-space AABB。
        // 使用 abs(linearMatrix) * extents，支持旋转、负缩放和非等比缩放。
        XJBoundingBox Transformed(const glm::mat4& transform) const
        {
            XJBoundingBox result;

            if(!IsValid())
                return result;
            //获取原来的属性
            const glm::vec3 center = GetCenter();
            const glm::vec3 extents = GetExtents();
            //转化为世界位置
            const glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(center, 1.0f));

            const glm::mat3 linear(transform);

            const glm::vec3 worldExtents = 
                glm::abs(linear[0]) * extents.x +
                glm::abs(linear[1]) * extents.y +
                glm::abs(linear[2]) * extents.z;
            //取范围
            result.Min = worldCenter - worldExtents;
            result.Max = worldCenter + worldExtents;
            
            return result;
        }

    };
}

#endif
