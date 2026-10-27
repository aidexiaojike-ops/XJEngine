#ifndef XJ_SCRIPT_NATIVE_CATALOGUE_H
#define XJ_SCRIPT_NATIVE_CATALOGUE_H

#include "Script/Value/XJScriptValue.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <memory>
//原生签名目录
namespace XJ
{
    enum class XJScriptNativeFunctionId : int32_t
    {
         // 保留既有 ID，避免重新编号。
        TransformRotateY = 0,

        // 数学与构造函数使用独立编号段。
        Vec3Create = 100,

        // 200..223：12 个 Transform 操作，每个包含 Owner/EntityRef 两种形式。
        TransformApiBegin = 200,

        SceneFindByUUID = 300,
        EntityIsValid = 301,
        EntityGetUUID = 302,
        EntitySelf = 303,

        InputGetAxis = 400,
        InputIsKeyHeld = 401,
        InputWasKeyPressed = 402,
        InputWasKeyReleased = 403,

        LogInfo = 500,
        LogWarn = 501,
        LogError = 502
    };

    struct XJScriptNativeDescriptor
    {
        XJScriptNativeFunctionId Id = XJScriptNativeFunctionId::TransformRotateY;

         // 构造函数可以使用空 ObjectName，例如 Vec3(...)。
        std::string ObjectName;
        std::string MemberName;

        XJScriptValueType ReturnType = XJScriptValueType::Void;
        std::vector<XJScriptValueType> ParameterTypes;
    };

    class XJScriptNativeCatalogue
    {
        public:
            static constexpr size_t MaximumParameterCount = 16;

            static std::optional<XJScriptNativeCatalogue> Build(
                std::vector<XJScriptNativeDescriptor> descriptors,
                std::string& error)
            {
                error.clear();

                for (size_t index = 0; index < descriptors.size(); ++index)
                {
                    const auto& descriptor = descriptors[index];

                    if (static_cast<int32_t>(descriptor.Id) < 0 ||
                        descriptor.MemberName.empty())
                    {
                        error = "Native descriptor has an invalid ID or name.";
                        return std::nullopt;
                    }

                    if (descriptor.ReturnType != XJScriptValueType::Void &&
                        !XJIsRuntimeScriptValueType(descriptor.ReturnType))
                    {
                        error = "Native descriptor has an invalid return type.";
                        return std::nullopt;
                    }

                    if (descriptor.ParameterTypes.size() > MaximumParameterCount)
                    {
                        error = "Native descriptor has too many parameters.";
                        return std::nullopt;
                    }

                    for (const auto type : descriptor.ParameterTypes)
                    {
                        if (!XJIsRuntimeScriptValueType(type))
                        {
                            error = "Native parameter cannot have this type.";
                            return std::nullopt;
                        }
                    }

                    for (size_t previous = 0; previous < index; ++previous)
                    {
                        const auto& other = descriptors[previous];

                        if (other.Id == descriptor.Id)
                        {
                            error = "Duplicate native function ID.";
                            return std::nullopt;
                        }

                        // 返回类型不同不能构成重载。
                        if (other.ObjectName == descriptor.ObjectName &&
                            other.MemberName == descriptor.MemberName &&
                            other.ParameterTypes == descriptor.ParameterTypes)
                        {
                            error = "Duplicate native function signature.";
                            return std::nullopt;
                        }
                    }
                }

                return XJScriptNativeCatalogue(std::move(descriptors));

            }

            const XJScriptNativeDescriptor* FindById(XJScriptNativeFunctionId id) const
            {
                const auto found = mIndices.find(static_cast<int32_t>(id));

                return found == mIndices.end()
                    ? nullptr
                    : &mDescriptors[found->second];
            }

            const XJScriptNativeDescriptor* Resolve(
                std::string_view objectName,
                std::string_view memberName,
                std::span<const XJScriptValueType> arguments) const
            {
                for(const auto& descriptor : mDescriptors)
                {
                    if(descriptor.ObjectName != objectName || descriptor.MemberName != memberName || descriptor.ParameterTypes.size()!= arguments.size())
                    {
                        continue;
                    }
                    bool matches = true;

                    for (size_t index = 0; index < arguments.size(); ++index)
                    {
                        if (descriptor.ParameterTypes[index] != arguments[index])
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (matches)
                        return &descriptor;
                }

                return nullptr;
                
            }

            std::span<const XJScriptNativeDescriptor> GetDescriptors() const
            {
                return mDescriptors;
            }

        private:
            explicit XJScriptNativeCatalogue(
                std::vector<XJScriptNativeDescriptor> descriptors)
                : mDescriptors(std::move(descriptors))
            {
                for (size_t index = 0; index < mDescriptors.size(); ++index)
                {
                    mIndices.emplace(
                        static_cast<int32_t>(mDescriptors[index].Id),
                        index);
                }
            }

            std::vector<XJScriptNativeDescriptor> mDescriptors;
            std::unordered_map<int32_t, size_t> mIndices;
    };

    inline std::optional<XJScriptNativeCatalogue>
    XJBuildInitialNativeCatalogue(std::string& error)//构建初始本地目录
    {
        using Type = XJScriptValueType;
        using Id = XJScriptNativeFunctionId;

        std::vector<XJScriptNativeDescriptor> descriptors
        {
            {
                Id::TransformRotateY,
                "Transform",
                "RotateY",
                Type::Void,
                {Type::Float}
            },
            {
                Id::Vec3Create,
                "",
                "Vec3",
                Type::Vec3,
                {Type::Float, Type::Float, Type::Float}
            },

            {
                Id::SceneFindByUUID,
                "Scene",
                "FindByUUID",
                Type::EntityRef,
                {Type::String}
            },
            {
                Id::EntityIsValid,
                "Entity",
                "IsValid",
                Type::Bool,
                {Type::EntityRef}
            },
            {
                Id::EntityGetUUID,
                "Entity",
                "GetUUID",
                Type::String,
                {Type::EntityRef}
            },
            {
                Id::EntitySelf,
                "Entity",
                "Self",
                Type::EntityRef,
                {}
            },

            {
                Id::InputGetAxis,
                "Input",
                "GetAxis",
                Type::Float,
                {Type::String}
            },
            {
                Id::InputIsKeyHeld,
                "Input",
                "IsKeyHeld",
                Type::Bool,
                {Type::String}
            },
            {
                Id::InputWasKeyPressed,
                "Input",
                "WasKeyPressed",
                Type::Bool,
                {Type::String}
            },
            {
                Id::InputWasKeyReleased,
                "Input",
                "WasKeyReleased",
                Type::Bool,
                {Type::String}
            },

            {
                Id::LogInfo,
                "Log",
                "Info",
                Type::Void,
                {Type::String}
            },
            {
                Id::LogWarn,
                "Log",
                "Warn",
                Type::Void,
                {Type::String}
            },
            {
                Id::LogError,
                "Log",
                "Error",
                Type::Void,
                {Type::String}
            }
        };

        // 这个顺序同时是 Dispatcher 的操作编号，发布后不要重新排序。
        constexpr const char* names[] = {
            "GetLocalPosition",       // 0
            "SetLocalPosition",       // 1
            "GetWorldPosition",       // 2
            "TrySetWorldPosition",    // 3

            "GetLocalEulerAngles",    // 4
            "SetLocalEulerAngles",    // 5
            "GetWorldEulerAngles",    // 6
            "TrySetWorldEulerAngles", // 7

            "GetLocalScale",          // 8
            "SetLocalScale",          // 9
            "GetLossyWorldScale",     // 10
            "TrySetWorldScale"        // 11
        };

        for (int operation = 0; operation < 12; ++operation)
        {
            const bool write = operation % 2 != 0;
            const bool world = operation % 4 >= 2;

            const Type returnType =
                !write ? Type::Vec3 :
                world ? Type::Bool :
                        Type::Void;

            // 每个操作都有两种形式：
            // 偶数 ID：操作当前脚本 Owner。
            // 奇数 ID：第一个参数显式传 EntityRef。
            for (int explicitEntity = 0;
                 explicitEntity < 2;
                 ++explicitEntity)
            {
                std::vector<Type> parameters;

                if (explicitEntity)
                    parameters.push_back(Type::EntityRef);

                if (write)
                    parameters.push_back(Type::Vec3);

                descriptors.push_back({
                    static_cast<Id>(
                        200 + operation * 2 + explicitEntity),
                    "Transform",
                    names[operation],
                    returnType,
                    std::move(parameters)
                });
            }
        }

        return XJScriptNativeCatalogue::Build(
            std::move(descriptors), error);
    }

    inline std::shared_ptr<const XJScriptNativeCatalogue> XJGetInitialNativeCatalogue()
    {
        static const std::shared_ptr<const XJScriptNativeCatalogue> initial = []
        {
            std::string error;
            auto result = XJBuildInitialNativeCatalogue(error);
            if (!result)
                return std::shared_ptr<const XJScriptNativeCatalogue>{};
            return std::make_shared<const XJScriptNativeCatalogue>(std::move(*result));
        }();
        return initial;
    }
}

#endif
