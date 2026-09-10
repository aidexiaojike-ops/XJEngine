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
        if (!cameraEntity || !XJEntity::HasComponent<XJCameraComponent>(cameraEntity))
            return;

        auto& cameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        CameraMode mode = cameraComp.XJGetCameraMode();

        if (mode == CameraMode::Free)
        {
            if (!XJEntity::HasComponent<XJTransformComponent>(cameraEntity))
                 return;
                    
             auto& transformComp = cameraEntity->GetComponent<XJTransformComponent>();
                    
              const glm::vec3 forward = CameraMath::BuildForwardFromYawPitch(
                  transformComp.rotation.x,
                  transformComp.rotation.y);
                
             transformComp.position += forward * yOffset * mCameraMoveSpeed * 10.0f;
             transformComp.UpdateModelMatrix();
        }
        else if (mode == CameraMode::Orbit)
        {
            float radius = cameraComp.XJGetRadius() + yOffset * -mCameraZoomSpeed;
            if (radius < 0.1f)
                radius = 0.1f;

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
            float yaw = transformComp.rotation.x;
            float pitch = transformComp.rotation.y;

            const glm::vec3 oldForward = CameraMath::BuildForwardFromYawPitch(yaw, pitch);

            yaw += mouseDelta.x * mMouseSensitivity;
            pitch += mouseDelta.y * mMouseSensitivity;
            pitch = CameraMath::ClampPitch(pitch);

            if (mHasOrbitPivot)
            {
                const glm::vec3 newForward = CameraMath::BuildForwardFromYawPitch(yaw, pitch);
                const glm::mat3 rotationDelta =
                    BuildCameraBasis(newForward) * glm::transpose(BuildCameraBasis(oldForward));
                transformComp.position = mOrbitPivot + rotationDelta * (transformComp.position - mOrbitPivot);
            }

            transformComp.rotation.x = yaw;
            transformComp.rotation.y = pitch;
            transformComp.UpdateModelMatrix();
        }

        if (rightDown)
        {
            const glm::vec3 forward = CameraMath::BuildForwardFromYawPitch(
                transformComp.rotation.x,
                transformComp.rotation.y);
                    
            const glm::vec3 right = CameraMath::BuildRightFromForward(forward);
            const glm::vec3 up = glm::normalize(glm::cross(right, forward));
                    
            const glm::vec3 moveVector =
                (right * -mouseDelta.x + up * mouseDelta.y) * mCameraMoveSpeed;
                    
            transformComp.position += moveVector;
            if (mHasOrbitPivot)
                mOrbitPivot += moveVector;
            transformComp.UpdateModelMatrix();
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
            float yaw = transformComp.rotation.x;
            float pitch = transformComp.rotation.y;

            yaw += mouseDelta.x * mMouseSensitivity;
            pitch += mouseDelta.y * mMouseSensitivity;
            pitch = CameraMath::ClampPitch(pitch);

            transformComp.rotation.x = yaw;
            transformComp.rotation.y = pitch;
            transformComp.UpdateModelMatrix();
        }

        if (rightDown)
        {
            float radius = cameraComp.XJGetRadius() + mouseDelta.y * -mCameraZoomSpeed;
            if (radius < 0.1f)
                radius = 0.1f;

            cameraComp.XJSetRadius(radius);
        }
    }
}
