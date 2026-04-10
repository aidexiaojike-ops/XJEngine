#ifndef XJ_CAMERA_COMPONENT_H
#define XJ_CAMERA_COMPONENT_H

#include "ECS/XJComponent.h"
#include "Edit/Mathinclude.h"

namespace XJ
{
    class XJCameraComponent : public XJComponent
    {
        public:
          
    
            const glm::mat4& XJGetProjectionMatrix();//获取投影矩阵
            const glm::mat4& XJGetViewMatrix();//获取视图矩阵

            float XJGetFov() const { return mFov; }//获取视野
            float XJGetAspectRatio() const { return mAspectRatio; }//获取宽高比
            float XJGetNear() const { return mNear; }//获取近裁剪面
            float XJGetFar() const { return mFar; }//获取远裁剪面

            const glm::vec3& XJGetPosition() const { return mPosition; }//获取摄像机位置
            float XJGetRadius() const { return mRadius; }//获取半径




            void XJSetProjectionMatrix(const glm::mat4& proj) { projMat = proj; }//设置投影矩阵
            void XJSetViewMatrix(const glm::mat4& view);//设置视图矩阵
   

            void XJSetFov(float fov) { this->mFov = fov; }
            void XJSetAspectRatio(float aspectRatio) { this->mAspectRatio = aspectRatio; }
            void XJSetNear(float nearVal) { this->mNear = nearVal; }
            void XJSetFar(float farVal) { this->mFar = farVal; }

            
            void XJSetPosition(const glm::vec3& position) {  this->mPosition = position; }//设置摄像机位置
            void XJSetRadius(float radius) { this->mRadius = radius; }//设置半径

        private:
            float mFov{65.f};//视野
            float mAspectRatio{1.0f};//宽高比
            float mNear{0.1f};//近裁剪面
            float mFar{100.0f};//远裁剪面
            glm::vec3 mTarget{0.0f, 0.0f, 0.0f};//摄像机目标点
            glm::vec3 mPosition{0.0f, 1.0f, 0.0f};//摄像机位置

            float mRadius{6.f};//半径

            glm::mat4 projMat{1.0f};//投影矩阵
            glm::mat4 viewMat{1.0f};//视图矩阵

    };
}


#endif

