#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"


namespace XJ
{
 
    const glm::mat4& XJCameraComponent::XJGetProjectionMatrix()
    {
        projMat = glm::perspective(glm::radians(mFov), mAspectRatio, mNear, mFar);
        projMat[1][1] *= -1;  // 启用 Y
        return projMat;
    }

    const glm::mat4& XJCameraComponent::XJGetViewMatrix()
    {
        XJEntity *kOwner = XJGetOwner();
        // if(XJEntity::HasComponent<XJTransformComponent>())
        if(kOwner && kOwner->HasComponent<XJTransformComponent>())
        {
            auto& kTransformComp = kOwner->GetComponent<XJTransformComponent>();
            //TODO  
            float kYaw = kTransformComp.rotation.x;//计算偏航角
            float kPitch = kTransformComp.rotation.y;//计算俯仰
           
            glm::vec3 kDirection;//计算摄像机方向
            kDirection.x = cos(glm::radians(kYaw)) * cos(glm::radians(kPitch));
            kDirection.y = sin(glm::radians(kPitch));
            kDirection.z = sin(glm::radians(kYaw)) * cos(glm::radians(kPitch));
            
            //kTransformComp.position = mTarget - glm::normalize(kDirection) * mRadius;//更新摄像机位置
            kTransformComp.position = mTarget + kDirection * mRadius;//更新摄像机位置

            viewMat = glm::lookAt(kTransformComp.position, mTarget, mPosition);
        }
        return viewMat;
    }

    void XJCameraComponent::XJSetViewMatrix(const glm::mat4& view)
    {
        //TODO: 这里可以添加一些额外的逻辑，例如更新摄像机位置、旋转等
        viewMat = view;
    }
}