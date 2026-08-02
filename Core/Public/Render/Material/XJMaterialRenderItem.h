#ifndef XJ_MATERIAL_RENDER_ITEM_H
#define XJ_MATERIAL_RENDER_ITEM_H

#include "Edit/Mathinclude.h"
#include <memory>

namespace XJ
{
    class XJMaterial;
    class XJMesh;

    struct XJMaterialRenderItem
    {
        XJMaterial* Material = nullptr;
        // RenderItem 持有 shared_ptr，保证命令录制期间 Mesh 不会因为组件或缓存变化而释放。
        std::shared_ptr<XJMesh> Mesh;
        
        glm::mat4 ModelMatrix{1.0f};
    };
}

#endif