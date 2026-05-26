#ifndef XJ_SCENE_ASSET_COMPONENTS_H
#define XJ_SCENE_ASSET_COMPONENTS_H

#include "ECS/XJComponent.h"
#include "Asset/XJAssetRef.h"
#include "ECS/XJUUID.h"     
#include <vector>            
//将 资产引用（XJAssetRef） 附着到运行时实体上，使实体在游戏或编辑器中能够访问外部加载的资源（场景、网格、材质）
//运行时 ECS 里保存“这个实体来自哪个资产引用”
namespace XJ
{
    class XJSceneAssetRefComponent : public XJComponent
    {
        public:
            XJAssetRef SourceScene;//引用一个 .xjscene 场景资产
            XJUUID  SourceEntity = 0;//源场景中实体的 UUID
    };

    class XJMeshAssetRefComponent : public XJComponent
    {
        public:
            XJAssetRef Mesh;
    };

    class XJMaterialAssetRefComponent : public XJComponent
    {
    public:
        std::vector<XJAssetRef> Materials;
    };
}

#endif