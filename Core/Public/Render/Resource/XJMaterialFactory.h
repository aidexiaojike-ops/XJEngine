#ifndef XJ_MATERIAL_FACTORY_H
#define XJ_MATERIAL_FACTORY_H

#include "Render/Resource/XJMaterial.h"
#include "Asset/XJMaterialAsset.h"
#include <unordered_map>
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Render/Material/XJMaterialParameterBlockBuilder.h"

#include <mutex>
#include <typeindex>
#include <algorithm>

namespace XJ
{

    class XJAssetRegistry;

    //材质工厂
    class XJMaterialFactory
    {
        public:
            XJMaterialFactory(const XJMaterialFactory&) = delete;
            XJMaterialFactory &operator = (const XJMaterialFactory&) = delete;
            
            void SetAssetRegistry(XJAssetRegistry* registry) { std::scoped_lock lock(mMutex);
                                mAssetRegistry = registry; }

            std::shared_ptr<XJUnlitMaterial> CreateFromAsset(const XJMaterialAsset& asset,
                                    const std::shared_ptr<XJTexture>& defaultTex,
                                    const std::shared_ptr<XJSampler>& defaultSampler);

            static XJMaterialFactory* GetInstance()
            {
                // 函数局部静态避免跨 TU 静态初始化顺序问题。
                // 这里故意不在全局静态对象析构阶段主动清 GPU 资源，资源应在渲染上下文销毁前由场景/组件释放。
                static XJMaterialFactory instance;
                return &instance;
            }

            ~XJMaterialFactory() = default;


            std::shared_ptr<XJUnlitMaterial> CreateDefaultMaterial(
                                            const std::shared_ptr<XJTexture>& defaultTexture,
                                            const std::shared_ptr<XJSampler>& defaultSampler);


            template<typename T>
            size_t GetMaterialSize()
            {
                std::scoped_lock lock(mMutex);

                const std::type_index typeIndex = std::type_index(typeid(T));
                auto it = mMaterials.find(typeIndex);
                if(it == mMaterials.end())
                {
                    return 0;
                }

                auto& materialList = it->second;

                // 工厂只保存 weak_ptr；这里顺手清理已经被场景/组件释放的材质。
                materialList.erase
                (
                    std::remove_if(materialList.begin(), materialList.end(),
                        [](const std::weak_ptr<XJMaterial>& material)
                        {
                            return material.expired();
                        }),
                    materialList.end()
                );
                
                return materialList.size();  // 原: return mMaterials;
            }

            template<typename T>
            std::shared_ptr<T> CreateMaterial()//可以提供外界创建一个材质类型ID
            {
                auto mat = std::make_shared<T>();

                std::scoped_lock lock(mMutex);

                const std::type_index typeId = std::type_index(typeid(T));
                auto& materials = mMaterials[typeId];
                
               // 清理已经释放的材质，避免材质列表只增不减。
                materials.erase(
                    std::remove_if(
                        materials.begin(),
                        materials.end(),
                        [](const std::weak_ptr<XJMaterial>& material)
                        {
                            return material.expired();
                        }),
                    materials.end());

                const uint32_t index = static_cast<uint32_t>(materials.size());
                mat->mIndex = index;

                // 工厂不强持有材质，避免材质和 GPU 资源被单例拖到进程退出才析构。
                materials.push_back(mat);
                return mat;
            }
        
        private:
            XJMaterialFactory() = default;
            XJAssetRegistry* mAssetRegistry = nullptr;

            // 保护材质列表、纹理缓存和 asset registry 指针。
            mutable std::mutex mMutex;

            // 用 type_index 避免 entt hash 截断和理论碰撞。
            std::unordered_map<std::type_index, std::vector<std::weak_ptr<XJMaterial>>> mMaterials;

            // 纹理缓存本来就是 weak_ptr，保留弱引用，但访问必须加锁。
            std::unordered_map<XJAssetHandle, std::weak_ptr<XJTexture>> mTextureCache;
            std::shared_ptr<XJTexture> GetOrLoadTexture(XJAssetHandle handle, const std::shared_ptr<XJTexture>& fallback);

            void ApplyTextureBindings(XJUnlitMaterial& material, const XJMaterialAsset& asset, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler);
           
        } ;
    
}



#endif