#ifndef XJ_MATERIAL_FACTORY_H
#define XJ_MATERIAL_FACTORY_H

#include "Render/Resource/XJMaterial.h"
#include "Asset/XJMaterialAsset.h"
#include <unordered_map>
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Render/Material/XJMaterialParameterBlockBuilder.h"

namespace XJ
{

    class XJAssetRegistry;

    //材质工厂
    class XJMaterialFactory
    {
        public:
            XJMaterialFactory(const XJMaterialFactory&) = delete;
            XJMaterialFactory &operator = (const XJMaterialFactory&) = delete;
            
            void SetAssetRegistry(XJAssetRegistry* registry) { mAssetRegistry = registry; }

            std::shared_ptr<XJUnlitMaterial> CreateFromAsset(const XJMaterialAsset& asset,
                                    const std::shared_ptr<XJTexture>& defaultTex,
                                    const std::shared_ptr<XJSampler>& defaultSampler);

            static XJMaterialFactory* GetInstance(){
                return &mMaterialFactory;
            }

            ~XJMaterialFactory()
            {
                mMaterials.clear();
            }

            std::shared_ptr<XJUnlitMaterial> CreateDefaultMaterial(
                                            const std::shared_ptr<XJTexture>& defaultTexture,
                                            const std::shared_ptr<XJSampler>& defaultSampler);


            template<typename T>
            size_t GetMaterialSize()
            {
                uint32_t typeId = entt::type_id<T>().hash();
                if(mMaterials.find(typeId) == mMaterials.end())
                {
                    return 0;
                }
                return mMaterials[typeId].size();  // 原: return mMaterials;
            }

            template<typename T>
            std::shared_ptr<T> CreateMaterial()//可以提供外界创建一个材质类型ID
            {
                auto mat = std::make_shared<T>();
                uint32_t typeId = entt::type_id<T>().hash();//entt 拿到 typeId
                
                uint32_t index = 0;
                if(mMaterials.find(typeId) == mMaterials.end())
                {
                    mMaterials.insert({ typeId, { mat }});
                } 
                else
                {
                    index = mMaterials[typeId].size();
                    mMaterials[typeId].push_back(mat);
                }
                mat->mIndex = index;//材质索引
                return mat;
            }
        
        private:
            XJMaterialFactory() = default;
            static XJMaterialFactory mMaterialFactory;
            XJAssetRegistry* mAssetRegistry = nullptr;
            std::unordered_map<uint32_t, std::vector<std::shared_ptr<XJMaterial>>> mMaterials;//所有材质放到这个数据结构里面
            
            std::unordered_map<XJAssetHandle, std::weak_ptr<XJTexture>> mTextureCache;
            std::shared_ptr<XJTexture> GetOrLoadTexture(XJAssetHandle handle, const std::shared_ptr<XJTexture>& fallback);

            void ApplyTextureBindings(XJUnlitMaterial& material, const XJMaterialAsset& asset, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler);

        } ;
    
}



#endif