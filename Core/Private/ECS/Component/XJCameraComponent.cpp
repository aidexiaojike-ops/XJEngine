#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"


namespace XJ
{
 
    const glm::mat4& XJCameraComponent::XJGetProjectionMatrix()
    {
        float aspect = mAspectRatio;
        if (aspect <= 0.0f) aspect = 1.0f;  // 避免无效值
        projMat = glm::perspective(glm::radians(mFov), mAspectRatio, mNear, mFar);
        projMat[1][1] *= -1;  // 启用 Y
        return projMat;
    }

    const glm::mat4& XJCameraComponent::XJGetViewMatrix()
    {
       if (mMode == CameraMode::Orbit)
            return UpdateOrbitView();
        else
            return UpdateFreeView();
    }

    void XJCameraComponent::XJSetViewMatrix(const glm::mat4& view)
    {
        //TODO: 这里可以添加一些额外的逻辑，例如更新摄像机位置、旋转等
        viewMat = view;
    }
    // 轨道模式：围绕 mTarget 旋转，摄像机位置由 TransformComponent 的 rotation 和 mRadius 计算
    const glm::mat4& XJCameraComponent::UpdateOrbitView()
    {
        XJEntity *kOwner = XJGetOwner();
        // if(XJEntity::HasComponent<XJTransformComponent>())
        if(kOwner && kOwner->HasComponent<XJTransformComponent>())
        {
            auto& kTransformComp = kOwner->GetComponent<XJTransformComponent>();
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
     // 自由模式：直接使用 TransformComponent 的位置和旋转构造视图矩阵
    const glm::mat4& XJCameraComponent::UpdateFreeView()
    {
        XJEntity *kOwner = XJGetOwner();
        if(kOwner && kOwner->HasComponent<XJTransformComponent>())
        {
            auto& kTransformComp = kOwner->GetComponent<XJTransformComponent>();
            float kYaw   = glm::radians(kTransformComp.rotation.x);
            float kPitch = glm::radians(-kTransformComp.rotation.y);

            // 计算前向向量（摄像机朝向）
            glm::vec3 kDirection;//计算摄像机方向
            kDirection.x = cos(kYaw) * cos(kPitch);
            kDirection.y = sin(kPitch);
            kDirection.z = sin(kYaw) * cos(kPitch);
            kDirection = glm::normalize(kDirection);

            // 计算右向量和上向量，保持摄像机直立
            glm::vec3 kRight = glm::normalize(glm::cross(kDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 kUp     = glm::cross(kRight, kDirection);

            
            viewMat = glm::lookAt(kTransformComp.position, kTransformComp.position + kDirection, kUp);
        }
        return viewMat;
    }
}