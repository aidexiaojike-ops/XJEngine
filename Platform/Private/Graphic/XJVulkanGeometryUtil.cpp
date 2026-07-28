#include "Graphic/XJVulkanGeometryUtil.h"

#include <cmath>

namespace XJ
{
    namespace
    {
        constexpr float kGeometryEpsilon = 1e-6f;

        glm::vec3 TransformPosition(const glm::mat4& transform, const glm::vec3& position)
        {
            const glm::vec4 transformed = transform * glm::vec4(position, 1.0f);

            if (std::abs(transformed.w) > kGeometryEpsilon)
            {
                return glm::vec3(transformed) / transformed.w;
            }

            return glm::vec3(transformed);
        }

        glm::vec3 TransformNormal(const glm::mat3& normalMat, const glm::vec3& normal)
        {
            const glm::vec3 transformed = normalMat * normal;
            const float lengthSq = glm::dot(transformed, transformed);

            if (lengthSq > kGeometryEpsilon * kGeometryEpsilon)
            {
                return glm::normalize(transformed);
            }

            return normal;
        }

        glm::mat3 BuildNormalMatrix(const glm::mat4& relativeMat)
        {
            const glm::mat3 linearMat(relativeMat);
            const float determinant = glm::determinant(linearMat);

            if (std::abs(determinant) <= kGeometryEpsilon)
            {
                return glm::mat3(1.0f);
            }

            return glm::transpose(glm::inverse(linearMat));
        }
    }

    void XJVulkanGeometryUtil::CreateCube(
        float leftPlane,
        float rightPlane,
        float bottomPlane,
        float topPlane,
        float nearPlane,
        float farPlane,
        std::vector<XJVulkanVertex>& vertices,
        std::vector<uint32_t>& indices,
        bool bUseTextcoords,
        bool bUseNormals,
        const glm::mat4& relativeMat)
    {
        const glm::mat3 normalMat = BuildNormalMatrix(relativeMat);

        vertices =
        {
            // near, normal -Z
            { TransformPosition(relativeMat, glm::vec3(rightPlane, topPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, bottomPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, bottomPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, topPlane, nearPlane)) },
        
            // right, normal +X
            { TransformPosition(relativeMat, glm::vec3(rightPlane, topPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, bottomPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, bottomPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, topPlane, nearPlane)) },
        
            // top, normal +Y
            { TransformPosition(relativeMat, glm::vec3(rightPlane, topPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, topPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, topPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, topPlane, farPlane)) },
        
            // left, normal -X
            { TransformPosition(relativeMat, glm::vec3(leftPlane, topPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, bottomPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, bottomPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, topPlane, farPlane)) },
        
            // bottom, normal -Y
            { TransformPosition(relativeMat, glm::vec3(leftPlane, bottomPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, bottomPlane, nearPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, bottomPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, bottomPlane, farPlane)) },
        
            // far, normal +Z
            { TransformPosition(relativeMat, glm::vec3(rightPlane, bottomPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(rightPlane, topPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, topPlane, farPlane)) },
            { TransformPosition(relativeMat, glm::vec3(leftPlane, bottomPlane, farPlane)) }
        };

        if (bUseTextcoords)
        {
            for (uint32_t face = 0; face < 6; ++face)
            {
                const uint32_t base = face * 4;

                vertices[base + 0].texcoord0 = glm::vec2(1.0f, 0.0f);
                vertices[base + 1].texcoord0 = glm::vec2(0.0f, 0.0f);
                vertices[base + 2].texcoord0 = glm::vec2(0.0f, 1.0f);
                vertices[base + 3].texcoord0 = glm::vec2(1.0f, 1.0f);
            }
        }

        if (bUseNormals)
        {
            const glm::vec3 faceNormals[6] =
            {
                glm::vec3(0.0f, 0.0f, -1.0f),
                glm::vec3(1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(-1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, 1.0f)
            };

            for (uint32_t face = 0; face < 6; ++face)
            {
                const uint32_t base = face * 4;
                const glm::vec3 normal = TransformNormal(normalMat, faceNormals[face]);

                vertices[base + 0].normal = normal;
                vertices[base + 1].normal = normal;
                vertices[base + 2].normal = normal;
                vertices[base + 3].normal = normal;
            }
        }

        indices =
        {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        };
    }
}