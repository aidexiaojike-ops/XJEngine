#include "Camera/XJCameraController.h"
#include "Camera/XJCameraMath.h"

#include "ECS/XJEntity.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"

namespace XJ
{
    namespace
    {
        glm::mat3 BuildCameraBasis(const glm::vec3& forward)
        {
            const glm::vec3 right = CameraMath::BuildRightFromForward(forward);
            const glm::vec3 up = glm::normalize(glm::cross(right, forward));
            return glm::mat3(right, up, forward);
        }

        bool ReadCameraPose(const XJTransformComponent& transform, glm::vec3& position, glm::vec3& forward)
        {
            glm::dquat rotation;
            if (transform.ReadWorldPosition(position) != XJTransformStatus::Success ||
                transform.ReadWorldRotation(rotation) != XJTransformStatus::Success)
                return false;
            forward = glm::vec3{rotation * glm::dvec3{0, 0, -1}};
            return true;
        }

        glm::vec3 DragForward(const glm::vec3& forward, const glm::vec2& delta, float sensitivity)
        {
            // yaw/pitch 仅用于鼠标控制，不存入 Transform 分量。
            const float yaw = glm::degrees(std::atan2(forward.z, forward.x)) + delta.x * sensitivity;
            const float pitch = glm::degrees(std::asin(glm::clamp(forward.y, -1.0f, 1.0f))) + delta.y * sensitivity;
            return CameraMath::BuildForwardFromYawPitch(yaw, pitch);
        }

        bool ApplyOrbit(XJTransformComponent& transform, const XJCameraComponent& camera,
                        const glm::vec3& forward, float radius)
        {
            return transform.TrySetWorldPose(
                glm::dvec3{camera.XJGetTarget()} - glm::dvec3{forward} * static_cast<double>(radius),
                CameraMath::RotationFromForward(forward)) == XJTransformStatus::Success;
        }
    }

    XJCameraController::XJCameraController(
        float mouseSensitivity,
        float cameraMoveSpeed,
        float cameraZoomSpeed,
        float cameraRotateSpeed)
        : mMouseSensitivity(mouseSensitivity),
          mCameraMoveSpeed(cameraMoveSpeed),
          mCameraZoomSpeed(cameraZoomSpeed),
          mCameraRotateSpeed(cameraRotateSpeed),
          mLastMousePos(0.0f, 0.0f),
          mFirstMouseDrag(true),
          mLeftButtonDown(false),
          mRightButtonDown(false)
    {
    }

    void XJCameraController::SetOrbitPivot(const glm::vec3& worldPosition)
    {
        mOrbitPivot = worldPosition;
        mHasOrbitPivot = true;
    }

    void XJCameraController::ClearOrbitPivot()
    {
        mHasOrbitPivot = false;
        mOrbitPivot = glm::vec3(0.0f);
    }

    void XJCameraController::UpdateCameraControl(
        float deltaTime,
        XJGlfwWindow* window,
        XJEntity* cameraEntity)
    {
        if (!window || !cameraEntity || !XJEntity::HasComponent<XJCameraComponent>(cameraEntity))
            return;

        auto& cameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        CameraMode mode = cameraComp.XJGetCameraMode();

        if (mode == CameraMode::Free)
            UpdateFreeCamera(deltaTime, window, cameraEntity);
        else if (mode == CameraMode::Orbit)
            UpdateOrbitCamera(deltaTime, window, cameraEntity);
    }

    void XJCameraController::OnMouseScroll(float yOffset, XJEntity* cameraEntity)
    {
        if (!std::isfinite(yOffset)) return;
        if (!cameraEntity || !XJEntity::HasComponent<XJCameraComponent>(cameraEntity))
            return;

        auto& cameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        CameraMode mode = cameraComp.XJGetCameraMode();

        if (mode == CameraMode::Free)
        {
            if (!XJEntity::HasComponent<XJTransformComponent>(cameraEntity))
                 return;
                    
             auto& transformComp = cameraEntity->GetComponent<XJTransformComponent>();
                    
              glm::vec3 position, forward;
              if (!ReadCameraPose(transformComp, position, forward)) return;
              transformComp.TrySetWorldPosition(glm::dvec3{position} +
                  glm::dvec3{forward} * static_cast<double>(yOffset * mCameraMoveSpeed * 10.0f));
        }
        else if (mode == CameraMode::Orbit)
        {
            float radius = cameraComp.XJGetRadius() + yOffset * -mCameraZoomSpeed;
            if (radius < 0.1f)
                radius = 0.1f;

            if (!XJEntity::HasComponent<XJTransformComponent>(cameraEntity)) return;
            auto& transform = cameraEntity->GetComponent<XJTransformComponent>();
            glm::vec3 position, forward;
            if (ReadCameraPose(transform, position, forward) && ApplyOrbit(transform, cameraComp, forward, radius))
                cameraComp.XJSetRadius(radius);
        }
    }

    void XJCameraController::UpdateFreeCamera(
        float deltaTime,
        XJGlfwWindow* window,
        XJEntity* cameraEntity)
    {
        if (!XJEntity::HasComponent<XJTransformComponent>(cameraEntity))
            return;

        auto& transformComp = cameraEntity->GetComponent<XJTransformComponent>();

        bool leftDown = window->IsMouseDown(MOUSE_BUTTON_LEFT);
        bool rightDown = window->IsMouseDown(MOUSE_BUTTON_RIGHT);

        if (!leftDown && !rightDown)
        {
            mFirstMouseDrag = true;
            mLeftButtonDown = false;
            mRightButtonDown = false;

            glm::vec2 mousePos;
            window->XJGetMousePos(mousePos);
            mLastMousePos = mousePos;
            return;
        }

        glm::vec2 mousePos;
        window->XJGetMousePos(mousePos);

        glm::vec2 mouseDelta = mousePos - mLastMousePos;
        mLastMousePos = mousePos;

        if (glm::length(mouseDelta) < 0.1f)
            return;

        if (mFirstMouseDrag)
        {
            mFirstMouseDrag = false;
            return;
        }

        if (leftDown)
        {
            glm::vec3 position, oldForward;
            if (!ReadCameraPose(transformComp, position, oldForward)) return;
            const glm::vec3 newForward = DragForward(oldForward, mouseDelta, mMouseSensitivity);

            if (mHasOrbitPivot)
            {
                const glm::mat3 rotationDelta =
                    BuildCameraBasis(newForward) * glm::transpose(BuildCameraBasis(oldForward));
                position = mOrbitPivot + rotationDelta * (position - mOrbitPivot);
            }

            transformComp.TrySetWorldPose(glm::dvec3{position}, CameraMath::RotationFromForward(newForward));
        }

        if (rightDown)
        {
            glm::vec3 position, forward;
            if (!ReadCameraPose(transformComp, position, forward)) return;
                    
            const glm::vec3 right = CameraMath::BuildRightFromForward(forward);
            const glm::vec3 up = glm::normalize(glm::cross(right, forward));
                    
            const glm::vec3 moveVector =
                (right * -mouseDelta.x + up * mouseDelta.y) * mCameraMoveSpeed;
                    
            if (transformComp.TrySetWorldPosition(glm::dvec3{position} + glm::dvec3{moveVector}) ==
                XJTransformStatus::Success && mHasOrbitPivot)
                mOrbitPivot += moveVector;
        }
    }

    void XJCameraController::UpdateOrbitCamera(
        float deltaTime,
        XJGlfwWindow* window,
        XJEntity* cameraEntity)
    {
        if (!XJEntity::HasComponent<XJTransformComponent>(cameraEntity))
            return;

        auto& cameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        auto& transformComp = cameraEntity->GetComponent<XJTransformComponent>();

        bool leftDown = window->IsMouseDown(MOUSE_BUTTON_LEFT);
        bool rightDown = window->IsMouseDown(MOUSE_BUTTON_RIGHT);

        if (!leftDown && !rightDown)
        {
            mFirstMouseDrag = true;
            mLeftButtonDown = false;
            mRightButtonDown = false;

            glm::vec2 mousePos;
            window->XJGetMousePos(mousePos);
            mLastMousePos = mousePos;
            return;
        }

        glm::vec2 mousePos;
        window->XJGetMousePos(mousePos);

        glm::vec2 mouseDelta = mousePos - mLastMousePos;
        mLastMousePos = mousePos;

        if (glm::length(mouseDelta) < 0.1f)
            return;

        if (mFirstMouseDrag)
        {
            mFirstMouseDrag = false;
            return;
        }

        if (leftDown)
        {
            glm::vec3 position, forward;
            if (!ReadCameraPose(transformComp, position, forward)) return;
            ApplyOrbit(transformComp, cameraComp, DragForward(forward, mouseDelta, mMouseSensitivity),
                       cameraComp.XJGetRadius());
        }

        if (rightDown)
        {
            float radius = cameraComp.XJGetRadius() + mouseDelta.y * -mCameraZoomSpeed;
            if (radius < 0.1f)
                radius = 0.1f;

            glm::vec3 position, forward;
            if (ReadCameraPose(transformComp, position, forward) && ApplyOrbit(transformComp, cameraComp, forward, radius))
                cameraComp.XJSetRadius(radius);
        }
    }
}
