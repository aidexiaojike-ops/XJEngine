#ifndef XJ_VULKAN_VERTEX_H
#define XJ_VULKAN_VERTEX_H

#include "Edit/Mathinclude.h"

namespace XJ
{
    struct XJVulkanVertex
    {
        glm::vec3 position{};
        glm::vec2 texcoord0{};
        glm::vec3 tangent{};
        glm::vec3 normal{};
    };
}

#endif