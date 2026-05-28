#ifndef XJ_SCENE_H
#define XJ_SCENE_H

// #include "ECS/XJNode.h"
#include "ECS/XJUUID.h"
// #include <entt/entt.hpp> 
#include <entt/entity/registry.hpp> 
/**
 * @file XJScene.h
 * @brief 场景类，管理场景中所有的实体与节点，基于 EnTT 的 ECS 注册表。
 * 
 * 场景（XJScene）是整个 ECS 世界的容器，内部持有一个 entt::registry，
 * 并通过 XJEntity 的封装，提供创建、销毁实体等高层接口。同时场景也
 * 维护一个根节点（XJNode），用于构建场景树结构。
 */


namespace XJ
{
    class XJNode;
    class XJEntity;

    class XJScene
    {
        private:
            /* data */
            std::string mName; ///< 场景名称
            entt::registry mEcsRegistry; ///< EnTT ECS 注册表，存储所有组件

            std::unordered_map<entt::entity, std::shared_ptr<XJEntity>> mEntities;
            std::shared_ptr<XJNode> mRootNode;  ///< 场景根节点，用于构建场景层级树

            friend class XJEntity;///< 允许 XJEntity 访问场景内部成员
            
        public:
            entt::registry &XJGetEcsRegistry() {return mEcsRegistry;}//获取注册表
            XJEntity *XJGetEntities(entt::entity enttEntity) const;
            XJNode *XJGetRootNode() const {return mRootNode.get();}//获取场景根节点

            XJScene();//初始化场景
            ~XJScene();

            XJEntity* CreateEntity(const std::string &name = "");//在场景中创建一个新实体
            XJEntity* CreateEntityWithUUID(const XJUUID &id, const std::string &name = "");
            void DestroyEntity(const XJEntity *entity);//销毁指定的实体。
            void DestroyAllEntity();//销毁场景中所有的实体。

            const std::unordered_map<entt::entity, std::shared_ptr<XJEntity>>& GetEntities() const;
    };
    
    
    
}

#endif  