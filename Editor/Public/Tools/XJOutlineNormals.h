#ifndef XJ_OUTLINE_NORMALS_H
#define XJ_OUTLINE_NORMALS_H

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace XJ
{
    // 生成编辑器轮廓使用的平滑法线。
    //
    // 输入允许包含按面拆分的重复顶点：位置完全相同的顶点会先焊接，
    // 累加面积加权法线后再写回原顶点。索引布局保持不变，
    // 因此原 Mesh 的 submesh FirstIndex/IndexCount 仍然有效。
    //
    // 返回数组长度始终等于 positions.size()。
    std::vector<glm::vec3> BuildSmoothOutlineNormals(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);
}

#endif