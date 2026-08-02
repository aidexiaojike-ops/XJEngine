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
            // 组件持有 Mesh 的 shared_ptr，避免场景实例化上下文析构后 Mesh 被释放。
            std::vector<std::shared_ptr<XJMesh>> mMeshList; 
            
            // Material 目前由 MaterialFactory 管理生命周期，这里先保留裸指针作为分组 key。
            std::unordered_map<T*, std::vector<uint32_t>> mMeshMaterials;
        public:
            void AddMesh(std::shared_ptr<XJMesh> mesh, T *material = nullptr)//添加网格
            {
                if(!mesh)
                {
                    return;
                }
                uint32_t meshIndex = static_cast<uint32_t>(mMeshList.size());
                // 保存 shared_ptr 所有权，保证组件存在期间 Mesh 不会释放。
                mMeshList.push_back(mesh);

                if(mMeshMaterials.find(material) != mMeshMaterials.end())
                {
                    mMeshMaterials[material].push_back(meshIndex);
                }
                else
                {
                    mMeshMaterials.insert({material, {meshIndex}});
                }
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