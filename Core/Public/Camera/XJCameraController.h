#ifndef XJ_CAMERA_CONTROLLER_H
#define XJ_CAMERA_CONTROLLER_H

#include "Edit/Mathinclude.h"
#include "Edit/XJGlfwWindow.h"
#include "Event/XJMouseEvent.h"

namespace XJ
{
    class XJEntity;

    class XJCameraController
    {
    public:
        XJCameraController(
            float mouseSensitivity = 0.25f,
            float cameraMoveSpeed = 0.05f,
            float cameraZoomSpeed = 0.3f,
            float cameraRotateSpeed = 0.25f);

        // 共享相机控制算法：只修改传入 camera entity 的组件，不关心 editor 或 runtime 场景。
        void UpdateCameraControl(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);
        void OnMouseScroll(float yOffset, XJEntity* cameraEntity);

        void XJSetMouseSensitivity(float mouseSensitivity) { mMouseSensitivity = mouseSensitivity; }
        void XJSetCameraMoveSpeed(float cameraMoveSpeed) { mCameraMoveSpeed = cameraMoveSpeed; }
        void XJSetCameraZoomSpeed(float cameraZoomSpeed) { mCameraZoomSpeed = cameraZoomSpeed; }
        void XJSetCameraRotateSpeed(float cameraRotateSpeed) { mCameraRotateSpeed = cameraRotateSpeed; }

        float XJGetMouseSensitivity() const { return mMouseSensitivity; }
        float XJGetCameraMoveSpeed() const { return mCameraMoveSpeed; }
        float XJGetCameraZoomSpeed() const { return mCameraZoomSpeed; }
        float XJGetCameraRotateSpeed() const { return mCameraRotateSpeed; }

    private:
        void UpdateFreeCamera(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);
        void UpdateOrbitCamera(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);

        float mMouseSensitivity = 0.25f;
        float mCameraMoveSpeed = 0.05f;
        float mCameraZoomSpeed = 0.3f;
        float mCameraRotateSpeed = 0.25f;

        glm::vec2 mLastMousePos{0.0f};
        bool mFirstMouseDrag = true;

        bool mLeftButtonDown = false;
        bool mRightButtonDown = false;
    };
}

#endif
