#include "ECS/XJScene.h"

#include "ECS/XJEntity.h"
#include "ECS/Component/XJTransformComponent.h"

#include <spdlog/spdlog.h>

namespace XJ
{

    //构造函数：初始化场景，创建根节点
    XJScene::XJScene()
    {
        mLifetimeToken = std::make_shared<uint8_t>(0);
        mRootNode = std::make_shared<XJNode>();
        mRootNode->XJSetName("RootNode");
    }
    //析构函数：在销毁场景时清理所有资源
    XJScene::~XJScene()
    {
        DestroyAllEntity();
        mRootNode.reset();
        mLifetimeToken.reset();
    }
    // CreateEntity：根据名称创建一个实体
    // 如果没有提供名称，将使用默认名称 "Entity"
    XJEntity* XJScene::CreateEntity(const std::string &name)
    {
         // 调用带 UUID 参数的创建函数，传递默认的空名称
        return CreateEntityWithUUID(XJUUID(), name);
    }

    XJEntity* XJScene::FindEntityByUUID(const XJUUID &id) const
    {
        if(id.mUUID == 0)
            return nullptr; // 如果 UUID 无效，直接返回空指针

        const auto it = mEntitiesByUUID.find(id);

        if (it == mEntitiesByUUID.end())
        {
            return nullptr;
        }

        return GetEntity(it->second);// 如果没有找到对应的实体，返回空指针
    }

    // CreateEntityWithUUID：根据指定的 UUID 和名称创建实体
    // 此方法会给实体添加一个默认的 Transform 组件，并返回该实体的指针
    XJEntity* XJScene::CreateEntityWithUUID(const XJUUID &id, const std::string &name) 
    {

        if(mDestroyingAll)
        {
            spdlog::error("CreateEntityWithUUID failed: scene is destroying all entities, uuid={}, name='{}'", static_cast<uint64_t>(id), name);
            return nullptr;
        }
        // UUID 是序列化、层级恢复、编辑器选择的稳定身份。重复 UUID 会让查找和父子关系恢复混乱。
        if (!id || mEntitiesByUUID.contains(id))
        {
            spdlog::error(
                "CreateEntityWithUUID failed: duplicate uuid={}, name='{}'",
                static_cast<uint64_t>(id),
                name);
            return nullptr;
        }
        // 使用 ECS 注册表创建一个新的实体
        entt::entity enttEntity = mEcsRegistry.create();

        auto entity = std::make_shared<XJEntity>(enttEntity, this);
        auto [it, inserted] = mEntities.emplace(enttEntity, entity);

        if (!inserted)
        {
            // 理论上 entt::create 不应返回 mEntities 已存在的 id。
            // 如果发生，必须回滚 registry，避免 registry 和 mEntities 脱同步。
            spdlog::critical(
                "CreateEntityWithUUID failed: entity id already exists in mEntities, entt={}",
                static_cast<uint32_t>(enttEntity));

            if (mEcsRegistry.valid(enttEntity))
                mEcsRegistry.destroy(enttEntity);

            return nullptr;
        }
        XJEntity* xjEntity = it->second.get();

        auto [uuidIt, uuidInserted] = mEntitiesByUUID.emplace(id, enttEntity);
        if (!uuidInserted)
        {
            mEntities.erase(it);
            mEcsRegistry.destroy(enttEntity);
            return nullptr;
        }

        if (mRootNode)
            mRootNode->XJAddChild(xjEntity); // 设置该实体的父节点为场景的根节点
        // 为实体设置唯一的 UUID（如果未提供名称则使用默认值）
        xjEntity->XJSetUUID(id);
        xjEntity->XJSetName(name.empty() ? "Entity" : name);
        ++mStructureRevision;

        return xjEntity;
    }

    XJEntity* XJScene::CreateEntityWithTransform(const std::string& name)
    {
        XJEntity* entity = CreateEntity(name);
        if (entity)
            entity->AddComponent<XJTransformComponent>(); // 给实体添加默认的 Transform 组件
    
        return entity;
    }
    
    XJEntity* XJScene::CreateEntityWithUUIDAndTransform(const XJUUID& id, const std::string& name)
    {
        XJEntity* entity = CreateEntityWithUUID(id, name);
        if (entity)
            entity->AddComponent<XJTransformComponent>();

        return entity;
    }

     // DestroyEntity：立即销毁指定实体。不要在 registry.view().each() 内调用。
    void XJScene::DestroyEntity(const XJEntity* entity)
    {
        if (mScriptExecutionDepth != 0)
        {
            QueueDestroyEntity(entity);
            return;
        }

        DestroyEntityImmediate(entity);
    }

    void XJScene::QueueDestroyEntity(const XJEntity* entity)
    {
        if (!entity)
            return;
        const entt::entity ecsEntity = entity->GetEcsEntity();
        // 不接受其他 Scene 中数值相同的 EnTT ID。
        auto it = mEntities.find(ecsEntity);
        if (it == mEntities.end() || it->second.get() != entity ||
            !mEcsRegistry.valid(ecsEntity) || mDestroyingEntities.contains(ecsEntity))
            return;

        if (mPendingDestroySet.insert(ecsEntity).second)
            mPendingDestroyEntities.push_back(ecsEntity);
    }

    void XJScene::FlushDestroyQueue()
    {

        if(mScriptExecutionDepth != 0)
        {
            return;
        }

        std::vector<entt::entity> pending = std::move(mPendingDestroyEntities);
        mPendingDestroyEntities.clear();
        mPendingDestroySet.clear();

        for (entt::entity ecsEntity : pending)
        {
            XJEntity* entity = GetEntity(ecsEntity);
            if (entity)
                DestroyEntityImmediate(entity);
        }
    }

    void XJScene::DestroyEntityImmediate(const XJEntity* entity)
    {
        if (!entity)
            return;

        const entt::entity ecsEntity = entity->GetEcsEntity();
        const auto entityIt =  mEntities.find(ecsEntity);
        if (entityIt == mEntities.end() ||
            entityIt->second.get() != entity ||
            !mEcsRegistry.valid(ecsEntity))
        {
            return;
        }
    
        if(!mDestroyingEntities.insert(ecsEntity).second)
        {
            spdlog::warn("DestroyEntityImmediate skipped: entity is already being destroyed, entt={}", static_cast<uint32_t>(ecsEntity));
            return;
        }
         // 保持 wrapper 有效，回调中的场景创建也不会使它失效。
        const std::shared_ptr<XJEntity> entityOwner = entityIt->second;
        XJEntity* entityToDestroy = entityOwner.get();

        struct DestroyGuard
        {
            std::unordered_set<entt::entity>& Entities;
            entt::entity Entity;

            ~DestroyGuard()
            {
                Entities.erase(Entity);
            }
        } guard{mDestroyingEntities, ecsEntity};

        // 保存 ECS ID，而不是跨递归保留可能失效的 child 指针。
        std::vector<entt::entity> childIds;
        for (XJNode* child : entityToDestroy->XJGetChildren())
        {
            if (XJEntity* childEntity = dynamic_cast<XJEntity*>(child))
                childIds.push_back(childEntity->GetEcsEntity());
        }

        for (const entt::entity childId : childIds)
        {
            if (XJEntity* child = GetEntity(childId))
                DestroyEntityImmediate(child);
        }
        // 此时 UUID、registry、Transform 等组件仍然有效。
        // 通知中的递归删除/清空必须排队，避免重入正在销毁的实体。
        {
            ScriptExecutionScope notificationScope(*this);
            mBeforeEntityDestroy.publish(*entityToDestroy);
        }
        if (XJNode* parent = entityToDestroy->XJGetParent())
            parent->XJRemoveChild(entityToDestroy);


        const XJUUID uuid = entityToDestroy->XJGetUUID();
        const auto uuidIt = mEntitiesByUUID.find(uuid);
        if (uuidIt != mEntitiesByUUID.end() && uuidIt->second == entityToDestroy->GetEcsEntity())
            mEntitiesByUUID.erase(uuidIt);
    
        if (mEcsRegistry.valid(ecsEntity))
            mEcsRegistry.destroy(ecsEntity);

        mPendingDestroySet.erase(ecsEntity);
        // 回调可能创建实体导致 unordered_map rehash，不继续使用旧 iterator。
        mEntities.erase(ecsEntity);
        ++mStructureRevision;
    }

     
    void XJScene::DestroyAllEntity() // DestroyAllEntity：销毁场景中的所有实体
    {

        if (mScriptExecutionDepth != 0)
        {
            for (const auto& [ecsEntity, entity] : mEntities)
            {
                (void)ecsEntity;
                QueueDestroyEntity(entity.get());
            }
            return;
        }

        if (mDestroyingAll)
            return;

        mDestroyingAll = true;

        struct ResetGuard
        {
            bool& Flag;

            ~ResetGuard()
            {
                Flag = false;
            }
        } guard{mDestroyingAll};

        while (!mEntities.empty())
        {
            // 每个实体必须经过通知路径，不能直接 registry.clear()。
            const auto entity = mEntities.begin()->second;
            DestroyEntityImmediate(entity.get());
        }

        mPendingDestroyEntities.clear();
        mPendingDestroySet.clear();
        mEntitiesByUUID.clear();

        if (mRootNode)
            mRootNode->XJClearChildren();
    }

    // GetEntity：根据实体 ID 获取对应的实体对象
    XJEntity *XJScene::GetEntity(entt::entity enttEntity) const
    {
        auto it = mEntities.find(enttEntity);
        if (it == mEntities.end())
            return nullptr;

        if (!mEcsRegistry.valid(enttEntity))
            return nullptr;

        return it->second.get();
    }

    const std::unordered_map<entt::entity, std::shared_ptr<XJEntity>>& XJScene::GetEntities() const
    {
        return mEntities;
    }
}
