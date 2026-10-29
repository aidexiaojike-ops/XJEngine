#ifndef XJ_ECS_SCRIPT_TRANSFORM_SERVICE_H
#define XJ_ECS_SCRIPT_TRANSFORM_SERVICE_H

#include "Script/Host/XJEcsScriptEntityService.h"
#include "ECS/Component/XJTransformComponent.h"

#include <memory>
#include <stdexcept>

namespace XJ
{
    // 只适配脚本类型和错误；所有坐标运算由 Transform 统一负责。
    class XJEcsScriptTransformService final : public IXJScriptTransformService
    {
    public:
        explicit XJEcsScriptTransformService(std::shared_ptr<XJEcsScriptEntityService> entities)
            : mEntities(std::move(entities))
        {
            if (!mEntities)
            {
                spdlog::error("Script Transform service requires an entity service.");
                throw std::invalid_argument("Missing script entity service");
            }
        }

        XJScriptHostResult<XJScriptVec3> Read(XJScriptEntityRef reference,
            XJScriptSpace space, XJScriptTransformProperty property) override
        {
            if (space != XJScriptSpace::Local && space != XJScriptSpace::World)
                return {{}, {XJScriptHostError::InvalidArgument, "Unknown Transform space"}};
            const auto entity = mEntities->Resolve(reference);
            if (!entity)
                return {{}, {XJScriptHostError::MissingEntity, "Entity reference is invalid"}};
            if (!entity->HasComponent<XJTransformComponent>())
                return {{}, {XJScriptHostError::MissingTransform, "Entity has no Transform"}};

            const auto& transform = entity->GetComponent<XJTransformComponent>();
            glm::dvec3 value;
            auto status = XJTransformStatus::Success;
            switch (property)
            {
                case XJScriptTransformProperty::Position:
                    if (space == XJScriptSpace::Local) value = transform.GetLocalPosition();
                    else status = transform.ReadWorldPosition(value);
                    break;
                case XJScriptTransformProperty::EulerAngles:
                    if (space == XJScriptSpace::Local) value = transform.GetLocalEulerAngles();
                    else status = transform.ReadWorldEulerAngles(value);
                    break;
                case XJScriptTransformProperty::Scale:
                    if (space == XJScriptSpace::Local) value = transform.GetLocalScale();
                    else status = transform.ReadLossyWorldScale(value);
                    break;
                default:
                    return {{}, {XJScriptHostError::InvalidArgument, "Unknown Transform property"}};
            }
            if (status != XJTransformStatus::Success)
                return {{}, ConvertStatus(status)};
            return {XJScriptVec3{value.x, value.y, value.z}, {}};
        }

        XJScriptHostStatus Write(XJScriptEntityRef reference, XJScriptSpace space,
            XJScriptTransformProperty property, XJScriptVec3 value) override
        {
            if ((space != XJScriptSpace::Local && space != XJScriptSpace::World) || !value.IsFinite())
                return {XJScriptHostError::InvalidArgument, "Transform space or value is invalid"};
            const auto entity = mEntities->Resolve(reference);
            if (!entity)
                return {XJScriptHostError::MissingEntity, "Entity reference is invalid"};
            if (!entity->HasComponent<XJTransformComponent>())
                return {XJScriptHostError::MissingTransform, "Entity has no Transform"};
            auto& transform = entity->GetComponent<XJTransformComponent>();
            const glm::dvec3 requested{value.X, value.Y, value.Z};
            switch (property)
            {
                case XJScriptTransformProperty::Position:
                    return ConvertStatus(space == XJScriptSpace::Local
                        ? transform.SetLocalPosition(requested) : transform.TrySetWorldPosition(requested));
                case XJScriptTransformProperty::EulerAngles:
                    return ConvertStatus(space == XJScriptSpace::Local
                        ? transform.SetLocalEulerAngles(requested) : transform.TrySetWorldEulerAngles(requested));
                case XJScriptTransformProperty::Scale:
                    return ConvertStatus(space == XJScriptSpace::Local
                        ? transform.SetLocalScale(requested) : transform.TrySetWorldScale(requested));
                default:
                    return {XJScriptHostError::InvalidArgument, "Unknown Transform property"};
            }
        }

        XJScriptHostStatus RotateLocalY(XJScriptEntityRef reference, double deltaDegrees) override
        {
            const auto entity = mEntities->Resolve(reference);
            if (!entity) return {XJScriptHostError::MissingEntity, "Entity reference is invalid"};
            if (!entity->HasComponent<XJTransformComponent>())
                return {XJScriptHostError::MissingTransform, "Entity has no Transform"};
            return ConvertStatus(entity->GetComponent<XJTransformComponent>().RotateLocalY(deltaDegrees));
        }

    private:
        static XJScriptHostStatus ConvertStatus(XJTransformStatus status)
        {
            switch (status)
            {
                case XJTransformStatus::Success: return {};
                case XJTransformStatus::InvalidArgument:
                    return {XJScriptHostError::InvalidArgument, "Invalid Transform argument"};
                case XJTransformStatus::InvalidHierarchy:
                    return {XJScriptHostError::InvalidArgument, "Invalid Transform hierarchy"};
                case XJTransformStatus::NonInvertibleParent:
                    return {XJScriptHostError::NonInvertibleParent, "Parent transform cannot represent the requested value"};
                case XJTransformStatus::NonFiniteResult:
                    return {XJScriptHostError::NonFiniteResult, "Transform result exceeds numeric storage range"};
            }
            return {XJScriptHostError::InvalidArgument, "Unknown Transform status"};
        }

        std::shared_ptr<XJEcsScriptEntityService> mEntities;
    };
}
#endif
