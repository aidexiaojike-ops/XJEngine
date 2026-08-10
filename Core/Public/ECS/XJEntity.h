#ifndef XJ_ENTITY_H
#define XJ_ENTITY_H

#include "ECS/XJNode.h"
#include "ECS/XJScene.h"

#include <stdexcept>
#include <memory>
#include <utility>

namespace XJ
{
    class XJEntity : public XJNode
    {
        private:
            /* data */
            entt::entity mEcsEntity;
            XJScene *mScene;
            std::weak_ptr<void> mSceneLifetimeToken;

            XJScene* GetSceneChecked() const
            {
                if (!mScene || mSceneLifetimeToken.expired())
                    return nullptr;

                return mScene;
            }
           
               
        public:
            XJEntity(entt::entity ecsEntity, XJScene *scene)
                : mEcsEntity(ecsEntity), mScene(scene), mSceneLifetimeToken(scene ? scene->GetLifetimeToken() : std::weak_ptr<void>{}) {}
            
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

            bool IsValid() const
            {
                XJScene* scene = GetSceneChecked();
                return scene &&
                       scene->mEcsRegistry.valid(mEcsEntity) &&
                       scene->GetEntity(mEcsEntity) == this;
            }//是否是空的
            entt::entity GetEcsEntity() const { return mEcsEntity; }

            template<typename T, typename... Args>//添加组件
            T& AddComponent(Args &&...args)
            {
                if (!IsValid())
                    throw std::runtime_error("AddComponent failed: entity is invalid");

                // 重复添加组件时替换旧组件，避免 EnTT emplace 触发断言。
                XJScene* scene = GetSceneChecked();
                if (!scene)
                    throw std::runtime_error("AddComponent failed: entity scene is invalid");

                T &component = scene->mEcsRegistry.emplace_or_replace<T>(mEcsEntity, std::forward<Args>(args)...);
                component.SetOwner(this);//设置组件的所有者为当前实体
                return component;
            }

            template<typename T>
            bool HasComponent() const//是否包含组件
            {
                XJScene* scene = GetSceneChecked();
                return scene && IsValid() && scene->mEcsRegistry.any_of<T>(mEcsEntity);
            }

            template<typename... T>
            bool HasAnyComponent() //其中一个包含
            {
                XJScene* scene = GetSceneChecked();
                return scene && IsValid() && scene->mEcsRegistry.any_of<T...>(mEcsEntity);
            }

            template<typename... T>
            bool HasAllComponent()//全部包含
            {
                XJScene* scene = GetSceneChecked();
                return scene && IsValid() && scene->mEcsRegistry.all_of<T...>(mEcsEntity);
            }

        
            template<typename T>
            T& GetComponent() //获取组件
            {
                if (!HasComponent<T>())
                    throw std::runtime_error("GetComponent failed: entity does not have component");

                XJScene* scene = GetSceneChecked();
                if (!scene)
                    throw std::runtime_error("GetComponent failed: entity scene is invalid");

                return scene->mEcsRegistry.get<T>(mEcsEntity);
            }

            template<typename T>
            const T& GetComponent() const //获取组件
            {
                if (!HasComponent<T>())
                    throw std::runtime_error("GetComponent failed: entity does not have component");

                XJScene* scene = GetSceneChecked();
                if (!scene)
                    throw std::runtime_error("GetComponent failed: entity scene is invalid");

                return scene->mEcsRegistry.get<T>(mEcsEntity);
            }

            template<typename T>
            void RemoveComponent()//移除组件
            {
                if (!HasComponent<T>())
                    return;

                XJScene* scene = GetSceneChecked();
                if (!scene)
                    return;

                scene->mEcsRegistry.remove<T>(mEcsEntity);
            }

          
    };
   
    
}


#endif
