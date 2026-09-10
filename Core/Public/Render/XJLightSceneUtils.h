#ifndef XJ_LIGHT_SCENE_UTILS_H
#define XJ_LIGHT_SCENE_UTILS_H

#include "Render/XJLightUbo.h"
#include "ECS/Component/XJLightComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJScene.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace XJ
{
    // 从场景收集灯光组件，填充 LightUbo（供 Lit 渲染系统每帧上传）。
    class XJLightSceneUtils
    {
        public:
            // 灯光局部 +X 为照射方向，旋转顺序与 XJTransformComponent::UpdateModelMatrix 一致。
            static glm::vec3 BuildDirection(const XJTransformComponent& transform)
            {
                glm::mat4 rotation(1.0f);
                rotation = glm::rotate(rotation, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                rotation = glm::rotate(rotation, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                rotation = glm::rotate(rotation, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

                const glm::vec3 direction = glm::vec3(rotation * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
                return glm::dot(direction, direction) > 0.000001f
                    ? glm::normalize(direction)
                    : glm::vec3(1.0f, 0.0f, 0.0f);
            }

            static void BuildLightUboFromScene(const XJScene& scene, XJLightUbo& out)
            {
                out = {};

                const auto& reg = scene.XJGetEcsRegistry();
                auto view = reg.view<XJLightComponent, XJTransformComponent>();

                view.each([&](auto, const XJLightComponent& lc, const XJTransformComponent& tc)
                {
                    if (!lc.XJGetEnable())
                        return;

                    switch (lc.XJGetLightType())
                    {
                        case XJLightType::Directional:
                        {
                            out.Directional.DirectionIntensity =
                                glm::vec4(BuildDirection(tc), lc.XJGetIntensity());
                            out.Directional.ColorEnabled = glm::vec4(lc.XJGetColor(), 1.0f);
                            out.DirectionalCount = 1;
                            break;
                        }

                        case XJLightType::Point:
                        {
                            if (out.PointCount >= XJ_MAX_POINT_LIGHTS)
                                return;

                            auto& p = out.Points[out.PointCount++];
                            p.PositionIntensity = glm::vec4(tc.position, lc.XJGetIntensity());
                            p.ColorEnabled = glm::vec4(lc.XJGetColor(), 1.0f);
                            p.RangeData = glm::vec4(lc.XJGetRange(), 0.0f, 0.0f, 0.0f);
                            break;
                        }

                        case XJLightType::Spot:
                        {
                            if (out.SpotCount >= XJ_MAX_SPOT_LIGHTS)
                                return;

                            auto& s = out.Spots[out.SpotCount++];
                            s.PositionIntensity = glm::vec4(tc.position, lc.XJGetIntensity());
                            s.ColorEnabled = glm::vec4(lc.XJGetColor(), 1.0f);
                            // 组件保存半角度数，GPU 使用其余弦进行锥体判定。
                            const float innerCos = std::cos(glm::radians(lc.XJGetInnerAngleDegrees()));
                            const float outerCos = std::cos(glm::radians(lc.XJGetOuterAngleDegrees()));
                            s.DirectionAngle = glm::vec4(BuildDirection(tc), innerCos);
                            s.RangeOuter = glm::vec4(lc.XJGetRange(), outerCos, 0.0f, 0.0f);
                            break;
                        }
                    }
                });
            }

    };
}

#endif
