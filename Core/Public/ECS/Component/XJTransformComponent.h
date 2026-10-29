#ifndef XJ_TRANSFORM_COMPONENT_H
#define XJ_TRANSFORM_COMPONENT_H

#include "ECS/XJComponent.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace XJ
{
    enum class XJTransformStatus
    {
        Success,
        InvalidArgument,
        InvalidHierarchy,
        NonInvertibleParent,
        NonFiniteResult
    };

    struct XJTransformWorldState
    {
        glm::dmat4 Matrix{1.0};
        glm::dquat Rotation{1.0, 0.0, 0.0, 0.0};
    };

    class XJTransformComponent : public XJComponent
    {
    public:
        // ---------- Local ----------
        const glm::vec3& GetLocalPosition() const { return mLocalPosition; }
        const glm::quat& GetLocalRotation() const { return mLocalRotation; }
        const glm::vec3& GetLocalScale() const { return mLocalScale; }
        glm::vec3 GetLocalEulerAngles() const;
        const glm::mat4& GetLocalMatrix() const;

        XJTransformStatus SetLocalPosition(const glm::dvec3& position);
        XJTransformStatus SetLocalRotation(const glm::dquat& rotation);
        XJTransformStatus SetLocalEulerAngles(const glm::dvec3& degrees);
        XJTransformStatus SetLocalScale(const glm::dvec3& scale);
        XJTransformStatus RotateLocalY(double deltaDegrees);

        // 一次性提交，任何一项失败都不改变原来的 Local 状态。
        XJTransformStatus SetLocalTRS(const glm::dvec3& position,
            const glm::dquat& rotation, const glm::dvec3& scale);
        XJTransformStatus SetLocalTRSEuler(const glm::dvec3& position,
            const glm::dvec3& degrees, const glm::dvec3& scale);

        // ---------- World ----------
        // 读取失败时不改变输出。
        XJTransformStatus EvaluateWorld(XJTransformWorldState& outState) const;
        XJTransformStatus ReadWorldPosition(glm::dvec3& outPosition) const;
        XJTransformStatus ReadWorldRotation(glm::dquat& outRotation) const;
        XJTransformStatus ReadWorldEulerAngles(glm::dvec3& outDegrees) const;
        XJTransformStatus ReadLossyWorldScale(glm::dvec3& outScale) const;

        // float 消费端（GPU、拾取等）显式检查可表示性。
        XJTransformStatus ReadWorldPosition(glm::vec3& outPosition) const;
        XJTransformStatus ReadWorldMatrix(glm::mat4& outMatrix) const;

        XJTransformStatus TrySetWorldPosition(const glm::dvec3& position);
        XJTransformStatus TrySetWorldRotation(const glm::dquat& rotation);
        XJTransformStatus TrySetWorldEulerAngles(const glm::dvec3& degrees);
        XJTransformStatus TrySetWorldPose(const glm::dvec3& position, const glm::dquat& rotation);
        // 非负目标；保留现有 Local Scale 的反射符号。
        XJTransformStatus TrySetWorldScale(const glm::dvec3& scale);

        // 角度制，Rx * Ry * Rz；转换输入必须为有限值/有效单位四元数。
        static glm::dquat RotationFromEulerXYZ(const glm::dvec3& degrees);
        static glm::dvec3 EulerXYZFromRotation(const glm::dquat& rotation);

    private:
        XJTransformStatus EvaluateParentWorld(XJTransformWorldState& outState) const;
        // World 写入的最终原子提交入口。
        XJTransformStatus CommitWorldCandidate(const XJTransformWorldState& parent,
            const glm::dvec3& position, const glm::dquat& rotation, const glm::dvec3& scale);
        static XJTransformStatus EvaluateNodeChain(const XJNode* node, XJTransformWorldState& outState);

        glm::vec3 mLocalPosition{0.0f};
        glm::quat mLocalRotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 mLocalScale{1.0f};
        mutable glm::mat4 mLocalMatrix{1.0f};
        mutable bool mLocalMatrixDirty = true;
    };
}
#endif
