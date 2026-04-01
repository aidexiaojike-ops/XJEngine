#ifndef XJ_TRANSFORM_COMPONENT_H
#define XJ_TRANSFORM_COMPONENT_H

#include "ECS/XJComponent.h"
#include "Edit/Mathinclude.h"

namespace XJ
{
    class XJTransformComponent : public XJComponent
    {
        public:
            glm::vec3 position{0.0f, 0.0f, 0.0f};//位置
            glm::vec3 rotation{0.0f, 0.0f, 0.0f};//旋转
            glm::vec3 scale{1.0f, 1.0f, 1.0f};//缩放
        
            glm::mat4 modelMatrix{1.0f};//模型矩阵，默认初始化为单位矩阵
        
            void UpdateModelMatrix()
            {
                // 计算模型矩阵
                modelMatrix = glm::mat4(1.0f); // 重置为单位矩阵
                modelMatrix = glm::translate(modelMatrix, position); // 应用平移
                modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // 应用绕X轴旋转
                modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // 应用绕Y轴旋转
                modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // 应用绕Z轴旋转
                modelMatrix = glm::scale(modelMatrix, scale); // 应用缩放
            }
        
            glm::mat4 GetModelMatrix() const
            {
                return modelMatrix;
            }
    };
} // namespace XJ


#endif // XJ_TRANSFORM_COMPONENT_H