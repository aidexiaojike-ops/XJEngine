#include "Controllers/XJEditorCameraController.h"

namespace XJ
{
    XJEditorCameraController::XJEditorCameraController(
        float mouseSensitivity,
        float cameraMoveSpeed,
        float cameraZoomSpeed,
        float cameraRotateSpeed)
        : mController(mouseSensitivity, cameraMoveSpeed, cameraZoomSpeed, cameraRotateSpeed)
    {
    }

    void XJEditorCameraController::UpdateCameraControl(
        float deltaTime,
        XJGlfwWindow* window,
        XJEntity* cameraEntity)
    {
        mController.UpdateCameraControl(deltaTime, window, cameraEntity);
    }

    void XJEditorCameraController::OnMouseScroll(float yOffset, XJEntity* cameraEntity)
    {
        mController.OnMouseScroll(yOffset, cameraEntity);
    }
}
