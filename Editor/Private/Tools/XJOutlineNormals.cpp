#include "Tools/XJOutlineNormals.h"

#include <glm/geometric.hpp>

#include <cstring>
#include <unordered_map>
#include <cmath>

namespace XJ
{
    namespace
    {
        struct PositionKey//位置哈希键
        {
            uint32_t X = 0;
            uint32_t Y = 0;
            uint32_t Z = 0;

            bool operator==(const PositionKey& other) const
            {
                return X == other.X &&
                       Y == other.Y &&
                       Z == other.Z;
            }
        };

        struct PositionKeyHash//哈希函数
        {
            std::size_t operator()(const PositionKey& key) const
            {
                std::size_t result = key.X;
                const auto combine = [&result](uint32_t value)
                {
                    result ^= std::size_t(value) +
                        0x9e3779b97f4a7c15ull +// 黄金比例常量
                        (result << 6) +
                        (result >> 2);
                };

                combine(key.Y);
                combine(key.Z);

                return result;
            }
        };

        uint32_t FloatBits(float value)//浮点转位模式
        {
            uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }

        PositionKey MakeKey(const glm::vec3& position)//构造位置键
        {
            PositionKey key;
            key.X = FloatBits(position.x);
            key.Y = FloatBits(position.y);
            key.Z = FloatBits(position.z);
            return key;
        }
    }

    std::vector<glm::vec3> BuildSmoothOutlineNormals(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices)
    {
        //准备输出、提前返回
        std::vector <glm::vec3> result(positions.size());

        if(positions.empty())
            return result;

        std::unordered_map<PositionKey, glm::dvec3, PositionKeyHash> accumulated;//累加器

        const std::size_t triangleCount = indices.empty() ? positions.size() / 3 : indices.size() / 3;//确定三角形数量（支持索引/非索引两种输入）

        for(std::size_t triangle = 0; triangle < triangleCount; ++triangle)//遍历每个三角形，累加面积加权法线
        {
            uint32_t i0 = 0;
            uint32_t i1 = 0;
            uint32_t i2 = 0;

            if (indices.empty())
            {
                i0 = uint32_t(triangle * 3);
                i1 = uint32_t(triangle * 3 + 1);
                i2 = uint32_t(triangle * 3 + 2);
            }
            else
            {
                i0 = indices[triangle * 3];
                i1 = indices[triangle * 3 + 1];
                i2 = indices[triangle * 3 + 2];
            }

            if (i0 >= positions.size() ||
                i1 >= positions.size() ||
                i2 >= positions.size())
            {
                continue;// 越界保护：坏索引直接跳过
            }

            const glm::dvec3 a(positions[i0]);
            const glm::dvec3 b(positions[i1]);
            const glm::dvec3 c(positions[i2]);

            const glm::dvec3 normal = glm::cross(b - a, c - a);

            // 未归一化的叉积天然带有面积权重。
            // 退化三角形不贡献法线。
            const double lengthSquared = glm::dot(normal, normal);

            if (!(lengthSquared > 0.0))
                continue;

            accumulated[MakeKey(positions[i0])] += normal;
            accumulated[MakeKey(positions[i1])] += normal;
            accumulated[MakeKey(positions[i2])] += normal;
        }

        for (std::size_t i = 0; i < positions.size(); ++i)//第二遍，归一化输出
        {
            const auto found =  accumulated.find(MakeKey(positions[i]));

            if (found == accumulated.end())
                continue;

            const glm::dvec3& value = found->second;
            const double lengthSquared = glm::dot(value, value);

            if (!(lengthSquared > 0.0))
                continue;

            result[i] = glm::vec3(value / std::sqrt(lengthSquared));
        }

        return result;

    }
}