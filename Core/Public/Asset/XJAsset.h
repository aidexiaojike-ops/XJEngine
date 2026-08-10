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

            static constexpr XJAssetHandle InvalidHandle = 0;
            static constexpr XJAssetHandle RuntimeHandleBit = 1ull << 63;

            static bool IsRuntimeHandle(XJAssetHandle handle)
            {
                return (handle & RuntimeHandleBit) != 0;
            }

            static XJAssetHandle GenerateHandle()
            {
                // Runtime-only assets live in the high-bit range so they cannot collide with
                // stable registry handles generated from persisted asset paths.
                return NextRuntimeHandle().fetch_add(1);
            }

            static void ReserveGeneratedHandlesUpTo(XJAssetHandle handle)
            {
                if (!IsRuntimeHandle(handle))
                    return;

                const XJAssetHandle desired = handle + 1;
                XJAssetHandle current = NextRuntimeHandle().load();

                while (desired > current &&
                       !NextRuntimeHandle().compare_exchange_weak(current, desired))
                {
                }
            }

        private:
            static std::atomic<XJAssetHandle>& NextRuntimeHandle()
            {
                static std::atomic<XJAssetHandle> sNext{RuntimeHandleBit | 1ull};
                return sNext;
            }


    };
    
}


#endif

