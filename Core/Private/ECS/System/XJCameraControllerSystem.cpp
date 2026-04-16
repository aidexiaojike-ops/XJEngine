//摄像机控制器
#include "ECS/System/XJCameraControllerSystem.h"//获取基础材质系统信息
#include "ECS/Component/XJTransformComponent.h"
#include "Event/XJMouseEvent.h"


namespace XJ
{
    XJCameraControllerSystem::XJCameraControllerSystem(float mouseSensitivity, float cameraMoveSpeed, float cameraZoomSpeed, float cameraRotateSpeed)
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
    //判断是否运行
    void XJCameraControllerSystem::UpdateCameraControl(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity)
    {
        if(!window|| !cameraEntity|| !XJEntity::HasComponent<XJCameraComponent>(cameraEntity))
            return;

        auto& kCameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        CameraMode kMode = kCameraComp.XJGetCameraMode();

        if(kMode == CameraMode::Free)
        {
            UpdateFreeCamera(deltaTime, window, cameraEntity);
        }
        else if(kMode == CameraMode::Orbit)
        {
            UpdateOrbitCamera(deltaTime, window, cameraEntity);
        }
    }

    void XJCameraControllerSystem::OnMouseScroll(float yOffset, XJEntity* cameraEntity)
    {
        if(!cameraEntity || !XJEntity::HasComponent<XJCameraComponent>(cameraEntity))
            return;
        
        
        auto& kCameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        CameraMode kMode = kCameraComp.XJGetCameraMode();

        if(kMode == CameraMode::Free)
        {
            // 自由模式：前后移动
            if(XJEntity::HasComponent<XJTransformComponent>(cameraEntity))
            {
                auto& kTransformComp = cameraEntity->GetComponent<XJTransformComponent>();
                //计算前向向量
                float kYaw = glm::radians(kTransformComp.rotation.x);
                float kPitch = glm::radians(-kTransformComp.rotation.y);

                glm::vec3 kDirection;
                kDirection.x = cos(kYaw) *cos(kPitch);
                kDirection.y = sin(kPitch);
                kDirection.z = sin(kYaw) * cos(kPitch);
                kDirection = glm::normalize(kDirection);

                kTransformComp.position += kDirection* yOffset * mCameraMoveSpeed *10.0f;
                kTransformComp.UpdateModelMatrix();
            }
        }
        else if ( kMode == CameraMode::Orbit)
        {
            // 轨道模式：缩放
            float kRadius = kCameraComp.XJGetRadius() + yOffset * - mCameraZoomSpeed; 
            if( kRadius < 0.1f){ kRadius = 0.1f;}
            kCameraComp.XJSetRadius(kRadius);
        }
        
    }

    void XJCameraControllerSystem::UpdateFreeCamera(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity)
    {
        auto& kCameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        auto& kTransformComp = cameraEntity->GetComponent<XJTransformComponent>();

        //获取鼠标状态
        bool kLeftDown = window->IsMouseDown(MOUSE_BUTTON_LEFT);
        bool kRightDown = window->IsMouseDown(MOUSE_BUTTON_RIGHT);

        // 如果没有按键按下，重置状态
        if(!kLeftDown && !kRightDown)
        {
            mFirstMouseDrag = true;
            mLeftButtonDown = false;
            mRightButtonDown = false;

            // 更新鼠标位置但不处理移动
            glm::vec2 kMousePos;
            window->XJGetMousePos(kMousePos);
            mLastMousePos = kMousePos;
            return;
        }
        // 获取当前鼠标位置
        glm::vec2 kMousePos;
        window->XJGetMousePos(kMousePos);
        glm::vec2 kMouseDelta = kMousePos - mLastMousePos;
        mLastMousePos = kMousePos;
        // 防抖动
        if (glm::length(kMouseDelta) < 0.1f) {return;}

        // 处理第一次拖动
        if (mFirstMouseDrag)
        {
            mFirstMouseDrag = false;
            return;
        }

        // 左键：旋转
        if(kLeftDown)
        {
            float kYaw = kTransformComp.rotation.x;
            float kPitch = kTransformComp.rotation.y;

            kYaw += kMouseDelta.x * mMouseSensitivity;
            kPitch += kMouseDelta.y * mMouseSensitivity;
            kPitch = glm::clamp(kPitch, -89.0f, 89.0f);

            kTransformComp.rotation.x = kYaw;
            kTransformComp.rotation.y = kPitch;
            kTransformComp.UpdateModelMatrix();
        }

        // 右键：视图平面平移
        if(kRightDown)
        {
            // 计算摄像机方向向量
            float kYaw = glm::radians(kTransformComp.rotation.x);
            float kPitch = glm::radians(kTransformComp.rotation.y);

            glm::vec3 kForward;
            kForward.x = cos(kYaw) * cos(kPitch);
            kForward.y = sin(kPitch);
            kForward.z = sin(kYaw) * cos(kPitch);
            kForward = glm::normalize(kForward);

            glm::vec3 kRight = glm::normalize(glm::cross(kForward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 kUp = glm::cross(kRight, kForward);

            // 在视图平面内平移
            glm::vec3 kMoveVec = (kRight* - kMouseDelta.x + kUp *kMouseDelta.y) * mCameraMoveSpeed;
            kTransformComp.position += kMoveVec;
            kTransformComp.UpdateModelMatrix();
        }
    }

    void XJCameraControllerSystem::UpdateOrbitCamera(float deltaTime, XJGlfwWindow* window, XJEntity* cameraEntity)
    {
        auto& kCameraComp = cameraEntity->GetComponent<XJCameraComponent>();
        auto& kTransformComp = cameraEntity->GetComponent<XJTransformComponent>();

         //获取鼠标状态
        bool kLeftDown = window->IsMouseDown(MOUSE_BUTTON_LEFT);
        bool kRightDown = window->IsMouseDown(MOUSE_BUTTON_RIGHT);
        // 如果没有按键按下，重置状态
        if(!kLeftDown && !kRightDown)
        {
            mFirstMouseDrag = true;
            mLeftButtonDown = false;
            mRightButtonDown = false;

            // 更新鼠标位置但不处理移动
            glm::vec2 kMousePos;
            window->XJGetMousePos(kMousePos);
            mLastMousePos = kMousePos;
            return;
        }

        // 获取当前鼠标位置
        glm::vec2 kMousePos;
        window->XJGetMousePos(kMousePos);
        glm::vec2 kMouseDelta = kMousePos - mLastMousePos;
        mLastMousePos = kMousePos;
        // 防抖动
        if (glm::length(kMouseDelta) < 0.1f) {return;}

        // 处理第一次拖动
        if (mFirstMouseDrag)
        {
            mFirstMouseDrag = false;
            return;
        }
        
        // 左键：围绕目标点旋转（与自由模式相同的旋转逻辑）
        if(kLeftDown)
        {
            float kYaw = kTransformComp.rotation.x;
            float kPitch = kTransformComp.rotation.y;

            kYaw += kMouseDelta.x * mMouseSensitivity;
            kPitch += kMouseDelta.y * mMouseSensitivity;
            kPitch = glm::clamp(kPitch, -89.0f, 89.0f);

            kTransformComp.rotation.x = kYaw;
            kTransformComp.rotation.y = kPitch;
            kTransformComp.UpdateModelMatrix();
        }


         // 右键：垂直拖动缩放
        if(kRightDown)
        {
            float kRadius = kCameraComp.XJGetRadius() + kMouseDelta.y * - mCameraZoomSpeed; 
            if( kRadius < 0.1f){ kRadius = 0.1f;}
            kCameraComp.XJSetRadius(kRadius);
        }
    }
    
}