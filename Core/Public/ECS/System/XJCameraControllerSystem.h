#ifndef XJ_CAMERA_CONTROLLER_SYSTEM
#define XJ_CAMERA_CONTROLLER_SYSTEM

#include "ECS/XJSystem.h"
#include "Edit/XJGlfwWindow.h"
#include "ECS/Component/XJCameraComponent.h"
#include "Edit/Mathinclude.h"
#include "ECS/XJEntity.h"

namespace XJ
{
    class XJCameraControllerSystem : public XJCameraSystem
    {
        public:
            XJCameraControllerSystem( 
            float mouseSensitivity = 0.25f,
            float cameraMoveSpeed = 0.05f,
            float cameraZoomSpeed = 0.3f,
            float cameraRotateSpeed = 0.25f);

            // 主更新函数 - 由应用程序在 OnUpdate 中调用
            void UpdateCameraControl(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);

            // 滚轮事件处理 - 由应用程序在事件回调中调用
            void OnMouseScroll(float yOffset, XJEntity* cameraEntity);

            
            void XJSetMouseSensitivity(float mouseSensitivity){ mMouseSensitivity = mouseSensitivity;};
            void XJSetCameraMoveSpeed(float cameraMoveSpeed){ mCameraMoveSpeed = cameraMoveSpeed;};
            void XJSetCameraZoomSpeed(float cameraZoomSpeed){ mCameraZoomSpeed = cameraZoomSpeed;};
            void XJSetCameraRotateSpeed(float cameraRotateSpeed){ mCameraRotateSpeed = cameraRotateSpeed;};


            float XJGetMouseSensitivity() const { return mMouseSensitivity;};
            float XJGetCameraMoveSpeed() const { return mCameraMoveSpeed;};
            float XJGetCameraZoomSpeed() const { return mCameraZoomSpeed;};
            float XJGetCameraRotateSpeed() const { return mCameraRotateSpeed;};
        private:

            // 模式特定的更新函数
            void UpdateFreeCamera(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);
            void UpdateOrbitCamera(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity);

            // 辅助函数
            bool HandleMouseButtonState(XJGlfwWindow* window, bool& buttonState, MouseButton button);

        private:
            // 控制参数（暂时放在头文件，以后添加UI再改）
            float mMouseSensitivity;      // 鼠标灵敏度
            float mCameraMoveSpeed;       // 平移速度（自由模式）
            float mCameraZoomSpeed;       // 缩放速度（轨道模式）
            float mCameraRotateSpeed;     // 旋转速度
                
            // 状态跟踪
            glm::vec2 mLastMousePos;      // 上一帧鼠标位置
            bool mFirstMouseDrag;         // 是否第一次鼠标拖动
                
            // 按键状态跟踪
            bool mLeftButtonDown;
            bool mRightButtonDown;


    };
}


#endif