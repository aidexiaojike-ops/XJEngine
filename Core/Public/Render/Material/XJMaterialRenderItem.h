#ifndef XJ_MATERIAL_RENDER_ITEM_H
#define XJ_MATERIAL_RENDER_ITEM_H

#include "Edit/Mathinclude.h"

namespace XJ
{
    class XJMaterial;
    class XJMesh;

    struct XJMaterialRenderItem
    {
        XJMaterial* Material = nullptr;
        XJMesh* Mesh = nullptr;
        glm::mat4 ModelMatrix{1.0f};
    };
}

#endif