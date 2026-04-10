#ifndef XJ_ENTITY_H
#define XJ_ENTITY_H

#include "ECS/XJNode.h"
#include "ECS/XJScene.h"

namespace XJ
{
    class XJEntity : public XJNode
    {
        private:
            /* data */
            entt::entity mEcsEntity;
            XJScene *mScene;
          
              
        public:
            XJEntity(entt::entity ecsEntity, XJScene *scene) : mEcsEntity(ecsEntity), mScene(scene) {}
            
            ~XJEntity() override = default;

           
            static bool IsValid(XJEntity *entity)
            {
                return entity && entity->IsValid();
            }
            template<typename T>//判断是否有这个组件
            static bool HasComponent(XJEntity *entity)
            {
                return IsValid(entity) && entity -> HasComponent<T>();
            }


            bool operator==(const XJEntity& other) const
            {
                return mEcsEntity == other.mEcsEntity && mScene == other.mScene;
            }
            bool operator!=(const XJEntity& other) const
            {
                return !(*this == other);
            }

            bool IsValid() const { return mScene && mScene->mEcsRegistry.valid(mEcsEntity); }//是否是空的
            entt::entity GetEcsEntity() const { return mEcsEntity; }

            template<typename T, typename... Args>//添加组件
            T& AddComponent(Args &&...args)
            {
                T &component = mScene->mEcsRegistry.emplace<T>(mEcsEntity, std::forward<Args>(args)...);
                component.SetOwner(this);//设置组件的所有者为当前实体
                return component;
            }

            template<typename T>
            bool HasComponent()//是否包含组件
            {
                return mScene->mEcsRegistry.any_of<T>(mEcsEntity);
            }

            template<typename... T>
            bool HasAnyComponent() //其中一个包含
            {
                return mScene->mEcsRegistry.any_of<T...>(mEcsEntity);
            }

            template<typename... T>
            bool HasAllComponent()//全部包含
            {
                return mScene->mEcsRegistry.all_of<T...>(mEcsEntity);
            }

            template<typename T>
            T& GetComponent()//获取组件
            {
                assert(HasComponent<T>() && "Entity does not have component!");
                return mScene->mEcsRegistry.get<T>(mEcsEntity);
            }

            template<typename T>
            void RemoveComponent()//移除组件
            {
                assert(HasComponent<T>() && "Entity does not have component!");
                mScene->mEcsRegistry.remove<T>(mEcsEntity);
            }

          
    };
   
    
}


#endif