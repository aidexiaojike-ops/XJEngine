#ifndef XJ_ASSET_H
#define XJ_ASSET_H


#include <string>
#include <atomic>
#include <filesystem>
#include <cstdint>


namespace XJ
{
    enum class XJAssetType//资产种类
    {
        None = 0,//change to None to avoid confusion with 0-based index of asset array
        
        Mesh,
        Texture,
        Material,
        Scene,
        Shader,
    };


    using XJAssetHandle = uint64_t;//资产的唯一标识

    class XJAsset//保存资产的句柄、类型、名字和文件路径
    {
        public:

            virtual ~XJAsset() = default;

        public:
            XJAssetHandle mHandle = 0;//文件句柄
            XJAssetType mType = XJAssetType::None;//文件类型
            std::string mName;//文件名字
            std::filesystem::path mPath;//文件路径

            static XJAssetHandle GenerateHandle()
            {
                static std::atomic<XJAssetHandle> sNext{1};
                return sNext.fetch_add(1);
            }


    };
    
}


#endif

