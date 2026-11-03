#ifndef XJ_TRANSFORM_GIZMO_GEOMETRY_H
#define XJ_TRANSFORM_GIZMO_GEOMETRY_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <vector>

namespace XJ
{
    enum class XJTransformGizmoHandle : uint8_t//变换控件手柄
    {
        None = 0,

        MoveX,
        MoveY,
        MoveZ,

        RotateX,
        RotateY,
        RotateZ,

        ScaleX,
        ScaleY,
        ScaleZ
    };

    enum class XJTransformGizmoOperation : uint8_t//变换控件操作
    {
        None,
        Move,
        Rotate,
        Scale
    };

    enum class XJTransformGizmoAxis : uint8_t//变换控件轴
    {
        X,
        Y,
        Z
    };

    struct XJTransformGizmoMetrics//变换控件度量
    {
        static constexpr float ArrowStart = 0.12f;//移动箭头轴杆从距原点 0.12 处开始，避免和中心重叠
        static constexpr float ArrowShaftEnd = 0.75f;//移动箭头轴杆末端位置
        static constexpr float ArrowEnd = 1.00f;//移动箭头尖端位置，即移动轴总长度约 1

        static constexpr float ArrowShaftRadius = 0.018f;//移动箭头轴杆半径
        static constexpr float ArrowHeadRadius = 0.065f;//移动箭头头部半径/底面半径

        static constexpr float RingRadius = 1.10f;//旋转环的环半径，即环中心线到原点的距离
        static constexpr float RingTubeRadius = 0.018f;//旋转环的管半径，类似圆环截面的粗细

        static constexpr float ScaleStemStart = 1.25f;//缩放柄杆起点距原点距离
        static constexpr float ScaleStemEnd = 1.43f;//缩放柄杆终点距原点距离
        static constexpr float ScaleStemRadius = 0.012f;//缩放柄杆半径

        static constexpr float ScaleBoxCenter = 1.50f;//缩放手柄方块中心距原点距离
        static constexpr float ScaleBoxHalfSize = 0.07f;//缩放手柄方块半边长，因此方块外端约到 1.50 + 0.07 = 1.57

        // 必须覆盖全部几何。
        static constexpr float OuterExtent = 1.60f;

        // Gizmo 的近似屏幕半径。
        static constexpr float ScreenRadiusPixels = 110.0f;
    };

    struct XJTransformGizmoMeshData//变换控件网格数据
    {
        XJTransformGizmoHandle Handle = XJTransformGizmoHandle::None;

        // 非索引三角形列表，每三个顶点为一个三角形。
        std::vector<glm::vec3> Positions;
    };

    struct XJTransformGizmoFrame//变换控件帧
    {
        bool Valid = false;

        // T * R * uniformScale
        glm::mat4 Model{1.0f};

        float WorldScale = 0.0f;
    };


    XJTransformGizmoOperation GetGizmoOperation(XJTransformGizmoHandle handle);//根据手柄获取操作类型
    XJTransformGizmoAxis GetGizmoAxis(XJTransformGizmoHandle handle);//根据手柄获取轴类型
    glm::vec4 GetGizmoAxisColor(XJTransformGizmoAxis axis);//根据轴类型获取颜色

    // 返回 9 个部件。
    // 顺序为：圆环、移动箭头、缩放方块。
    std::vector<XJTransformGizmoMeshData>
        BuildTransformGizmoGeometry();

    // 纯数学计算，不读取 ECS 或 GPU。
    XJTransformGizmoFrame BuildTransformGizmoFrame(
        const glm::dvec3& worldPosition,
        const glm::dquat& orientation,
        const glm::mat4& view,
        const glm::mat4& projection,
        uint32_t viewportWidth,
        uint32_t viewportHeight);

}
#endif