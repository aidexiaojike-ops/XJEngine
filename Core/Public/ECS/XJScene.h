#ifndef XJ_SCENE_H
#define XJ_SCENE_H

// #include "ECS/XJNode.h"
#include "ECS/XJUUID.h"
// #include <entt/entt.hpp> 
#include <entt/entity/registry.hpp> 
#include <entt/signal/sigh.hpp>

#include <memory>
#include <unordered_set>
#include <cstdint>
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
            std::unordered_map<XJUUID, entt::entity> mEntitiesByUUID;
            std::shared_ptr<XJNode> mRootNode;  ///< 场景根节点，用于构建场景层级树
            std::shared_ptr<void> mLifetimeToken;

            // view.each 遍历期间不要直接 destroy registry；先排队，遍历结束后 Flush。
            std::vector<entt::entity> mPendingDestroyEntities;
            std::unordered_set<entt::entity> mPendingDestroySet;

            void DestroyEntityImmediate(const XJEntity* entity);

            friend class XJEntity;///< 允许 XJEntity 访问场景内部成员
            entt::sigh<void(XJEntity&)> mBeforeEntityDestroy;///< 实体销毁前信号
            std::unordered_set<entt::entity> mDestroyingEntities;///< 正在销毁的实体集合，防止重复销毁

            uint32_t mScriptExecutionDepth = 0;///< 脚本执行深度计数器，防止递归调用导致的无限循环
            uint64_t mStructureRevision = 1;///< 场景结构修订号，每次结构变化时递增，用于脚本系统检测场景变化
            bool mDestroyingAll = false;///< 是否正在销毁所有实体，防止在销毁过程中再次触发销毁操作
            
        public:
            // 只允许外部读取 registry，不能直接 create/destroy。
            // 实体生命周期必须走 XJScene::CreateEntity / DestroyEntity，保证 mEcsRegistry、mEntities、Node 层级同步。
            const entt::registry& XJGetEcsRegistry() const { return mEcsRegistry; }//获取注册表
            std::weak_ptr<void> GetLifetimeToken() const { return mLifetimeToken; }

            XJEntity *GetEntity(entt::entity enttEntity) const;
            XJEntity* FindEntityByUUID(const XJUUID &id) const; //通过UUID查找实体 

            XJNode *XJGetRootNode() const {return mRootNode.get();}//获取场景根节点

            XJScene();//初始化场景
            ~XJScene();

            XJEntity* CreateEntity(const std::string &name = "");//在场景中创建一个新实体
            XJEntity* CreateEntityWithUUID(const XJUUID &id, const std::string &name = "");
            XJEntity* CreateEntityWithTransform(const std::string& name);//添加transform组件
            XJEntity* CreateEntityWithUUIDAndTransform(const XJUUID& id, const std::string& name);//添加一个带有transform组件的实体
            
            void DestroyEntity(const XJEntity *entity);//销毁指定的实体。
            void QueueDestroyEntity(const XJEntity *entity);//遍历 ECS view 时使用的延迟销毁接口。
            void FlushDestroyQueue();//在安全点批量执行延迟销毁。
            void DestroyAllEntity();//销毁场景中所有的实体。

            const std::unordered_map<entt::entity, std::shared_ptr<XJEntity>>& GetEntities() const;

            auto BeforeEntityDestroy() { return entt::sink{mBeforeEntityDestroy}; }
            uint64_t GetStructureRevision() const { return mStructureRevision; }

        // VM 和销毁通知期间保持 Owner 有效；实体删除在安全点处理。
        class ScriptExecutionScope
        {
        public:
            explicit ScriptExecutionScope(XJScene& scene) : mScene(scene)
            {
                ++mScene.mScriptExecutionDepth;
            }
            ~ScriptExecutionScope()
            {
                --mScene.mScriptExecutionDepth;
            }

            ScriptExecutionScope(const ScriptExecutionScope&) = delete;
            ScriptExecutionScope& operator=(const ScriptExecutionScope&) = delete;

        private:
            XJScene& mScene;
        };
    };
    
    
}

#endif  
