#pragma once

#include "ECS/Component/XJCameraComponent.h"
#include "ECS/XJReservedUUID.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace XJ::CameraGizmoGeometry
{
    // 局部 -Z 指向画面，顶部标记指示画面上方。
    inline constexpr std::array<glm::vec3, 8> Vertices{{
        {0.0f, 0.0f, 0.0f},
        {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
        {-0.5f, 1.0f, -1.0f}, {0.0f, 1.5f, -1.0f}, {0.5f, 1.0f, -1.0f}
    }};
    inline constexpr std::array<uint32_t, 20> Indices{{
        0, 1, 0, 2, 0, 3, 0, 4,
        1, 2, 2, 3, 3, 4, 4, 1,
        5, 6, 6, 7
    }};

    inline bool TryBuildModel(const XJCameraComponent& camera, glm::mat4& model)
    {
        const auto* entity = camera.XJGetOwner();
        if (!entity || entity->XJGetUUID() == XJUUID(XJ_PREVIEW_CAMERA_UUID))
            return false;

        glm::mat4 cameraView;
        if (!camera.TryGetViewMatrix(cameraView))
            return false;

        const float halfHeight = std::tan(glm::radians(camera.XJGetFov()) * 0.5f);
        const float halfWidth = halfHeight * camera.XJGetAspectRatio();
        if (!std::isfinite(halfHeight) || !std::isfinite(halfWidth))
            return false;

        // 保留视锥角度和宽高比，限制辅助线在场景中的尺寸。
        const float size = 0.8f / std::max({1.0f, halfWidth, halfHeight});
        model = glm::scale(
            glm::affineInverse(cameraView), glm::vec3(halfWidth, halfHeight, 1.0f) * size);
        return true;
    }
}
