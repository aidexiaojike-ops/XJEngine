#ifndef XJ_SCENE_H
#define XJ_SCENE_H

// #include "ECS/XJNode.h"
#include "ECS/XJUUID.h"
// #include <entt/entt.hpp> 
#include <entt/entity/registry.hpp> // 直接包含具体头文件

namespace XJ
{
    class XJNode;
    class XJEntity;

    class XJScene
    {
        private:
            /* data */
            std::string mName;
            entt::registry mEcsRegistry;

            std::unordered_map<entt::entity, std::shared_ptr<XJEntity>> mEntities;
            std::shared_ptr<XJNode> mRootNode;

            friend class XJEntity;
            
        public:
            entt::registry &XJGetEcsRegistry() {return mEcsRegistry;}
            XJEntity *XJGetEntities(entt::entity enttEntity);
            XJNode *XJGetRootNode() const {return mRootNode.get();}

            XJScene();
            ~XJScene();

            XJEntity* CreateEntity(const std::string &name = "");
            XJEntity* CreateEntityWithUUID(const XJUUID &id, const std::string &name = "");
            void DestroyEntity(const XJEntity *entity);
            void DestroyAllEntity();


    };
    
    
    
}

#endif  