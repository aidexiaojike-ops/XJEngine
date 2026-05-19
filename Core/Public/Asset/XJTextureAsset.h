#ifndef XJ_TEXTURE_ASSET_H
#define XJ_TEXTURE_ASSET_H

#include "Asset/XJAsset.h"
#include <vector>
#include <cstdint>


namespace XJ
{

    class XJTextureAsset  : public XJAsset
    {
        public:
        //贴图宽高
            uint32_t Width = 0;
            uint32_t Height = 0;

            std::vector<uint8_t> Pixels;//像素数据，通常是 RGBA 格式，每个像素占 4 字节（8 位），根据需要也可以使用其他格式
    };

}

#endif
