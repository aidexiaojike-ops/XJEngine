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
    };
}

#endif