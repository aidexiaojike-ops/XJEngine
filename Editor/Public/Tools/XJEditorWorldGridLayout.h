#ifndef XJ_EDITOR_WORLD_GRID_LAYOUT_H
#define XJ_EDITOR_WORLD_GRID_LAYOUT_H

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace XJ
{
    enum class XJEditorGridLabelKind//网格标签类型
    {
        XAxis,
        ZAxis,
        Spacing
    };

    struct XJEditorGridLabel//网格标签
    {
        XJEditorGridLabelKind Kind = XJEditorGridLabelKind::Spacing;

        glm::vec3 Position{0.0f};
        std::string Text;
        // 世界空间中的文字高度。
        float Height = 0.1f;
        // 主轴短刻线的半长；Spacing 标签不绘制短刻线。
        float TickHalfLength = 0.0f;
    };

    struct XJEditorWorldGridLayout//世界网格布局
    {
        bool Valid = false;
        glm::vec3 Center{0.0f};//世界网格中心
        float Step = 1.0f;//世界网格步长
        // 每个方向中心两侧各 40 格。
        int HalfLineCount = 40;
        std::vector<XJEditorGridLabel> Labels;
    };

    class XJEditorWorldGridLayoutBuilder
    {
        public:
            // 返回不小于 requested 的 1/2/5 系列值。
            // 世界网格最小为 1 米。
            static double ChooseStep(double requested);

            static XJEditorWorldGridLayout Build(
                const glm::mat4& view,
                const glm::mat4& projection,
                unsigned int width,
                unsigned int height);
    };
}

#endif