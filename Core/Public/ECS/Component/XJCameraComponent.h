#ifndef XJ_CAMERA_COMPONENT_H
#define XJ_CAMERA_COMPONENT_H

#include "ECS/XJComponent.h"
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <algorithm>

namespace XJ
{
    // 摄像机模式
    enum class CameraMode
    {
        Orbit,  // 轨道模式：围绕目标点旋转，始终看向目标
        Free    // 自由模式：位置/朝向独立控制，无固定目标
    };

    class XJCameraComponent : public XJComponent
    {
        public:
            const glm::mat4& XJGetProjectionMatrix();//获取投影矩阵
            const glm::mat4& XJGetViewMatrix();//获取视图矩阵

            bool XJGetEnabled() const { return mEnabled; }
            void XJSetEnabled(bool enabled) { mEnabled = enabled; }

            void XJSetProjectionMatrix(const glm::mat4& proj) { projMat = proj; }//设置投影矩阵
            void XJSetViewMatrix(const glm::mat4& view);//设置视图矩阵

            //通用设置
            float XJGetFov() const { return mFov; }//获取视野
            float XJGetAspectRatio() const { return mAspectRatio; }//获取宽高比
            float XJGetNear() const { return mNear; }//获取近裁剪面
            float XJGetFar() const { return mFar; }//获取远裁剪面

            void XJSetFov(float fov) { mFov = glm::clamp(fov, 1.0f, 179.0f); }
            void XJSetAspectRatio(float aspectRatio) { mAspectRatio = aspectRatio > 0.0f ? aspectRatio : 1.0f; }
            void XJSetNear(float nearVal) { mNear = std::max(nearVal, 0.001f); }
            void XJSetFar(float farVal) { mFar = std::max(farVal, mNear + 0.001f); }

            // 轨道模式专用接口
            glm::vec3 XJGetPosition() const;//读取 Transform 世界位置
            float XJGetRadius() const { return mRadius; }//获取半径
            const glm::vec3& XJGetTarget() const { return mTarget; }///   轨道模式专用数据
   
            void XJSetPosition(const glm::vec3& position);//通过 Transform 写入世界位置
            void XJSetRadius(float radius) { mRadius = std::max(radius, 0.1f); }//设置半径
            void XJSetTarget(const glm::vec3& target) { mTarget = target; }///   轨道模式专用数据

            //切换摄像机
            void XJSetCameraMode(CameraMode mode) { mMode = mode; }
            CameraMode XJGetCameraMode() const { return mMode; }

        private:
             // 投影参数
            float mFov{65.f};//视野
            float mAspectRatio{1.0f};//宽高比
            float mNear{0.1f};//近裁剪面
            float mFar{100.0f};//远裁剪面
            bool mEnabled{true};
            glm::vec3 mTarget{0.0f, 0.0f, 0.0f};//摄像机目标点 // 轨道模式专用数据

             // 轨道模式专用数据
            float mRadius{6.f};//半径
            // // 视图/投影矩阵缓存
            glm::mat4 projMat{1.0f};//投影矩阵
            glm::mat4 viewMat{1.0f};//视图矩阵

            // 当前模式
            //CameraMode mMode{CameraMode::Orbit};  // 默认轨道模式
            CameraMode mMode{CameraMode::Free};  // 默认轨道模式

    };
}


#endif

