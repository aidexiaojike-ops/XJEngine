#ifndef XJ_VULKAN_GEOMETRY_UTIL_H
#define XJ_VULKAN_GEOMETRY_UTIL_H

#include "Graphic/XJVulkanVertex.h"
#include <cstdint>
#include <vector>

namespace XJ
{
    class XJVulkanGeometryUtil
    {
        public:
            static void CreateCube(
                float leftPlane,
                float rightPlane,
                float bottomPlane,
                float topPlane,
                float nearPlane,
                float farPlane,
                std::vector<XJVulkanVertex>& vertices,
                std::vector<uint32_t>& indices,
                bool bUseTextcoords = true,
                bool bUseNormals = true,
                const glm::mat4& relativeMat = glm::mat4(1.0f));
    };
}

#endif