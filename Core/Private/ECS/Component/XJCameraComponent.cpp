#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <limits>
#include <spdlog/spdlog.h>

namespace XJ
{
    const glm::mat4& XJCameraComponent::XJGetProjectionMatrix()
    {
        const float aspect = std::max(mAspectRatio, 0.0001f);
        const float nearPlane = std::max(mNear, 0.001f);
        const float farPlane = std::max(mFar, nearPlane + 0.001f);
        projMat = glm::perspective(glm::radians(mFov), aspect, nearPlane, farPlane);
        projMat[1][1] *= -1.0f;
        return projMat;
    }

    const glm::mat4& XJCameraComponent::XJGetViewMatrix()
    {
        // Free/Orbit 只影响控制器，不再在 Getter 中改变 ECS 或轨道参数。
        const auto* owner = XJGetOwner();
        if (!owner || !owner->HasComponent<XJTransformComponent>()) return viewMat;
        XJTransformWorldState world;
        if (owner->GetComponent<XJTransformComponent>().EvaluateWorld(world) != XJTransformStatus::Success)
            return viewMat;
        const glm::dmat4 rotation = glm::mat4_cast(glm::conjugate(world.Rotation));
        const glm::dmat4 candidate = rotation *
            glm::translate(glm::dmat4{1.0}, -glm::dvec3{world.Matrix[3]});
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (!std::isfinite(candidate[column][row]) ||
                    std::abs(candidate[column][row]) > std::numeric_limits<float>::max())
                    return viewMat;
        viewMat = glm::mat4{candidate};
        return viewMat;
    }

    glm::vec3 XJCameraComponent::XJGetPosition() const
    {
        glm::vec3 position{0.0f};
        const auto* owner = XJGetOwner();
        if (owner && owner->HasComponent<XJTransformComponent>())
            owner->GetComponent<XJTransformComponent>().ReadWorldPosition(position);
        return position;
    }

    void XJCameraComponent::XJSetPosition(const glm::vec3& position)
    {
        auto* owner = XJGetOwner();
        if (!owner || !owner->HasComponent<XJTransformComponent>() ||
            owner->GetComponent<XJTransformComponent>().TrySetWorldPosition(glm::dvec3{position}) !=
                XJTransformStatus::Success)
            spdlog::warn("Camera position request could not be applied.");
    }

    void XJCameraComponent::XJSetViewMatrix(const glm::mat4& view)
    {
        viewMat = view;
    }
}
