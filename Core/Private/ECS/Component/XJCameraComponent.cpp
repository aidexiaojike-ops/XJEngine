#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "Camera/XJCameraMath.h"

#include <algorithm>

namespace XJ
{
    namespace
    {   
        constexpr float MIN_ASPECT_RATIO = 0.0001f;//constexpr 这个变量、函数或对象的值在编译期间就能确定
        constexpr float MIN_NEAR_PLANE = 0.001f;
        constexpr float MIN_FAR_PLANE_OFFSET = 0.001f;
        constexpr float MIN_RADIUS = 0.1f;
    }
 
    const glm::mat4& XJCameraComponent::XJGetProjectionMatrix()
    {
        const float aspect = std::max(mAspectRatio, MIN_ASPECT_RATIO);
        const float nearPlane = std::max(mNear, MIN_NEAR_PLANE);
        const float farPlane = std::max(mFar, nearPlane + MIN_FAR_PLANE_OFFSET);

        // 使用保护后的 aspect/near/far，避免外部传入 0 或负数导致投影矩阵无效。
        projMat = glm::perspective(glm::radians(mFov), aspect, nearPlane, farPlane);
        projMat[1][1] *= -1.0f; // Vulkan NDC 的 Y 方向与 GLM 默认约定相反。

        return projMat;
    }

    const glm::mat4& XJCameraComponent::XJGetViewMatrix()
    {
       if (mMode == CameraMode::Orbit)
            return UpdateOrbitView();
        else
            return UpdateFreeView();
    }

    void XJCameraComponent::XJSetViewMatrix(const glm::mat4& view)
    {
        //TODO: 这里可以添加一些额外的逻辑，例如更新摄像机位置、旋转等
        viewMat = view;
    }
    // 轨道模式：围绕 mTarget 旋转，摄像机位置由 TransformComponent 的 rotation 和 mRadius 计算
    const glm::mat4& XJCameraComponent::UpdateOrbitView()
    {
        XJEntity* owner = XJGetOwner();
        if (owner && owner->HasComponent<XJTransformComponent>())
        {
            auto& transform = owner->GetComponent<XJTransformComponent>();

            const float yaw = transform.rotation.x;
            const float pitch = CameraMath::ClampPitch(transform.rotation.y);
            transform.rotation.y = pitch;

            const glm::vec3 forward = CameraMath::BuildForwardFromYawPitch(yaw, pitch);
            const float radius = std::max(mRadius, MIN_RADIUS);
            mRadius = radius;

            // Orbit 模式中 forward 表示从目标点指向摄像机的方向。
            transform.position = mTarget + forward * radius;
            mPosition = transform.position;

            const glm::vec3 viewDirection = glm::normalize(mTarget - transform.position);
            const glm::vec3 up = CameraMath::BuildSafeUp(viewDirection);

            viewMat = glm::lookAt(transform.position, mTarget, up);
        }

        return viewMat;
    }
     // 自由模式：直接使用 TransformComponent 的位置和旋转构造视图矩阵
    const glm::mat4& XJCameraComponent::UpdateFreeView()
    {
        XJEntity* owner = XJGetOwner();
        if (owner && owner->HasComponent<XJTransformComponent>())
        {
            auto& transform = owner->GetComponent<XJTransformComponent>();

            const float yaw = transform.rotation.x;
            const float pitch = CameraMath::ClampPitch(transform.rotation.y);
            transform.rotation.y = pitch;

            const glm::vec3 forward = CameraMath::BuildForwardFromYawPitch(yaw, pitch);
            const glm::vec3 up = CameraMath::BuildSafeUp(forward);

            mPosition = transform.position;
            viewMat = glm::lookAt(transform.position, transform.position + forward, up);
        }

        return viewMat;
    }
}
