#ifndef XJ_SCENE_ASSET_SERIALIZER_H
#define XJ_SCENE_ASSET_SERIALIZER_H

#include "Asset/XJSceneAsset.h"
#include <filesystem>
#include <memory>

//.xjscene 文件读写。只负责数据，不创建 ECS entity。
namespace XJ
{
    class XJScene;
    class XJEntity;

    class XJSceneAssetSerializer
    {
        public:
           
            static bool SaveToFile(const XJSceneAsset& sceneAsset, const std::filesystem::path& path); //将一个 XJSceneAsset（场景资产数据）写入磁盘文件（.xjscene）
            static std::shared_ptr<XJSceneAsset> LoadFromFile(const std::filesystem::path& path); //从磁盘读取 .xjscene 文件，返回一个 XJSceneAsset 对象

            
            static std::shared_ptr<XJSceneAsset> BuildFromScene(const XJScene& scene);//从 运行时场景对象（XJScene） 构建一个 XJSceneAsset（用于导出场景）
            
            static XJSceneEntityData BuildEntityData(const XJEntity& entity);//从 单个运行时实体（XJEntity） 构建 XJSceneEntityData（序列化表示）
    };


}
#endif