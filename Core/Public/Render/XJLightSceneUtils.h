#ifndef XJ_LIGHT_SCENE_UTILS_H
#define XJ_LIGHT_SCENE_UTILS_H

#include "Render/XJLightUbo.h"
#include "ECS/Component/XJLightComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJScene.h"
#include <cmath>

namespace XJ
{
    // 从场景收集灯光组件，填充 LightUbo（供 Lit 渲染系统每帧上传）。
    class XJLightSceneUtils
    {
        public:
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
                                glm::vec4(BuildForward(tc.rotation), lc.XJGetIntensity());
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
                            s.DirectionAngle = glm::vec4(BuildForward(tc.rotation), innerCos);
                            s.RangeOuter = glm::vec4(lc.XJGetRange(), outerCos, 0.0f, 0.0f);
                            break;
                        }
                    }
                });
            }

        private:
            // rotation 为欧拉角(度)：x=偏航(yaw), y=俯仰(pitch)，与 XJCameraMath 约定一致。
            static glm::vec3 BuildForward(const glm::vec3& rotationDegrees)
            {
                const float yaw = glm::radians(rotationDegrees.x);
                const float pitch = glm::radians(rotationDegrees.y);

                return glm::normalize(glm::vec3(
                    std::cos(yaw) * std::cos(pitch),
                    std::sin(pitch),
                    std::sin(yaw) * std::cos(pitch)));
            }
    };
}

#endif
