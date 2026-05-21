#include "Asset/XJAssetManager.h"

namespace XJ
{
    std::unordered_map<XJAssetHandle, std::shared_ptr<XJAsset>> XJAssetManager::mAssets;

    void XJAssetManager::XJLoadAsset(const std::shared_ptr<XJAsset>& asset)
    {

    }
}