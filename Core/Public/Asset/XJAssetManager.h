#ifndef XJ_ASSET_MANAGER_H
#define XJ_ASSET_MANAGER_H

#include "Asset/XJAsset.h"

#include <unordered_map>
#include <memory>

namespace XJ
{
    class XJAssetManager
    {
        public:
            template<typename T>
            static std::shared_ptr<T> XJGetAsset(XJAssetHandle handle)
            {
                // 这里可以根据路径和类型来加载资产，并缓存它们以避免重复加载
                // 例如，可以使用一个 unordered_map 来存储已经加载的资产
                // 下面是一个简单的示例，实际实现可能需要更复杂的逻辑来处理不同类型的资产

                auto it = mAssets.find(handle);
                
                if (it != mAssets.end())
                {
                    return std::static_pointer_cast<T>(it->second);
                }

                // 如果资产未被缓存，则加载它
                std::shared_ptr<T> asset = std::make_shared<T>();
                asset->mHandle = handle;
                mAssets[handle] = asset;

                return asset;
            }
            static void XJLoadAsset(const std::shared_ptr<XJAsset>& asset);
        private:
            static std::unordered_map<XJAssetHandle, std::shared_ptr<XJAsset>> mAssets;

          
    };
    
}


#endif