//组件type   用于添加到node里面
#ifndef XJ_EDITOR_COMPONENT_TYPES_H
#define XJ_EDITOR_COMPONENT_TYPES_H

namespace XJ
{
    enum class XJEditorComponentType
    {
        None = 0,
        Transform,
        Camera,
        MeshRenderer,
        SceneAssetRef,
    };
}

#endif