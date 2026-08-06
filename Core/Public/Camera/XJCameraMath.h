#ifndef XJ_CAMERA_MATH_H
#define XJ_CAMERA_MATH_H

#include "Edit/Mathinclude.h"

#include <cmath>

namespace XJ
{
    namespace CameraMath
    {
        inline constexpr float MAX_PITCH_DEGREES = 89.0f;
        inline constexpr float VECTOR_EPSILON = 0.000001f;
        inline const glm::vec3 WORLD_UP{0.0f, 1.0f, 0.0f};

        inline float ClampPitch(float pitchDegrees)
        {
            return glm::clamp(pitchDegrees, -MAX_PITCH_DEGREES, MAX_PITCH_DEGREES);
        }

        inline glm::vec3 BuildForwardFromYawPitch(float yawDegrees, float pitchDegrees)
        {
            const float yaw = glm::radians(yawDegrees);
            const float pitch = glm::radians(ClampPitch(pitchDegrees));

            glm::vec3 forward{};
            forward.x = std::cos(yaw) * std::cos(pitch);
            forward.y = std::sin(pitch);
            forward.z = std::sin(yaw) * std::cos(pitch);

            if (glm::dot(forward, forward) <= VECTOR_EPSILON)
                return glm::vec3{1.0f, 0.0f, 0.0f};

            return glm::normalize(forward);
        }

        inline glm::vec3 BuildSafeUp(const glm::vec3& forward)
        {
            if (std::abs(glm::dot(forward, WORLD_UP)) > 0.99f)
                return glm::vec3{0.0f, 0.0f, 1.0f};

            return WORLD_UP;
        }

        inline glm::vec3 BuildRightFromForward(const glm::vec3& forward)
        {
            glm::vec3 right = glm::cross(forward, WORLD_UP);

            if (glm::dot(right, right) <= VECTOR_EPSILON)
                return glm::vec3{1.0f, 0.0f, 0.0f};

            return glm::normalize(right);
        }
    }
}

#endif