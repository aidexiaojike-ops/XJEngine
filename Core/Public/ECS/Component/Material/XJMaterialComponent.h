#ifndef XJ_MATERIAL_COMPONENT_H
#define XJ_MATERIAL_COMPONENT_H

#include "Render/Resource/XJMesh.h"
#include "Render/Resource/XJMaterial.h"
#include "ECS/XJComponent.h"
#include <memory>

namespace XJ
{
    template<typename T>
    class XJMaterialComponent : public XJComponent
    {
        private:
            // 组件持有 Mesh，避免实例化上下文或缓存释放后留下悬垂指针。
            std::vector<std::shared_ptr<XJMesh>> mMeshList;
            
            // 组件持有 Material，避免工厂改成 weak cache 后材质被提前释放。
            std::vector<std::shared_ptr<T>> mMaterialOwners;
            
            // 仍用裸指针作为分组 key；对象所有权由 mMaterialOwners 持有。
            std::unordered_map<T*, std::vector<uint32_t>> mMeshMaterials;
        public:
            void AddMesh(std::shared_ptr<XJMesh> mesh, std::shared_ptr<T> material = nullptr)//添加网格
            {
                if(!mesh)
                {
                    return;
                }
                uint32_t meshIndex = static_cast<uint32_t>(mMeshList.size());
                // 保存 shared_ptr 所有权，保证组件存在期间 Mesh 不会释放。
                mMeshList.push_back(std::move(mesh));

                T* materialKey = material.get();

                 // 组件持有材质 shared_ptr，避免工厂改为 weak cache 后材质释放。
                 if (material)
                     mMaterialOwners.push_back(std::move(material));

                 mMeshMaterials[materialKey].push_back(meshIndex);
            };

            uint32_t XJGetMaterialCount()const
            {
                return static_cast<uint32_t>(mMeshMaterials.size());
            }//获取材质数量
            const std::unordered_map<T*, std::vector<uint32_t>> &XJGetMeshMaterials() const
            {
                return mMeshMaterials;
            }//获取材质列表
            XJMesh *XJGetMesh(uint32_t index)const 
            {
                if(index < mMeshList.size())
                {
                    return mMeshList[index].get();
                }
                return nullptr;
            }//获取mesh指针

            std::shared_ptr<XJMesh> XJGetMeshShared(uint32_t index) const
            {
                if(index < mMeshList.size())
                {
                    return mMeshList[index];
                }
                return nullptr;
            }

    };
    
  
    
}


#endif