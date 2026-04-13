#ifndef XJ_MATERIAL_COMPONENT_H
#define XJ_MATERIAL_COMPONENT_H

#include "Render/XJMesh.h"
#include "Render/XJMaterial.h"
#include "ECS/XJComponent.h"

namespace XJ
{
    template<typename T>
    class XJMaterialComponent : public XJComponent
    {
        private:
            /* data */
            std::vector<XJMesh*> mMeshList; 
            std::unordered_map<T*, std::vector<uint32_t>> mMeshMaterials;
        public:
         

            void AddMesh(XJMesh *mesh, T *material = nullptr)//添加网格
            {
                if(!mesh)
                {
                    return;
                }
                uint32_t meshIndex = mMeshList.size();
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
                return mMeshMaterials.size();
            }//获取材质数量
            const std::unordered_map<T*, std::vector<uint32_t>> &XJGetMeshMaterials() const
            {
                return mMeshMaterials;
            }//获取材质列表
            XJMesh *XJGetMesh(uint32_t index)const 
            {
                if(index < mMeshList.size())
                {
                    return mMeshList[index];
                }
                return nullptr;
            }//获取mesh指针

    };
    
  
    
}


#endif