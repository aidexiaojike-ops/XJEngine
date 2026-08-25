#ifndef XJ_RAY_INTERSECTION_H
#define XJ_RAY_INTERSECTION_H

#include "Geometry/XJBoundingBox.h"

#include <glm/glm.hpp>

namespace XJ
{
    struct XJRayAABBHit
    {
        bool Hit = false;

        // 从 RayOrigin 到命中点的世界空间距离。
        float Distance = 0.0f;

        glm::vec3 Position{0.0f};

        // 命中 AABB 表面的世界空间法线。
        glm::vec3 Normal{0.0f};
    };

    bool XJIntersectRayAABB(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const XJBoundingBox& bounds,
        float maxDistance,
        XJRayAABBHit& outHit);
}

#endif