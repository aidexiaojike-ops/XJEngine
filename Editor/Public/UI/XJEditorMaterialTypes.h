#ifndef XJ_EDITOR_MATERIAL_TYPES_H
#define XJ_EDITOR_MATERIAL_TYPES_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"

#include <variant>

namespace XJ
{
    enum class XJEditorMaterialParameterType//用于窗口的参数
    {
        None = 0,

        Float,
        Int,
        Bool,

        Vec2,
        Vec3,
        Vec4,

        Color3,
        Color4,

        Texture2D
    };

    using XJEditorMaterialParameterValue = std::variant<
        std::monostate,
        float,
        int,
        bool,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        XJAssetHandle>;
}

#endif