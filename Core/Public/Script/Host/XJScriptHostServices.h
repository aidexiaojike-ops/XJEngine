#ifndef XJ_SCRIPT_HOST_SERVICES_H
#define XJ_SCRIPT_HOST_SERVICES_H

#include "Script/Value/XJScriptValue.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace XJ
{
    enum class XJScriptSpace {Local, World};//空间
    enum class XJScriptPhase { Create, Update, FixedUpdate, Destroy };//阶段
    enum class XJScriptLogLevel { Info, Warning, Error };//log等级
    enum class XJScriptTransformProperty { Position, EulerAngles, Scale };//transform
   
    enum class XJScriptHostError //报错符号
    {
        None, InvalidArgument, InvalidUUID, MissingEntity,
        MissingTransform, NonInvertibleParent, NonFiniteResult,
        ServiceUnavailable
    };

    struct XJScriptHostStatus//主机状态
    {
        XJScriptHostError Code = XJScriptHostError::None;
        std::string Message;
        bool IsValid() const { return Code == XJScriptHostError::None; }
    };

    template<typename T> struct XJScriptHostResult//主机结果
    {   
        std::optional<T> Value;
        XJScriptHostStatus Status;
        bool IsValid() const { return Status.IsValid() && Value.has_value(); }
    };

    struct XJScriptCallContext//联系上下文
    {
        XJScriptEntityRef Owner;
        uint64_t SlotId = 0;
        uint64_t FrameId = 0;
        XJScriptPhase Phase = XJScriptPhase::Create;
    };

    class IXJScriptEntityService//脚本控制ECS
    {
        public:
            virtual ~IXJScriptEntityService() = default;
            virtual XJScriptHostResult<XJScriptEntityRef> FindByUUID(std::string_view uuid) = 0;
            virtual bool IsValid(XJScriptEntityRef entity) const = 0;
            virtual XJScriptHostResult<std::string> GetUUID(XJScriptEntityRef entity) const = 0;
    };

    class IXJScriptTransformService//脚本控制Transform
    {
        public:
            virtual ~IXJScriptTransformService() = default;
            virtual XJScriptHostResult<XJScriptVec3> Read(XJScriptEntityRef entity, XJScriptSpace space, XJScriptTransformProperty property) = 0;
            virtual XJScriptHostStatus Write(XJScriptEntityRef entity, XJScriptSpace space, XJScriptTransformProperty property, XJScriptVec3 value) = 0;
    };

    class IXJScriptInputService//脚本 输入
    {
        public:
            virtual ~IXJScriptInputService() = default;
            virtual XJScriptHostResult<double> GetAxis(std::string_view axis) const = 0;
            virtual XJScriptHostResult<bool> IsKeyHeld(std::string_view key) const = 0;
            virtual XJScriptHostResult<bool> WasKeyPressed(std::string_view key, XJScriptPhase phase) const = 0;
            virtual XJScriptHostResult<bool> WasKeyReleased(std::string_view key, XJScriptPhase phase) const = 0;
    };

    class IXJScriptLogService//脚本log
    {
        public:
            virtual ~IXJScriptLogService() = default;
            virtual XJScriptHostStatus Write(const XJScriptCallContext& context, XJScriptLogLevel level, std::string_view message) = 0;
    };

    struct XJScriptHostServices//状态
    {
        std::shared_ptr<IXJScriptEntityService> Entities;
        std::shared_ptr<IXJScriptTransformService> Transforms;
        std::shared_ptr<IXJScriptInputService> Input;
        std::shared_ptr<IXJScriptLogService> Log;
    };
}

#endif