//添加所有资产
#ifndef XJ_ASSET_REGISTRY_SCANNER_H
#define XJ_ASSET_REGISTRY_SCANNER_H

#include "Asset/XJAsset.h"

#include <filesystem>


namespace XJ
{
    class XJAssetRegistry;

    class XJAssetRegistryScanner
    {
        public:
            static int ScanResourceAssets(XJAssetRegistry& registry, const std::filesystem::path& resourceRoot);//扫描资源目录，注册所有资产到注册表中。返回新注册的资产数量
        
            static XJAssetType GetAssetTypeFromExtension(const std::filesystem::path& path);//根据文件扩展名判断资产类型
            static XJAssetHandle GenerateStableHandle(const std::filesystem::path& path, XJAssetType type);//根据文件路径和类型生成一个稳定的 handle，确保同一文件每次扫描得到的 handle 都相同
            static std::string GetAssetNameFromPath(const std::filesystem::path& path);//根据文件路径提取资产名称，通常是去掉扩展名的文件名部分
        private:
    };
    
}


#endif