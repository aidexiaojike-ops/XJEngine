#include "Tools/XJOutlineNormals.h"

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace
{
    using namespace XJ;

    void Check(bool value, std::string_view message)
    {
        if (!value)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    bool Near(
        const glm::vec3& a,
        const glm::vec3& b,
        float tolerance = 1.0e-4f)
    {
        return glm::length(a - b) <= tolerance;
    }

    void TestFlatQuad()
    {
        const std::vector<glm::vec3> positions = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}
        };

        const std::vector<uint32_t> indices = {
            0, 1, 2,
            0, 2, 3
        };

        const auto normals =
            BuildSmoothOutlineNormals(
                positions,
                indices);

        Check(
            normals.size() == positions.size(),
            "Normal count matches vertex count");

        for (const auto& normal : normals)
        {
            Check(
                Near(normal, {0.0f, 0.0f, 1.0f}),
                "Flat quad normal points +Z");
        }
    }

    void TestWeldedFold()
    {
        // 两个互相垂直的三角形共享一条边。
        // 共享位置的顶点被拆成两份，用于验证焊接。
        const std::vector<glm::vec3> positions = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},

            {0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}
        };

        const std::vector<uint32_t> indices = {
            0, 1, 2,
            3, 4, 5
        };

        const auto normals =
            BuildSmoothOutlineNormals(
                positions,
                indices);

        const glm::vec3 expected =
            glm::normalize(
                glm::vec3(1.0f, 0.0f, 1.0f));

        Check(
            Near(normals[0], expected),
            "Shared origin is welded");

        Check(
            Near(normals[3], expected),
            "Duplicated origin receives same normal");

        Check(
            Near(normals[2], expected),
            "Shared edge vertex is welded");

        Check(
            Near(normals[4], expected),
            "Duplicated edge vertex receives same normal");

        Check(
            Near(normals[1], {0.0f, 0.0f, 1.0f}),
            "Unshared vertex keeps its face normal");
    }

    void TestDegenerateAndInvalid()
    {
        const std::vector<glm::vec3> degenerate = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {2.0f, 0.0f, 0.0f}
        };

        const auto zeroNormals =
            BuildSmoothOutlineNormals(
                degenerate,
                {0, 1, 2});

        for (const auto& normal : zeroNormals)
        {
            Check(
                near(normal, glm::vec3(0.0f), 1.0e-6f),
                "Degenerate triangle contributes no normal");
        }

        const std::vector<glm::vec3> triangle = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}
        };

        const auto invalidNormals =
            BuildSmoothOutlineNormals(
                triangle,
                {0, 1, 99});

        for (const auto& normal : invalidNormals)
        {
            Check(
                near(normal, glm::vec3(0.0f), 1.0e-6f),
                "Invalid index is ignored");
        }
    }

    void TestNonIndexedAndEmpty()
    {
        const std::vector<glm::vec3> triangle = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}
        };

        const auto normals =
            BuildSmoothOutlineNormals(triangle, {});

        Check(
            normals.size() == 3,
            "Non-indexed input keeps vertex count");

        for (const auto& normal : normals)
        {
            Check(
                Near(normal, {0.0f, 0.0f, 1.0f}),
                "Non-indexed triangle normal");
        }

        const auto empty =
            BuildSmoothOutlineNormals({}, {});

        Check(
            empty.empty(),
            "Empty input returns empty normals");
    }
}

int main()
{
    TestFlatQuad();
    TestWeldedFold();
    TestDegenerateAndInvalid();
    TestNonIndexedAndEmpty();

    spdlog::info("XJOutlineNormalsTests passed.");
    return EXIT_SUCCESS;
}
