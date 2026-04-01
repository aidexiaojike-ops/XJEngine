#include "ECS/XJScene.h"

#include "ECS/XJEntity.h"
#include "ECS/Component/XJTransformComponent.h"

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
        // 释放根节点
        mRootNode.reset();
        // 销毁所有实体
       DestroyAllEntity();

         // 清空实体映射表
        mEntities.clear();
    }
    // CreateEntity：根据名称创建一个实体
    // 如果没有提供名称，将使用默认名称 "Entity"
    XJEntity* XJScene::CreateEntity(const std::string &name)
    {
         // 调用带 UUID 参数的创建函数，传递默认的空名称
        return CreateEntityWithUUID(XJUUID(), name);
    }

    // CreateEntityWithUUID：根据指定的 UUID 和名称创建实体
    // 此方法会给实体添加一个默认的 Transform 组件，并返回该实体的指针
    XJEntity* XJScene::CreateEntityWithUUID(const XJUUID &id, const std::string &name) 
    {
        // 使用 ECS 注册表创建一个新的实体
        auto enttEntity = mEcsRegistry.create();

        // 将新创建的实体加入到实体列表中，存储为一个智能指针
        mEntities.insert({ enttEntity, std::make_shared<XJEntity>(enttEntity, this) });

        // 设置该实体的父节点为场景的根节点
        mEntities[enttEntity]->XJSetParent(mRootNode.get());

        // 为实体设置唯一的 UUID（如果未提供名称则使用默认值）
        mEntities[enttEntity]->XJSetId(id);
        mEntities[enttEntity]->XJSetName(name.empty() ? "Entity" : name);

        // 给实体添加默认的 Transform 组件
        mEntities[enttEntity]->AddComponent<XJTransformComponent>();

        // 返回实体的指针
        return mEntities[enttEntity].get();
    }

     // DestroyEntity：销毁指定的实体并清除其相关资源
    void XJScene::DestroyEntity(const XJEntity *entity) 
    {
        // 如果实体有效，首先销毁该实体
        if (entity && entity->IsValid()) {
            mEcsRegistry.destroy(entity->GetEcsEntity());
        }

        // 在实体列表中查找该实体
        auto it = mEntities.find(entity->GetEcsEntity());
        
        // 如果找到了该实体
        if (it != mEntities.end()) {
            // 获取该实体的父节点
            XJNode *parent = it->second->XJGetParent();
            if (parent) {
                // 从父节点的子节点列表中移除该实体
                parent->XJRemoveChild(it->second.get());
            }

            // 从实体列表中移除该实体
            mEntities.erase(it);
        }
    }

     // DestroyAllEntity：销毁场景中的所有实体
    void XJScene::DestroyAllEntity() 
    {
        // 清空 ECS 注册表中的所有实体
        mEcsRegistry.clear();

        // 清空实体列表
        mEntities.clear();
    }

    // GetEntity：根据实体 ID 获取对应的实体对象
    XJEntity *XJScene::XJGetEntities(entt::entity enttEntity)
    {
        // 如果实体存在于列表中，返回该实体的指针
        if(mEntities.find(enttEntity) != mEntities.end())
        {
            return mEntities.at(enttEntity).get();
        }
         // 如果实体不存在，返回空指针
        return nullptr;
    }
}