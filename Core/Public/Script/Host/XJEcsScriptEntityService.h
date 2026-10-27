#ifndef XJ_ECS_SCRIPT_ENTITY_SERVICE_H
#define XJ_ECS_SCRIPT_ENTITY_SERVICE_H

#include "Script/Host/XJScriptHostServices.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <charconv>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

//实体句柄服务
namespace XJ
{
    class XJEcsScriptEntityService final : public IXJScriptEntityService 
    {
        public:
            explicit XJEcsScriptEntityService(XJScene& scene)
                : mScene(&scene),
                mSceneLifetime(scene.GetLifetimeToken()),
                mScopeId(AllocateScope())
            {
            }

            XJEcsScriptEntityService(const XJEcsScriptEntityService&) = delete;
            XJEcsScriptEntityService& operator=(const XJEcsScriptEntityService&) = delete;

            XJScriptEntityRef Capture(XJEntity& entity)//捕获
            {
                if(!HasScene() || !entity.IsValid())
                    return{};

                const auto& entities = mScene->GetEntities();
                const auto found = entities.find(entity.GetEcsEntity());

                if(found == entities.end() || found->second.get() != &entity)
                    return{};
                
                const auto existing = mIndices.find(entity.GetEcsEntity());

                if(existing != mIndices.end())
                {
                    const uint32_t index = existing->second;
                    const auto previous = mEntries[index].lock();

                    if(previous.get() == &entity)
                        return {mScopeId, index, 1};
                }

                if(mEntries.size() >= static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
                {
                    spdlog::error("Script entity handle table exhausted.");
                    throw std::overflow_error("Script entity handle table exhausted");
                }

                const uint32_t index = static_cast<uint32_t>(mEntries.size());

                mEntries.emplace_back(found->second);
                mIndices[entity.GetEcsEntity()] = index;

                return {mScopeId, index, 1};
            }

            std::shared_ptr<XJEntity> Resolve(XJScriptEntityRef reference) const//解决
            {
                if(!HasScene() || reference.ScopeId != mScopeId || reference.Generation != 1 ||  reference.Index >= mEntries.size())
                {
                    return {};
                }

                auto entity = mEntries[reference.Index].lock();

                if(!entity || !entity->IsValid())
                    return {};
                // IsValid() 同时检查 registry 和 wrapper 身份。
                return entity;
            }

            bool IsValid(XJScriptEntityRef reference) const override
            {
                return static_cast<bool>(Resolve(reference));
            }


            XJScriptHostResult<XJScriptEntityRef> FindByUUID(std::string_view text) override//查询UUID
            {
                if (!HasScene())
                {
                    return {
                        {},
                        {
                            XJScriptHostError::ServiceUnavailable,
                            "Runtime scene is unavailable"
                        }
                    };
                }

                uint64_t value = 0;

                if (!ParseUUID(text, value))
                {
                    return {
                        {},
                        {
                            XJScriptHostError::InvalidUUID,
                            "UUID must be a non-zero decimal or 0x hexadecimal integer"
                        }
                    };
                }

                XJEntity* entity =
                    mScene->FindEntityByUUID(XJUUID{value});

                // 格式正确但未找到：成功返回 null。
                return {
                    entity ? Capture(*entity) : XJScriptEntityRef{},
                    {}
                };
            }

            XJScriptHostResult<std::string> GetUUID(XJScriptEntityRef reference) const override//获取赋值UUID
            {
                const auto entity = Resolve(reference);

                if (!entity)
                {
                    return {
                        {},
                        {
                            XJScriptHostError::MissingEntity,
                            "Entity reference is invalid or expired"
                        }
                    };
                }

                return {
                    std::to_string(
                        static_cast<uint64_t>(entity->XJGetUUID())),
                    {}
                };
            }


        private:
            bool HasScene() const//是否有场景
            {
                return mScene && !mSceneLifetime.expired();
            }

            static bool ParseUUID(std::string_view text, uint64_t& result)//解析UUID
            {
                int base = 10;

                if (text.starts_with("0x") ||
                    text.starts_with("0X"))
                {
                    text.remove_prefix(2);
                    base = 16;
                }

                if (text.empty())
                    return false;

                const auto parsed = std::from_chars(
                    text.data(),
                    text.data() + text.size(),
                    result,
                    base);

                return parsed.ec == std::errc{} &&
                       parsed.ptr == text.data() + text.size() &&
                       result != 0;
            }

            static uint64_t AllocateScope()//分配范围
            {
                // 仅分配身份，不保存游戏状态。
                static std::atomic<uint64_t> next{1};

                uint64_t value = next.load(
                    std::memory_order_relaxed);

                for (;;)
                {
                    if (value ==
                        std::numeric_limits<uint64_t>::max())
                    {
                        spdlog::error(
                            "Script session scope ID exhausted.");

                        throw std::overflow_error(
                            "Script session scope ID exhausted");
                    }

                    if (next.compare_exchange_weak(
                        value,
                        value + 1,
                        std::memory_order_relaxed))
                    {
                        return value;
                    }
                }
            }


            XJScene* mScene = nullptr;
            std::weak_ptr<void> mSceneLifetime;

            uint64_t mScopeId = 0;

            std::vector<std::weak_ptr<XJEntity>> mEntries;

            std::unordered_map<entt::entity, uint32_t> mIndices;

    };
    

}

#endif
