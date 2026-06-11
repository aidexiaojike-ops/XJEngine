#ifndef XJ_CAMERA_SYSTEM_H
#define XJ_CAMERA_SYSTEM_H

#include "Camera/XJCameraController.h"
#include "ECS/XJSystem.h"

namespace XJ
{
    class XJEntity;
    class XJGlfwWindow;

    class XJCameraSystem : public XJSystem
    {
        public:
            XJCameraSystem(
                float mouseSensitivity = 0.25f,
                float cameraMoveSpeed = 0.05f,
                float cameraZoomSpeed = 0.3f,
                float cameraRotateSpeed = 0.25f);

            // Runtime/game 入口：具体算法委托给共享 XJCameraController。
            void UpdateCameraControl(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);
            void OnMouseScroll(float yOffset, XJEntity* cameraEntity);

            XJCameraController& GetController() { return mController; }
            const XJCameraController& GetController() const { return mController; }

        private:
            XJCameraController mController;
    };
}

#endif
