#include "ECS/Component/XJTransformComponent.h"

#include "ECS/XJEntity.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>


namespace XJ
{
    namespace
    {
        bool IsFinite(const glm::dvec3& value)//是否是有限
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool IsFinite(const glm::dquat& value)
        {
            return std::isfinite(value.w) &&
                   std::isfinite(value.x) &&
                   std::isfinite(value.y) &&
                   std::isfinite(value.z);
        }

        template<typename Matrix> bool IsFiniteMatrix(const Matrix& matrix)//是否为有限矩阵
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

        bool FitsFloat(double value)
        {
            if(!std::isfinite(value))
                return false;

            const double magnitude =  std::abs(value);

            if (magnitude > static_cast<double>(std::numeric_limits<float>::max()))
            {
                return false;
            }

            // 不允许非零 double 静默变成 float 零。
            if (value != 0.0 &&
                magnitude <
                    static_cast<double>(
                        std::numeric_limits<float>::denorm_min()))
            {
                return false;
            }

            return true;
        }


        bool FitsFloat(const glm::dvec3& value)//合适的浮点数？
        {
            return FitsFloat(value.x) &&
                   FitsFloat(value.y) &&
                   FitsFloat(value.z);
        }

        double AxisLength(const glm::dvec3& value)//轴长
        {
            return std::hypot(value.x, value.y, value.z);
        }

        bool NormalizeRotation(const glm::dquat& source, glm::dquat& outRotation)
        {
            if(!IsFinite(source))
                return false;

             // 先缩放，避免直接 length(source) 溢出或下溢。
            const double largest = std::max(std::max(std::abs(source.w),std::abs(source.x)),
                std::max(std::abs(source.y), std::abs(source.z)));
            
            if(largest == 0.0)
                return false;

            const glm::dquat scaled{//归一化？
                source.w / largest,
                source.x / largest,
                source.y / largest,
                source.z / largest
            };

            const double length = std::sqrt(glm::dot(scaled, scaled));//点乘的并平方根 求出长度

            if (!std::isfinite(length) || length == 0.0)
                return false;

            outRotation = scaled / length;
            return IsFinite(outRotation);
        }//

        glm::dmat4 BuildMatrix(const glm::dvec3& position, const glm::dquat& rotation, const glm::dvec3& scale)//构建矩阵
        {
            return glm::translate(glm::dmat4{1.0}, position) *
                glm::mat4_cast(rotation) *
                glm::scale(glm::dmat4{1.0}, scale);
        }

        XJTransformStatus PrepareLocal(const glm::dvec3& position, const glm::dquat& rotation, const glm::dvec3& scale,
            glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale, glm::mat4& outMatrix)//准备本transorm？
        {
            if (!IsFinite(position) || !IsFinite(scale))
                return XJTransformStatus::InvalidArgument;

            glm::dquat normalized;

            if(!NormalizeRotation(rotation, normalized))
                return XJTransformStatus::InvalidArgument;

            if (!FitsFloat(position) || !FitsFloat(scale))
                return XJTransformStatus::NonFiniteResult;

            outPosition = glm::vec3{position};
            outScale = glm::vec3{scale};

            // normalized 的分量绝对值不超过 1。
            outRotation = glm::normalize(glm::quat{normalized});

            const glm::dmat4 matrix = BuildMatrix(glm::dvec3{outPosition},
                glm::dquat{outRotation},
                glm::dvec3{outScale});

            if(!IsFiniteMatrix(matrix))
                return XJTransformStatus::NonFiniteResult;
                
            // Local Matrix 使用 float 存储，先检查再转换。
            const double maximum = std::numeric_limits<float>::max();

            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (std::abs(matrix[column][row]) > maximum)
                        return XJTransformStatus::NonFiniteResult;
                }
            }

            outMatrix = glm::mat4{matrix};

            if (!IsFiniteMatrix(outMatrix))
                return XJTransformStatus::NonFiniteResult;

            return XJTransformStatus::Success;
        }

        bool InvertParentBasis(const glm::dmat3& basis, glm::dmat3& outInverse)//反正父级基座
        {
            // 按列归一化后再求逆，避免直接 determinant
            // 在父级整体缩放很大或很小时发生中间溢出。
            glm::dvec3 lengths;

            glm::dmat3 normalized{1.0};

            for (int axis = 0; axis < 3; ++axis)
            {
                lengths[axis] = AxisLength(basis[axis]);

                if (!std::isfinite(lengths[axis]) || lengths[axis] == 0.0)
                {
                    return false;
                }

                normalized[axis] = basis[axis] / lengths[axis];
            }

            const double determinant =  glm::determinant(normalized);

            // 拒绝奇异或数值上严重病态的父变换。
            if (!std::isfinite(determinant) || std::abs(determinant) <= 1.0e-12)
            {
                return false;
            }

            const glm::dmat3 inverseNormalized = glm::inverse(normalized);
            // basis = normalized * diag(lengths)
            // inverse = diag(1 / lengths) * inverse(normalized)
            for (int column = 0; column < 3; ++column)
            {
                for (int row = 0; row < 3; ++row)
                {
                    outInverse[column][row] =
                        inverseNormalized[column][row] /
                        lengths[row];

                    if (!std::isfinite(outInverse[column][row]))
                        return false;
                }
            }

            return true;
        }

    }

        glm::dquat XJTransformComponent::RotationFromEulerXYZ(const glm::dvec3& degrees)//从欧拉角转XYZ
        {
           // 缩减周期，避免极大角度传入三角函数。
            const glm::dvec3  wrapped{std::remainder(degrees.x, 360.0),
                std::remainder(degrees.y, 360.0),
                std::remainder(degrees.z, 360.0)};
            
            const glm::dvec3 radians = glm::radians(wrapped);

            const glm::dquat x = glm::angleAxis(
                radians.x, glm::dvec3{1.0, 0.0, 0.0});

            const glm::dquat y = glm::angleAxis(
                radians.y, glm::dvec3{0.0, 1.0, 0.0});

            const glm::dquat z = glm::angleAxis(
                radians.z, glm::dvec3{0.0, 0.0, 1.0});

            // 保留旧普通物体的 Rx * Ry * Rz 顺序。
            return glm::normalize(x * y * z);
        }

        glm::dvec3 XJTransformComponent::EulerXYZFromRotation(const glm::dquat& rotation)//从XYZ转欧拉角
        {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;

            glm::extractEulerAngleXYZ(glm::mat4_cast(rotation), x, y, z);
            return glm::degrees(glm::dvec3{x, y, z});
        }

        glm::vec3 XJTransformComponent::GetLocalEulerAngles() const//获取本地旋转角度
        {
            return glm::vec3{
                EulerXYZFromRotation(glm::dquat{mLocalRotation})
            };
        }

        const glm::mat4& XJTransformComponent::GetLocalMatrix() const//获取本地矩阵
        {
            if (mLocalMatrixDirty)
            {
                mLocalMatrix = glm::mat4{
                    BuildMatrix(
                        glm::dvec3{mLocalPosition},
                        glm::dquat{mLocalRotation},
                        glm::dvec3{mLocalScale})
                };

                mLocalMatrixDirty = false;
            }

            return mLocalMatrix;
        }

        XJTransformStatus XJTransformComponent::SetLocalTRS(const glm::dvec3& position, const glm::dquat& rotation, const glm::dvec3& scale)//设置本地transform
        {
            glm::vec3 candidatePosition;
            glm::quat candidateRotation;
            glm::vec3 candidateScale;
            glm::mat4 candidateMatrix;

            const auto status = PrepareLocal(
                position,
                rotation,
                scale,
                candidatePosition,
                candidateRotation,
                candidateScale,
                candidateMatrix);

            if (status != XJTransformStatus::Success)
                return status;

            mLocalPosition = candidatePosition;
            mLocalRotation = candidateRotation;
            mLocalScale = candidateScale;

            mLocalMatrix = candidateMatrix;
            mLocalMatrixDirty = false;

            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::SetLocalTRSEuler(const glm::dvec3& position, const glm::dvec3& degrees, const glm::dvec3& scale)//设置本地角？
        {
            if (!IsFinite(degrees))
                return XJTransformStatus::InvalidArgument;

            return SetLocalTRS(
                position,
                RotationFromEulerXYZ(degrees),
                scale);
        }
        /// @name 本地变换分量设置
        /// @{
        /// @brief 局部更新本地 TRS 分量（位置 / 旋转 / 缩放）。
        /// @details
        /// 所有接口均为"读-改-写"：只替换目标分量，其余两个分量从当前成员拷贝后
        /// 一并交给 SetLocalTRS 统一处理。本地变换按 M = T * R * S 复合。
        ///
        /// 校验约定：
        /// - SetLocalEulerAngles 在入口检查 degrees 是否有限（需做三角函数转换）；
        /// - position / rotation / scale 的有限性校验、以及旋转的归一化，
        ///   统一由 SetLocalTRS 完成（rotation 经 NormalizeRotation 归一化，
        ///   零四元数返回 InvalidArgument）。
        ///
        /// 返回值：成功返回 Success；参数非法返回 InvalidArgument。
        XJTransformStatus XJTransformComponent::SetLocalPosition(
        const glm::dvec3& position)
        {
            return SetLocalTRS(
                position,
                glm::dquat{mLocalRotation},
                glm::dvec3{mLocalScale});
        }

        XJTransformStatus XJTransformComponent::SetLocalRotation(
            const glm::dquat& rotation)
        {
            return SetLocalTRS(
                glm::dvec3{mLocalPosition},
                rotation,
                glm::dvec3{mLocalScale});
        }

        XJTransformStatus XJTransformComponent::SetLocalEulerAngles(
            const glm::dvec3& degrees)
        {
            if (!IsFinite(degrees))
                return XJTransformStatus::InvalidArgument;

            return SetLocalRotation(
                RotationFromEulerXYZ(degrees));
        }

        XJTransformStatus XJTransformComponent::SetLocalScale(
            const glm::dvec3& scale)
        {
            return SetLocalTRS(
                glm::dvec3{mLocalPosition},
                glm::dquat{mLocalRotation},
                scale);
        }

        XJTransformStatus XJTransformComponent::RotateLocalY(double deltaDegrees)
        {
            if (!std::isfinite(deltaDegrees)) return XJTransformStatus::InvalidArgument;
            const auto delta = glm::angleAxis(glm::radians(std::remainder(deltaDegrees, 360.0)),
                                             glm::dvec3{0, 1, 0});
            return SetLocalRotation(glm::dquat{mLocalRotation} * delta);
        }

        XJTransformStatus XJTransformComponent::EvaluateNodeChain(const XJNode* node, XJTransformWorldState& outState)
        {
            std::vector<const XJTransformComponent*> chain;
            std::unordered_set<const XJNode*> visited;


            for(const XJNode* current = node; current; current = current->XJGetParent())
            {
                if (!visited.insert(current).second)
                    return XJTransformStatus::InvalidHierarchy;

                const auto* entity = dynamic_cast<const XJEntity*>(current);
                 // 普通 XJNode 没有 Transform，视为单位变换。
                if(!entity)
                    continue;

                if(!entity->IsValid())
                    return XJTransformStatus::InvalidHierarchy;

                if (entity->HasComponent<XJTransformComponent>())
                {
                    chain.push_back(
                        &entity->GetComponent<XJTransformComponent>());
                }
            }

            XJTransformWorldState candidate;

            // 从根到叶，避免递归层级过深。
            for (auto iterator = chain.rbegin();
                 iterator != chain.rend();
                 ++iterator)
            {
                const auto& transform = **iterator;

                    candidate.Matrix *= BuildMatrix(
                    glm::dvec3{transform.mLocalPosition},
                    glm::dquat{transform.mLocalRotation},
                    glm::dvec3{transform.mLocalScale});

                    glm::dquat rotation;

                    if (!NormalizeRotation(
                        candidate.Rotation *
                            glm::dquat{transform.mLocalRotation},
                        rotation))
                {
                    return XJTransformStatus::NonFiniteResult;
                }

                    candidate.Rotation = rotation;

                    if (!IsFiniteMatrix(candidate.Matrix))
                    return XJTransformStatus::NonFiniteResult;
            }

                outState = candidate;
            return XJTransformStatus::Success;
        }

        /// @name 世界空间求值与读取
        /// @{
        /// @brief 沿父链合成世界变换，并从中读取世界位置 / 旋转 / 欧拉角 / 缩放。
        /// @details
        /// 本地变换按 M_local = T * R * S 复合，世界变换按
        ///     M_world = M_parent_world * M_local
        /// 递归合成（EvaluateNodeChain 负责父链部分）。
        ///
        /// 各接口语义：
        /// - EvaluateParentWorld：只求父节点（无 Owner 视为根，父世界 = 单位阵）；
        /// - EvaluateWorld：父世界 * 本地，旋转按 R_world = R_parent * R_local 再归一化，
        ///   矩阵结果做有限性校验，失败返回 NonFiniteResult；
        /// - ReadWorldPosition：取 M_world 第 4 列（平移分量）；
        /// - ReadWorldRotation：取归一化后的 R_world；
        /// - ReadWorldEulerAngles：把 R_world 转回 XYZ 欧拉角（度）；
        /// - ReadLossyWorldScale：取 M_world 三个基向量的长度作为缩放。
        ///
        /// 注意：非均匀缩放 + 旋转会产生剪切，此时世界缩放无法与旋转唯一分离，
        /// 故 ReadLossyWorldScale 是"有损"的，仅反映各轴基向量长度。
        ///
        /// 所有接口在失败时返回对应 XJTransformStatus，成功返回 Success。
        XJTransformStatus XJTransformComponent::EvaluateParentWorld(//旋转到世界
            XJTransformWorldState& outState) const
        {
            const auto* owner = XJGetOwner();

            if (owner && !owner->IsValid())
                return XJTransformStatus::InvalidHierarchy;

            // 无 Owner 的独立组件按根节点处理，便于数学测试。
            return EvaluateNodeChain(
                owner ? owner->XJGetParent() : nullptr,
                outState);
        }

        XJTransformStatus XJTransformComponent::EvaluateWorld(
            XJTransformWorldState& outState) const
        {
            XJTransformWorldState candidate;

            const auto status = EvaluateParentWorld(candidate);

            if (status != XJTransformStatus::Success)
                return status;

            candidate.Matrix *= BuildMatrix(
                glm::dvec3{mLocalPosition},
                glm::dquat{mLocalRotation},
                glm::dvec3{mLocalScale});

            glm::dquat rotation;

            if (!NormalizeRotation(
                    candidate.Rotation *
                        glm::dquat{mLocalRotation},
                    rotation))
            {
                return XJTransformStatus::NonFiniteResult;
            }

            candidate.Rotation = rotation;

            if (!IsFiniteMatrix(candidate.Matrix))
                return XJTransformStatus::NonFiniteResult;

            outState = candidate;
            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::ReadWorldPosition(
            glm::dvec3& outPosition) const
        {
            XJTransformWorldState state;

            const auto status = EvaluateWorld(state);

            if (status != XJTransformStatus::Success)
                return status;

            outPosition = glm::dvec3{state.Matrix[3]};
            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::ReadWorldRotation(
            glm::dquat& outRotation) const
        {
            XJTransformWorldState state;

            const auto status = EvaluateWorld(state);

            if (status != XJTransformStatus::Success)
                return status;

            outRotation = state.Rotation;
            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::ReadWorldEulerAngles(
            glm::dvec3& outDegrees) const
        {
            glm::dquat rotation;

            const auto status = ReadWorldRotation(rotation);

            if (status != XJTransformStatus::Success)
                return status;

            outDegrees = EulerXYZFromRotation(rotation);
            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::ReadLossyWorldScale(
            glm::dvec3& outScale) const
        {
            XJTransformWorldState state;

            const auto status = EvaluateWorld(state);

            if (status != XJTransformStatus::Success)
                return status;

            const glm::dvec3 scale{
                AxisLength(glm::dvec3{state.Matrix[0]}),
                AxisLength(glm::dvec3{state.Matrix[1]}),
                AxisLength(glm::dvec3{state.Matrix[2]})
            };

            if (!IsFinite(scale))
                return XJTransformStatus::NonFiniteResult;

            outScale = scale;
            return XJTransformStatus::Success;
        }
        //World 原子写入
        XJTransformStatus XJTransformComponent::CommitWorldCandidate(
            const XJTransformWorldState& parent,
            const glm::dvec3& position,
            const glm::dquat& rotation,
            const glm::dvec3& scale)
        {
            glm::vec3 candidatePosition;
            glm::quat candidateRotation;
            glm::vec3 candidateScale;
            glm::mat4 candidateLocalMatrix;

            const auto status = PrepareLocal(
                position,
                rotation,
                scale,
                candidatePosition,
                candidateRotation,
                candidateScale,
                candidateLocalMatrix);

            if (status != XJTransformStatus::Success)
                return status;

            // 用真正将被存储的 float TRS 验证最终世界结果。
            const glm::dmat4 worldMatrix =
                parent.Matrix *
                BuildMatrix(
                    glm::dvec3{candidatePosition},
                    glm::dquat{candidateRotation},
                    glm::dvec3{candidateScale});

            if (!IsFiniteMatrix(worldMatrix))
                return XJTransformStatus::NonFiniteResult;

            mLocalPosition = candidatePosition;
            mLocalRotation = candidateRotation;
            mLocalScale = candidateScale;

            mLocalMatrix = candidateLocalMatrix;
            mLocalMatrixDirty = false;

            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::ReadWorldPosition(glm::vec3& outPosition) const
        {
            glm::dvec3 position;
            const auto status = ReadWorldPosition(position);
            if (status != XJTransformStatus::Success) return status;
            if (!FitsFloat(position)) return XJTransformStatus::NonFiniteResult;
            outPosition = glm::vec3{position};
            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::ReadWorldMatrix(glm::mat4& outMatrix) const
        {
            XJTransformWorldState state;
            const auto status = EvaluateWorld(state);
            if (status != XJTransformStatus::Success) return status;
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    if (std::abs(state.Matrix[column][row]) > std::numeric_limits<float>::max())
                        return XJTransformStatus::NonFiniteResult;
            outMatrix = glm::mat4{state.Matrix};
            return XJTransformStatus::Success;
        }

        XJTransformStatus XJTransformComponent::TrySetWorldPose(
            const glm::dvec3& position, const glm::dquat& rotation)
        {
            glm::dquat normalized;
            if (!IsFinite(position) || !NormalizeRotation(rotation, normalized))
                return XJTransformStatus::InvalidArgument;
            XJTransformWorldState parent;
            const auto status = EvaluateParentWorld(parent);
            if (status != XJTransformStatus::Success) return status;
            glm::dmat3 inverse;
            if (!InvertParentBasis(glm::dmat3{parent.Matrix}, inverse))
                return XJTransformStatus::NonInvertibleParent;
            const auto localPosition = inverse * (position - glm::dvec3{parent.Matrix[3]});
            if (!IsFinite(localPosition)) return XJTransformStatus::NonFiniteResult;
            return CommitWorldCandidate(parent, localPosition,
                glm::conjugate(parent.Rotation) * normalized, glm::dvec3{mLocalScale});
        }

        XJTransformStatus XJTransformComponent::TrySetWorldPosition(
            const glm::dvec3& position)
        {
            if (!IsFinite(position))
                return XJTransformStatus::InvalidArgument;

            XJTransformWorldState parent;

            const auto status = EvaluateParentWorld(parent);

            if (status != XJTransformStatus::Success)
                return status;

            glm::dmat3 inverseBasis;

            if (!InvertParentBasis(
                    glm::dmat3{parent.Matrix},
                    inverseBasis))
            {
                return XJTransformStatus::NonInvertibleParent;
            }

            const glm::dvec3 localPosition =
                inverseBasis *
                (position - glm::dvec3{parent.Matrix[3]});

            if (!IsFinite(localPosition))
                return XJTransformStatus::NonFiniteResult;

            return CommitWorldCandidate(
                parent,
                localPosition,
                glm::dquat{mLocalRotation},
                glm::dvec3{mLocalScale});
        }

        XJTransformStatus XJTransformComponent::TrySetWorldRotation(
            const glm::dquat& rotation)
        {
            glm::dquat normalized;

            if (!NormalizeRotation(rotation, normalized))
                return XJTransformStatus::InvalidArgument;

            XJTransformWorldState parent;

            const auto status = EvaluateParentWorld(parent);

            if (status != XJTransformStatus::Success)
                return status;

            // 纯旋转语义不依赖父矩阵是否可逆。
            const glm::dquat localRotation =
                glm::conjugate(parent.Rotation) * normalized;

            return CommitWorldCandidate(
                parent,
                glm::dvec3{mLocalPosition},
                localRotation,
                glm::dvec3{mLocalScale});
        }

        XJTransformStatus XJTransformComponent::TrySetWorldEulerAngles(
            const glm::dvec3& degrees)
        {
            if (!IsFinite(degrees))
                return XJTransformStatus::InvalidArgument;

            return TrySetWorldRotation(
                RotationFromEulerXYZ(degrees));
        }

        XJTransformStatus XJTransformComponent::TrySetWorldScale(
            const glm::dvec3& scale)
        {
            if (!IsFinite(scale) ||
                scale.x < 0.0 ||
                scale.y < 0.0 ||
                scale.z < 0.0)
            {
                return XJTransformStatus::InvalidArgument;
            }

            XJTransformWorldState parent;

            const auto status = EvaluateParentWorld(parent);

            if (status != XJTransformStatus::Success)
                return status;

            const glm::dmat3 inherited =
                glm::dmat3{parent.Matrix} *
                glm::mat3_cast(glm::dquat{mLocalRotation});

            glm::dvec3 localScale;

            for (int axis = 0; axis < 3; ++axis)
            {
                const double inheritedLength =
                    AxisLength(inherited[axis]);

                if (!std::isfinite(inheritedLength))
                    return XJTransformStatus::NonFiniteResult;

                if (inheritedLength == 0.0)
                {
                    if (scale[axis] != 0.0)
                        return XJTransformStatus::NonInvertibleParent;

                    // 目标本来就是零，保留原有 Local 值即可。
                    localScale[axis] = mLocalScale[axis];
                    continue;
                }

                const double magnitude =
                    scale[axis] / inheritedLength;

                if (!std::isfinite(magnitude) ||
                    (scale[axis] != 0.0 && magnitude == 0.0))
                {
                    return XJTransformStatus::NonFiniteResult;
                }

                // 支持保留负零的符号。
                localScale[axis] = std::copysign(
                    magnitude,
                    static_cast<double>(mLocalScale[axis]));
            }

            return CommitWorldCandidate(
                parent,
                glm::dvec3{mLocalPosition},
                glm::dquat{mLocalRotation},
                localScale);
        }



}
