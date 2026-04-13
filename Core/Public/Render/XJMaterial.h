#ifndef XJ_MATERIAL_H
#define XJ_MATERIAL_H

#include "Edit/Mathinclude.h"
#include "Render/XJSampler.h"
#include "XJTexture.h"
#include "entt/core/type_info.hpp"

namespace XJ
{
    struct TextureView
    {
        XJTexture *texture = nullptr;//纹理
        XJSampler *sampler = nullptr;//采样
        bool bEnable = true;//是否启用
        glm::vec2 uvTranslation{0.f, 0.f};//uv位移
        float uvRotation{0.f};//uv旋转
        glm::vec2 uvScale{1.0f ,1.0f};//uv缩放


        bool IsValid() const
        {
            return bEnable && texture != nullptr && sampler !=nullptr;
        }
    };

    class XJMaterial
    {
        private:
            /* data */
            int32_t mIndex = -1;//材质索引   DescriptorSet
            std::unordered_map<uint32_t, TextureView> mTextures;
            friend class XJMaterialFactory;
        public:
           
    };

 //材质工厂
    class XJMaterialFactory
    {
        public:
            XJMaterialFactory(const XJMaterialFactory&) = delete;
            XJMaterialFactory &operator = (const XJMaterialFactory&) = delete;

            static XJMaterialFactory* GetInstance(){
                return &s_MaterialFactory;
            }

            ~XJMaterialFactory()
            {
                mMaterials.clear();
            }

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
            T* CreateMaterial()//可以提供外界创建一个材质类型ID
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
                return mat.get();
            }
        
        private:
            XJMaterialFactory() = default;
    
            static XJMaterialFactory s_MaterialFactory;
    
            std::unordered_map<uint32_t, std::vector<std::shared_ptr<XJMaterial>>> mMaterials;//所有材质放到这个数据结构里面
    } ;
    
    
}



#endif