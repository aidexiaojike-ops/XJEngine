#ifndef XJ_MATERIAL_COMPONENT_H
#define XJ_MATERIAL_COMPONENT_H

#include "Render/Resource/XJMesh.h"
#include "Render/Resource/XJMaterial.h"
#include "ECS/XJComponent.h"

#include <memory>
#include <vector>


namespace XJ
{
    template<typename T>
    class XJMaterialComponent : public XJComponent
    {
        public:
            struct MeshMaterialSlot
            {
                std::shared_ptr<XJMesh> Mesh;
                std::shared_ptr<T> Material;
            };
            void AddMesh(std::shared_ptr<XJMesh> mesh, std::shared_ptr<T> material = nullptr)//添加网格
            {
                if(!mesh)
                {
                    return;
                }
                // 同一个 mesh 只保留一个 slot。再次添加时覆盖 material，避免重复绘制。
                for (auto& slot : mSlots)
                {
                    if (slot.Mesh == mesh)
                    {
                        slot.Material = std::move(material);
                        return;
                    }
                }

                mSlots.push_back({std::move(mesh), std::move(material)});
            };

            bool RemoveMesh(const std::shared_ptr<XJMesh>& mesh)
            {
                if (!mesh)
                    return false;

                for (auto it = mSlots.begin(); it != mSlots.end(); ++it)
                {
                    if (it->Mesh == mesh)
                    {
                        mSlots.erase(it);
                        return true;
                    }
                }

                return false;
            }

            bool RemoveMesh(uint32_t index)
            {
                if (index >= mSlots.size())
                    return false;

                mSlots.erase(mSlots.begin() + index);
                return true;
            }

            void ClearMeshes()
            {
                mSlots.clear();
            }

            uint32_t XJGetMaterialCount()const
            {
                uint32_t count = 0;

                // 返回实际非空材质数量。若同一材质绑定多个 mesh，这里按 slot 计数。
                for (const auto& slot : mSlots)
                {
                    if (slot.Material)
                        ++count;
                }

                return count;
            }//获取材质数量

            uint32_t XJGetMeshCount() const
            {
                return static_cast<uint32_t>(mSlots.size());
            }

            const std::vector<MeshMaterialSlot>& XJGetSlots() const
            {
                return mSlots;
            }

            XJMesh* XJGetMesh(uint32_t index) const
            {
                if (index < mSlots.size() && mSlots[index].Mesh)
                    return mSlots[index].Mesh.get();

                return nullptr;
            }//获取mesh指针

            std::shared_ptr<XJMesh> XJGetMeshShared(uint32_t index) const
            {
                if (index < mSlots.size())
                    return mSlots[index].Mesh;

                return nullptr;
            }

            T* XJGetMaterial(uint32_t index) const
            {
                if (index < mSlots.size() && mSlots[index].Material)
                    return mSlots[index].Material.get();

                return nullptr;
            }

            std::shared_ptr<T> XJGetMaterialShared(uint32_t index) const
            {
                if (index < mSlots.size())
                    return mSlots[index].Material;

                return nullptr;
            }//获取材质列表


        private:
             // 稳定顺序：渲染按添加顺序遍历。后续如果透明物体需要排序，可以在 render item 阶段排序。
            std::vector<MeshMaterialSlot> mSlots;

    };
    
  
    
}


#endif