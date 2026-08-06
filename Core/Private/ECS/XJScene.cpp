#include "ECS/XJScene.h"

#include "ECS/XJEntity.h"
#include "ECS/Component/XJTransformComponent.h"

#include <spdlog/spdlog.h>

namespace XJ
{

    //构造函数：初始化场景，创建根节点
    XJScene::XJScene()
    {
        mRootNode = std::make_shared<XJNode>();
        mRootNode->XJSetName("RootNode");
    }
    //析构函数：在销毁场景时清理所有资源
    XJScene::~XJScene()
    {
        DestroyAllEntity();
        mRootNode.reset();
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
        if(!id)
            return nullptr; // 如果 UUID 无效，直接返回空指针

        for (const auto& [enttEntity, entity] : mEntities)
        {
            if (!entity)
                continue;

            if (entity && entity->XJGetUUID() == id)
                return entity.get();
        }
        return nullptr; // 如果没有找到对应的实体，返回空指针
    }

    // CreateEntityWithUUID：根据指定的 UUID 和名称创建实体
    // 此方法会给实体添加一个默认的 Transform 组件，并返回该实体的指针
    XJEntity* XJScene::CreateEntityWithUUID(const XJUUID &id, const std::string &name) 
    {
        // UUID 是序列化、层级恢复、编辑器选择的稳定身份。重复 UUID 会让查找和父子关系恢复混乱。
        if (id && FindEntityByUUID(id))
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

        if (mRootNode)
            mRootNode->XJAddChild(xjEntity); // 设置该实体的父节点为场景的根节点
        // 为实体设置唯一的 UUID（如果未提供名称则使用默认值）
        xjEntity->XJSetUUID(id);
        xjEntity->XJSetName(name.empty() ? "Entity" : name);

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
        DestroyEntityImmediate(entity);
    }

    void XJScene::QueueDestroyEntity(const XJEntity* entity)
    {
        if (!entity)
            return;

        const entt::entity ecsEntity = entity->GetEcsEntity();
        if (!mEcsRegistry.valid(ecsEntity))
            return;

        if (mPendingDestroySet.insert(ecsEntity).second)
            mPendingDestroyEntities.push_back(ecsEntity);
    }

    void XJScene::FlushDestroyQueue()
    {
        std::vector<entt::entity> pending = std::move(mPendingDestroyEntities);
        mPendingDestroyEntities.clear();
        mPendingDestroySet.clear();

        for (entt::entity ecsEntity : pending)
        {
            XJEntity* entity = XJGetEntities(ecsEntity);
            if (entity)
                DestroyEntityImmediate(entity);
        }
    }

    void XJScene::DestroyEntityImmediate(const XJEntity* entity)
    {
        if (!entity)
            return;
    
        auto it = mEntities.find(entity->GetEcsEntity());
        if (it == mEntities.end())
            return;
    
        // 防止传入旧 scene 或旧包装对象，但 entt id 碰巧相同，误删当前 scene 的实体。
        if (it->second.get() != entity)
        {
            spdlog::warn("DestroyEntity skipped: entity wrapper does not belong to this scene.");
            return;
        }
    
        XJEntity* entityToDestroy = it->second.get();
    
        std::vector<XJNode*> children = entityToDestroy->XJGetChildren();
        for (XJNode* child : children)
        {
            if (XJEntity* childEntity = dynamic_cast<XJEntity*>(child))
                DestroyEntityImmediate(childEntity);
        }
    
        XJNode* parent = entityToDestroy->XJGetParent();
        if (parent)
            parent->XJRemoveChild(entityToDestroy);
    
        if (mEcsRegistry.valid(entityToDestroy->GetEcsEntity()))
            mEcsRegistry.destroy(entityToDestroy->GetEcsEntity());

        mPendingDestroySet.erase(entityToDestroy->GetEcsEntity());
        mEntities.erase(it);
    }

     
    void XJScene::DestroyAllEntity() // DestroyAllEntity：销毁场景中的所有实体
    {

        mPendingDestroyEntities.clear();
        mPendingDestroySet.clear();

        if(mRootNode)
            mRootNode->XJClearChildren();
        // 清空 ECS 注册表中的所有实体
        mEcsRegistry.clear();

        // 清空实体列表
        mEntities.clear();
    }

    // GetEntity：根据实体 ID 获取对应的实体对象
    XJEntity *XJScene::XJGetEntities(entt::entity enttEntity) const
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
