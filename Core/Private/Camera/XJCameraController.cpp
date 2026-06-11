#include "Camera/XJCameraController.h"

#include "ECS/XJEntity.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"

#include <cmath>

namespace XJ
{
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

            float yaw = glm::radians(transformComp.rotation.x);
            float pitch = glm::radians(-transformComp.rotation.y);

            glm::vec3 direction;
            direction.x = std::cos(yaw) * std::cos(pitch);
            direction.y = std::sin(pitch);
            direction.z = std::sin(yaw) * std::cos(pitch);
            direction = glm::normalize(direction);

            transformComp.position += direction * yOffset * mCameraMoveSpeed * 10.0f;
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

            yaw += mouseDelta.x * mMouseSensitivity;
            pitch += mouseDelta.y * mMouseSensitivity;
            pitch = glm::clamp(pitch, -89.0f, 89.0f);

            transformComp.rotation.x = yaw;
            transformComp.rotation.y = pitch;
            transformComp.UpdateModelMatrix();
        }

        if (rightDown)
        {
            float yaw = glm::radians(transformComp.rotation.x);
            float pitch = glm::radians(transformComp.rotation.y);

            glm::vec3 forward;
            forward.x = std::cos(yaw) * std::cos(pitch);
            forward.y = std::sin(pitch);
            forward.z = std::sin(yaw) * std::cos(pitch);
            forward = glm::normalize(forward);

            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 up = glm::cross(right, forward);

            glm::vec3 moveVector = (right * -mouseDelta.x + up * mouseDelta.y) * mCameraMoveSpeed;
            transformComp.position += moveVector;
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
            pitch = glm::clamp(pitch, -89.0f, 89.0f);

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
