#ifndef XJ_ASSET_REGISTRY_H
#define XJ_ASSET_REGISTRY_H

#include "Asset/XJAsset.h"
#include <filesystem>
#include <unordered_map>
#include <optional>
//资产数据库。负责 handle -> path/type/name。Scene 加载时靠它把 handle 找回真实资产。
//读取序列化，根据场景序列加载对应的资产
namespace XJ
{
    struct XJAssetMeta
    {
        XJAssetHandle Handle = 0;//资产的唯一数字标识
        XJAssetType Type = XJAssetType::None;//资产类型
        std::string Name;//资产的显示名称或逻辑名称
        std::filesystem::path SourcePath;//原始资产文件的路径
        std::filesystem::path ImportedPath;//导入后的引擎内部格式路径
    };

    class XJAssetRegistry
    {
        public:
            bool RegisterAsset(const XJAssetMeta& meta);//将一个新资产的元数据加入注册表，分配或记录其 handle
            bool RemoveAsset(XJAssetHandle handle);//从注册表中移除指定 handle 的资产

            bool Contains(XJAssetHandle handle) const;//检查某个 handle 是否存在于注册表中
            std::optional<XJAssetMeta> GetMeta(XJAssetHandle handle) const;//根据 handle 查询对应的元数据

            bool Save(const std::filesystem::path& path) const;//将整个注册表的元数据序列化到文件（例如保存为 JSON 或二进制），用于持久化
            bool Load(const std::filesystem::path& path);//从文件反序列化恢复注册表状态

            const std::unordered_map<XJAssetHandle, XJAssetMeta>& XJGetAllMetas() const { return mMetas; }//获取注册表中所有资产的元数据，供场景加载或编辑器 UI 使用
        private:
            std::unordered_map<XJAssetHandle, XJAssetMeta> mMetas;
    };
}



#endif