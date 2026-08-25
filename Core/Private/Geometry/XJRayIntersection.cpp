#include "Geometry/XJRayIntersection.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace XJ
{
    bool XJIntersectRayAABB(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const XJBoundingBox& bounds,
        float maxDistance,
        XJRayAABBHit& outHit)
    {
        outHit = {};

        if (!bounds.IsValid() || maxDistance <= 0.0f)//是否有包围盒
        {
            return false;
        }

        const float directionLength = glm::length(rayDirection);//射线长度

        constexpr float epsilon = 0.000001f;

        if (directionLength <= epsilon)
            return false;

        // 归一化后，t 才能表示实际世界空间距离。
        const glm::vec3 direction = rayDirection / directionLength;

        float nearDistance = -std::numeric_limits<float>::infinity();

        float farDistance = std::numeric_limits<float>::infinity();

        glm::vec3 nearNormal{0.0f};
        glm::vec3 farNormal{0.0f};


        for (int axis = 0; axis < 3; ++axis)
        {
            const float origin = rayOrigin[axis];
            const float directionValue = direction[axis];

            const float minimum = bounds.Min[axis];
            const float maximum = bounds.Max[axis];

            // 射线与当前轴平行。
            if (std::abs(directionValue) <= epsilon)
            {
                // Origin 不在 slab 内，永远不可能命中。
                if (origin < minimum ||
                    origin > maximum)
                {
                    return false;
                }

                continue;
            }

            const float inverseDirection = 1.0f / directionValue;

            float firstDistance = (minimum - origin) * inverseDirection;
            float secondDistance = (maximum - origin) * inverseDirection;

            glm::vec3 firstNormal{0.0f};
            glm::vec3 secondNormal{0.0f};

            // Min 平面的外法线指向负轴。
            firstNormal[axis] = -1.0f;
            // Max 平面的外法线指向正轴。
            secondNormal[axis] = 1.0f;

            if (firstDistance > secondDistance)
            {
                std::swap(firstDistance, secondDistance);

                std::swap(firstNormal, secondNormal);
            }

            if (firstDistance > nearDistance)
            {
                nearDistance = firstDistance;
                nearNormal = firstNormal;
            }

            if (secondDistance < farDistance)
            {
                farDistance = secondDistance;
                farNormal = secondNormal;
            }

            // 三个轴的区间没有公共部分。
            if (nearDistance > farDistance)
                return false;
        }

        // 整个 AABB 位于射线后方。
        if (farDistance < 0.0f)
            return false;

        float hitDistance = 0.0f;
        glm::vec3 hitNormal{0.0f};

        if (nearDistance >= 0.0f)
        {
            // 射线从 AABB 外部进入。
            hitDistance = nearDistance;
            hitNormal = nearNormal;
        }
        else
        {
            // RayOrigin 位于 AABB 内部，使用离开盒子的交点。
            hitDistance = farDistance;
            hitNormal = farNormal;
        }

        if (hitDistance < 0.0f ||  hitDistance > maxDistance)
        {
            return false;
        }

        outHit.Hit = true;
        outHit.Distance = hitDistance;
        outHit.Position = rayOrigin + direction * hitDistance;
        outHit.Normal = hitNormal;

        return true;
    }
}
