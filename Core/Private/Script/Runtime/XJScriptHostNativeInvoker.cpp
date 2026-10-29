#include "Script/Runtime/XJScriptHostNativeInvoker.h"

#include <utility>

namespace XJ
{
    namespace
    {
        using NativeResult = XJScriptNativeInvokeResult;

        NativeResult HostFailure(const XJScriptHostStatus& status)//主机失败
        {
            return NativeResult::Failure(
                XJScriptNativeErrorCode::HostFailure,
                status.Message.empty()
                    ? "Script host operation failed"
                    : status.Message);
        }

        NativeResult Unavailable(std::string service)//不开用
        {
            return NativeResult::Failure(
                XJScriptNativeErrorCode::HostFailure,
                std::move(service) + " service is unavailable");
        }

        template<typename T> NativeResult Convert(const XJScriptHostResult<T>& result)//转化
        {
            if (!result.IsValid())
            {
                if (!result.Status.IsValid())
                    return HostFailure(result.Status);

                return NativeResult::Failure(
                    XJScriptNativeErrorCode::HostFailure,
                    "Host operation succeeded without a result");
            }

            return NativeResult::Success(XJScriptValue{*result.Value});
        }

        NativeResult Convert(const XJScriptHostStatus& status)//转化
        {
            return status.IsValid()
                ? NativeResult::Success()
                : HostFailure(status);
        }
    }

    XJScriptHostNativeInvoker::XJScriptHostNativeInvoker(XJScriptHostServices services)
        : mServices(std::move(services))
    {
    }

    std::shared_ptr<const XJScriptNativeCatalogue> XJScriptHostNativeInvoker::GetNativeCatalogue() const//获取原生目录
    {
        return XJGetInitialNativeCatalogue();
    }

    XJScriptNativeInvokeResult XJScriptHostNativeInvoker::InvokeTransform(//调试变化
        int operation,
        bool explicitEntity,
        std::span<const XJScriptValue> arguments,
        XJScriptNativeContext& context)
    {
        if(!mServices.Transforms)//先判断对象有没有tansform数据
            return Unavailable("Transform");
        //获取uuid
        const XJScriptEntityRef entity = explicitEntity ? std::get<XJScriptEntityRef>(arguments[0]) : context.Call.Owner;

        const bool write = operation % 2 != 0;
        const XJScriptSpace space = operation % 4>= 2 ? XJScriptSpace::World : XJScriptSpace::Local;//判断坐标空间

        XJScriptTransformProperty property;//存储获取对象的transform

        switch (operation / 4)
        {
            case 0:
                property = XJScriptTransformProperty::Position;
                break;

            case 1:
                property = XJScriptTransformProperty::EulerAngles;
                break;

            case 2:
                property = XJScriptTransformProperty::Scale;
                break;

            default:
                return NativeResult::Failure(
                    XJScriptNativeErrorCode::UnsupportedFunction,
                    "Unknown Transform operation");
        }

        if (!write)
        {
            return Convert(
                mServices.Transforms->Read(
                    entity, space, property));
        }

        const auto valueIndex = explicitEntity ? 1 : 0;
        const auto value =  std::get<XJScriptVec3>(arguments[valueIndex]);
        const auto status = mServices.Transforms->Write(entity, space, property, value);

        if(space == XJScriptSpace::Local)
            return Convert(status);

        // TrySet 只将“数学上无法应用”转换为 false。
        // 参数错误、失效实体、缺少组件仍然使调用失败。
        if(status.IsValid())
            return NativeResult::Success(true);

        if(status.Code == XJScriptHostError::NonInvertibleParent || status.Code == XJScriptHostError::NonFiniteResult)
        {
            return NativeResult::Success(false);
        }

        return HostFailure(status);
    }

    XJScriptNativeInvokeResult XJScriptHostNativeInvoker::Invoke(//调用
        XJScriptNativeFunctionId functionId,
        std::span<const XJScriptValue> arguments,
        XJScriptNativeContext& context)
    {
        const auto catalogue = GetNativeCatalogue();

        const auto* descriptor = catalogue ? catalogue->FindById(functionId) : nullptr;

        if (!descriptor)
        {
            return NativeResult::Failure(
                XJScriptNativeErrorCode::UnsupportedFunction,
                "Unknown native function ID");
        }

        if (arguments.size() != descriptor->ParameterTypes.size())
        {
            return NativeResult::Failure(
                XJScriptNativeErrorCode::InvalidArguments,
                "Native argument count mismatch");
        }

        for (size_t index = 0; index < arguments.size(); ++index)
        {
            if (XJGetScriptValueType(arguments[index]) !=
                    descriptor->ParameterTypes[index] ||
                !XJIsWellFormedScriptValue(arguments[index]))
            {
                return NativeResult::Failure(
                    XJScriptNativeErrorCode::InvalidArguments,
                    "Native argument type or value is invalid");
            }
        }

        const int id = static_cast<int>(functionId);

        if (id >= 200 && id < 224)
        {
            const int encoded = id - 200;

            return InvokeTransform(
                encoded / 2,
                encoded % 2 != 0,
                arguments,
                context);
        }

        using Id = XJScriptNativeFunctionId;

        switch (functionId)
        {
            case Id::TransformRotateY:
            {
                if (!mServices.Transforms)
                    return Unavailable("Transform");

                return Convert(mServices.Transforms->RotateLocalY(
                    context.Call.Owner, std::get<double>(arguments[0])));
            }

            case Id::SceneFindByUUID:
            {
                if (!mServices.Entities)
                    return Unavailable("Entity");

                return Convert(mServices.Entities->FindByUUID(std::get<std::string>(arguments[0])));
            }

            case Id::EntityIsValid:
            {
                if (!mServices.Entities)
                    return Unavailable("Entity");

                return NativeResult::Success(
                    mServices.Entities->IsValid(
                        std::get<XJScriptEntityRef>(
                            arguments[0])));
            }

            case Id::EntityGetUUID:
            {
                if (!mServices.Entities)
                    return Unavailable("Entity");

                return Convert(
                    mServices.Entities->GetUUID(
                        std::get<XJScriptEntityRef>(
                            arguments[0])));
            }

            case Id::EntitySelf:
            {
                if (!mServices.Entities)
                    return Unavailable("Entity");

                if (!mServices.Entities->IsValid(
                    context.Call.Owner))
                {
                    return NativeResult::Failure(
                        XJScriptNativeErrorCode::MissingEntity,
                        "Script owner is invalid");
                }

                return NativeResult::Success(
                    context.Call.Owner);
            }

            case Id::InputGetAxis:
            {
                if (!mServices.Input)
                    return Unavailable("Input");

                return Convert(
                    mServices.Input->GetAxis(
                        std::get<std::string>(
                            arguments[0])));
            }

            case Id::InputIsKeyHeld:
            {
                if (!mServices.Input)
                    return Unavailable("Input");

                return Convert(
                    mServices.Input->IsKeyHeld(
                        std::get<std::string>(
                            arguments[0])));
            }

            case Id::InputWasKeyPressed:
            {
                if (!mServices.Input)
                    return Unavailable("Input");

                return Convert(
                    mServices.Input->WasKeyPressed(
                        std::get<std::string>(
                            arguments[0]),
                        context.Call.Phase));
            }

            case Id::InputWasKeyReleased:
            {
                if (!mServices.Input)
                    return Unavailable("Input");

                return Convert(
                    mServices.Input->WasKeyReleased(
                        std::get<std::string>(
                            arguments[0]),
                        context.Call.Phase));
            }

            case Id::LogInfo:
            case Id::LogWarn:
            case Id::LogError:
            {
                if (!mServices.Log)
                    return Unavailable("Log");

                XJScriptLogLevel level =
                    XJScriptLogLevel::Info;

                if (functionId == Id::LogWarn)
                    level = XJScriptLogLevel::Warning;
                else if (functionId == Id::LogError)
                    level = XJScriptLogLevel::Error;

                return Convert(
                    mServices.Log->Write(
                        context.Call,
                        level,
                        std::get<std::string>(
                            arguments[0])));
            }

            default:
                break;
        }

        // Vec3Create 由现有 XJScriptValueNativeInvoker 处理，
        // VM 执行时已经自动套上该适配器。
        return NativeResult::Failure(
            XJScriptNativeErrorCode::UnsupportedFunction,
            "Native function is not handled by the host dispatcher");
    }
    
}
