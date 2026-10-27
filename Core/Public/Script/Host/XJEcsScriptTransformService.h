#ifndef XJ_ECS_SCRIPT_TRANSFORM_SERVICE_H
#define XJ_ECS_SCRIPT_TRANSFORM_SERVICE_H

#include "Script/Host/XJEcsScriptEntityService.h"
#include "ECS/Component/XJTransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>
//ECS Transform 后端
namespace XJ
{
    class XJEcsScriptTransformService final : public IXJScriptTransformService
    {
        public:
            explicit XJEcsScriptTransformService(
                std::shared_ptr<XJEcsScriptEntityService> entities)
                : mEntities(std::move(entities))
            {
                if (!mEntities)
                {
                    spdlog::error("Script Transform service requires an entity service.");
                    throw std::invalid_argument("Missing script entity service");
                }
            }
            XJScriptHostResult<XJScriptVec3> Read(//读取transform
                XJScriptEntityRef reference,
                XJScriptSpace space,
                XJScriptTransformProperty property) override
            {
                if (space != XJScriptSpace::Local && space != XJScriptSpace::World)
                    return {{}, {XJScriptHostError::InvalidArgument, "Unknown Transform space"}};

                const auto entity = mEntities->Resolve(reference);

                if (!entity)
                    return Failure("Entity reference is invalid");

                if (!entity->HasComponent<XJTransformComponent>())
                {
                    return {
                        {},
                        {
                            XJScriptHostError::MissingTransform,
                            "Entity has no Transform"
                        }
                    };
                }

                const auto& transform = entity->GetComponent<XJTransformComponent>();

                if (space == XJScriptSpace::Local)
                {
                    switch (property)
                    {
                        case XJScriptTransformProperty::Position:
                            return Checked(ToScript(transform.position));

                        case XJScriptTransformProperty::EulerAngles:
                            return Checked(ToScript(transform.rotation));

                        case XJScriptTransformProperty::Scale:
                            return Checked(ToScript(transform.scale));
                    }

                    return InvalidProperty();
                }

                WorldState state;
                const auto status = Evaluate(entity.get(), state);

                if (!status.IsValid())
                    return {{}, status};

                switch (property)
                {
                    case XJScriptTransformProperty::Position:
                        return Checked(ToScript(
                            glm::dvec3(state.Matrix[3])));

                    case XJScriptTransformProperty::EulerAngles:
                        return Checked(ToEuler(state.Rotation));

                    case XJScriptTransformProperty::Scale:
                        return Checked({
                            AxisLength(glm::dvec3(state.Matrix[0])),
                            AxisLength(glm::dvec3(state.Matrix[1])),
                            AxisLength(glm::dvec3(state.Matrix[2]))
                        });
                }

                return InvalidProperty();
            }

            XJScriptHostStatus Write(//写入transform
                XJScriptEntityRef reference,//获取uuid 是否是对象
                XJScriptSpace space,//是否是世界空间
                XJScriptTransformProperty property,//修改的transform
                XJScriptVec3 value) override//传入的向量
            {
                if (space != XJScriptSpace::Local && space != XJScriptSpace::World)
                    return {XJScriptHostError::InvalidArgument, "Unknown Transform space"};

                if (!value.IsFinite())
                {
                    return {
                        XJScriptHostError::InvalidArgument,
                        "Transform value must be finite"
                    };
                }

                const auto entity = mEntities->Resolve(reference);

                if (!entity)
                {
                    return {
                        XJScriptHostError::MissingEntity,
                        "Entity reference is invalid"
                    };
                }

                if (!entity->HasComponent<XJTransformComponent>())
                {
                    return {
                        XJScriptHostError::MissingTransform,
                        "Entity has no Transform"
                    };
                }

                auto& transform = entity->GetComponent<XJTransformComponent>();
                //
                glm::dvec3 position(transform.position);
                glm::dvec3 rotation(transform.rotation);
                glm::dvec3 scale(transform.scale);

                glm::dvec3 requested(value.X, value.Y, value.Z);

                WorldState parent;
                if (space == XJScriptSpace::Local)
                {
                    switch (property)
                    {
                        case XJScriptTransformProperty::Position:
                            position = requested;
                            break;

                        case XJScriptTransformProperty::EulerAngles:
                            rotation = requested;
                            break;

                        case XJScriptTransformProperty::Scale:
                            scale = requested;
                            break;

                        default:
                            return {
                                XJScriptHostError::InvalidArgument,
                                "Unknown Transform property"
                            };
                    }
                }
                else
                {
                    auto status = Evaluate(entity->XJGetParent(), parent);

                    if (!status.IsValid())
                        return status;

                    switch (property)
                    {
                        case XJScriptTransformProperty::Position:
                        {
                            const glm::dmat3 basis(parent.Matrix);

                            const double volume =
                                AxisLength(basis[0]) *
                                AxisLength(basis[1]) *
                                AxisLength(basis[2]);

                            const double determinant =
                                glm::determinant(basis);

                            if (!std::isfinite(volume) ||
                                !std::isfinite(determinant) ||
                                volume == 0.0 ||
                                std::abs(determinant) <=
                                    volume * 1.0e-12)
                            {
                                return {
                                    XJScriptHostError::NonInvertibleParent,
                                    "Parent world transform is singular or ill-conditioned"
                                };
                            }

                            const glm::dvec4 local =
                                glm::inverse(parent.Matrix) *
                                glm::dvec4(requested, 1.0);

                            position = glm::dvec3(local);
                            break;
                        }

                        case XJScriptTransformProperty::EulerAngles:
                        {
                            const glm::dmat4 desired =
                                EulerMatrix(requested);

                            // 纯旋转矩阵的逆等于转置。
                            const glm::dmat4 local = glm::transpose(parent.Rotation) * desired;

                            const auto angles = ToEuler(local);

                            rotation = {
                                angles.X,
                                angles.Y,
                                angles.Z
                            };
                            break;
                        }

                        case XJScriptTransformProperty::Scale:
                        {
                            if (requested.x < 0.0 ||
                                requested.y < 0.0 ||
                                requested.z < 0.0)
                            {
                                return {
                                    XJScriptHostError::InvalidArgument,
                                    "World scale must be non-negative; use local scale for reflections"
                                };
                            }

                            const glm::dmat4 localRotation =
                                EulerMatrix(rotation);

                            const glm::dmat3 inherited =
                                glm::dmat3(parent.Matrix) *
                                glm::dmat3(localRotation);

                            for (int axis = 0; axis < 3; ++axis)
                            {
                                const double length =
                                    AxisLength(inherited[axis]);

                                if (!std::isfinite(length) ||
                                    length <= 0.0)
                                {
                                    return {
                                        XJScriptHostError::NonInvertibleParent,
                                        "Parent transform collapses a scale axis"
                                    };
                                }

                                const double sign =
                                    std::signbit(scale[axis])
                                        ? -1.0
                                        : 1.0;

                                scale[axis] =
                                    sign * requested[axis] / length;
                            }
                            break;
                        }

                        default:
                            return {
                                XJScriptHostError::InvalidArgument,
                                "Unknown Transform property"
                            };
                    }
                }

                if (!FitsFloat(position) ||
                    !FitsFloat(rotation) ||
                    !FitsFloat(scale))
                {
                    return {
                        XJScriptHostError::NonFiniteResult,
                        "Transform result exceeds float storage range"
                    };
                }

                // 用实际 float 存储精度验证矩阵，避免先修改再发现溢出。
                const glm::vec3 newPosition(position);
                const glm::vec3 newRotation(rotation);
                const glm::vec3 newScale(scale);

                glm::mat4 candidate(1.0f);

                candidate = glm::translate(
                    candidate, newPosition);

                candidate *= glm::eulerAngleXYZ(
                    glm::radians(newRotation.x),
                    glm::radians(newRotation.y),
                    glm::radians(newRotation.z));

                candidate = glm::scale(candidate, newScale);

                if (!IsFinite(candidate))
                {
                    return {
                        XJScriptHostError::NonFiniteResult,
                        "Transform matrix contains a non-finite result"
                    };
                }

                // World 写入还必须保证提交后的完整世界矩阵可读取。
                if (space == XJScriptSpace::World &&
                    !IsFinite(parent.Matrix * glm::dmat4(candidate)))
                {
                    return {
                        XJScriptHostError::NonFiniteResult,
                        "World Transform calculation overflowed"
                    };
                }

                transform.position = newPosition;
                transform.rotation = newRotation;
                transform.scale = newScale;
                transform.modelMatrix = candidate;

                return {};
            }



        private:
            static double AxisLength(const glm::dvec3& axis)
            {
                // 避免 length() 的平方中间值在结果仍可表示时溢出/下溢。
                return std::hypot(axis.x, axis.y, axis.z);
            }

            struct WorldState//世界状态
            {
                glm::dmat4 Matrix{1.0};
                glm::dmat4 Rotation{1.0};
            };

            static glm::dmat4 EulerMatrix(const glm::dvec3& degrees)//欧拉矩阵
            {
                // 与当前 UpdateModelMatrix 的 Rx * Ry * Rz 一致。
                return glm::eulerAngleXYZ(
                    glm::radians(degrees.x),
                    glm::radians(degrees.y),
                    glm::radians(degrees.z));
            }

            static XJScriptVec3 ToEuler(const glm::dmat4& matrix)//转换欧拉角
            {
                double x = 0.0;
                double y = 0.0;
                double z = 0.0;

                glm::extractEulerAngleXYZ(matrix, x, y, z);

                return {
                    glm::degrees(x),
                    glm::degrees(y),
                    glm::degrees(z)
                };
            }

            template<typename T>static XJScriptVec3 ToScript(const T& vector)//向量转换？
            {
                return {
                    static_cast<double>(vector.x),
                    static_cast<double>(vector.y),
                    static_cast<double>(vector.z)
                };
            }

            template<typename Matrix>static bool IsFinite(const Matrix& matrix)//是否有限值
            {
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        if (!std::isfinite(matrix[column][row]))
                            return false;
                    }
                }

                return true;
            }

            static bool FitsFloat(const glm::dvec3& vector)//适合浮点数
            {
                const double maximum =
                    std::numeric_limits<float>::max();

                for (int axis = 0; axis < 3; ++axis)
                {
                    if (!std::isfinite(vector[axis]) ||
                        std::abs(vector[axis]) > maximum ||
                        (vector[axis] != 0.0 &&
                         std::abs(vector[axis]) < std::numeric_limits<float>::denorm_min()))
                    {
                        return false;
                    }
                }

                return true;
            }

            static XJScriptHostStatus Evaluate(XJNode* node, WorldState& result)//估计？
            {
                std::vector<XJNode*> chain;
                std::unordered_set<XJNode*> visited;

                for(;node; node = node->XJGetParent())
                {
                    if (!visited.insert(node).second ||
                        chain.size() >= 1024)
                    {
                        return {
                            XJScriptHostError::InvalidArgument,
                            "Transform parent hierarchy contains a cycle or is too deep"
                        };
                    }

                    chain.push_back(node);
                }

                result = {};

                for(auto iterator = chain.rbegin(); iterator != chain.rend(); ++iterator)
                {
                    auto* entity = dynamic_cast<XJEntity*>(*iterator);
                    // 非实体根节点、无 Transform 节点视为单位变换。

                    if(!entity)
                        continue;

                    if(!entity->IsValid())
                    {
                        return {
                            XJScriptHostError::MissingEntity,
                            "Transform hierarchy contains an invalid entity"
                        };
                    }

                    if(!entity -> HasComponent<XJTransformComponent>())
                        continue;
                    
                    const auto& transform = entity->GetComponent<XJTransformComponent>();

                    const glm::dvec3 position(transform.position);
                    const glm::dvec3 rotation(transform.rotation);
                    const glm::dvec3 scale(transform.scale);

                    if (!FitsFloat(position) ||
                        !FitsFloat(rotation) ||
                        !FitsFloat(scale))
                    {
                        return {
                            XJScriptHostError::NonFiniteResult,
                            "Transform hierarchy contains invalid values"
                        };
                    }

                    const glm::dmat4 orientation = EulerMatrix(rotation);

                    glm::dmat4 local =
                        glm::translate(glm::dmat4(1.0), position);

                    local *= orientation;
                    local = glm::scale(local, scale);

                    result.Matrix *= local;
                    result.Rotation *= orientation;

                    if (!IsFinite(result.Matrix) ||
                        !IsFinite(result.Rotation))
                    {
                        return {
                            XJScriptHostError::NonFiniteResult,
                            "World Transform calculation overflowed"
                        };
                    }

                }
                return {};
            }

            static XJScriptHostResult<XJScriptVec3> Checked(XJScriptVec3 value)//一检查
            {
                if (!value.IsFinite())
                {
                    return {
                        {},
                        {
                            XJScriptHostError::NonFiniteResult,
                            "Transform result is not finite"
                        }
                    };
                }

                return {value, {}};
            }

            static XJScriptHostResult<XJScriptVec3> Failure(std::string message)//失败
            {
                return {
                    {},
                    {
                        XJScriptHostError::MissingEntity,
                        std::move(message)
                    }
                };
            }

            static XJScriptHostResult<XJScriptVec3> InvalidProperty()//无效参数
            {
                return {
                    {},
                    {
                        XJScriptHostError::InvalidArgument,
                        "Unknown Transform property"
                    }
                };
            }

            std::shared_ptr<XJEcsScriptEntityService> mEntities;

    };
}


#endif
