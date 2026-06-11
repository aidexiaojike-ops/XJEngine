#include "ECS/System/XJCameraSystem.h"

namespace XJ
{
    XJCameraSystem::XJCameraSystem(
        float mouseSensitivity,
        float cameraMoveSpeed,
        float cameraZoomSpeed,
        float cameraRotateSpeed)
        : mController(mouseSensitivity, cameraMoveSpeed, cameraZoomSpeed, cameraRotateSpeed)
    {
    }

    void XJCameraSystem::UpdateCameraControl(
        float deltaTime,
        XJGlfwWindow* window,
        XJEntity* cameraEntity)
    {
        mController.UpdateCameraControl(deltaTime, window, cameraEntity);
    }

    void XJCameraSystem::OnMouseScroll(float yOffset, XJEntity* cameraEntity)
    {
        mController.OnMouseScroll(yOffset, cameraEntity);
    }
}
