#ifndef XJ_UNLIT_MATERIAL_RENDER_ITEM_BUILDER_H
#define XJ_UNLIT_MATERIAL_RENDER_ITEM_BUILDER_H

#include "Render/Material/XJMaterialRenderItem.h"

#include <vector>

namespace XJ
{
    class XJScene;

    class XJUnlitMaterialRenderItemBuilder
    {
        public:
            static std::vector<XJMaterialRenderItem> Build(XJScene& scene);

             // 复用调用方传入的 vector，避免每帧返回值构造导致额外堆分配。
            static void Build(XJScene& scene, std::vector<XJMaterialRenderItem>& outItems);
    };
}

#endif