#ifndef XJ_EDITOR_CAMERA_CONTROLLER_H
#define XJ_EDITOR_CAMERA_CONTROLLER_H

#include "Camera/XJCameraController.h"

namespace XJ
{
    class XJEntity;
    class XJGlfwWindow;

    class XJEditorCameraController
    {
        public:
            XJEditorCameraController(
                float mouseSensitivity = 0.25f,
                float cameraMoveSpeed = 0.05f,
                float cameraZoomSpeed = 0.3f,
                float cameraRotateSpeed = 0.25f);

            // Editor Scene Preview 的调用入口，后续 editor-only 规则可集中加在这里。
            void UpdateCameraControl(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);
            void OnMouseScroll(float yOffset, XJEntity* cameraEntity);

            XJCameraController& GetController() { return mController; }
            const XJCameraController& GetController() const { return mController; }

            

        private:
            XJCameraController mController;
    };
}

#endif
