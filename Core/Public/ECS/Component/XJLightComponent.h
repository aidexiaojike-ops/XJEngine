#ifndef XJ_LIGHT_COMPONENT_H
#define XJ_LIGHT_COMPONENT_H

#include "ECS/XJComponent.h"
#include "Edit/Mathinclude.h"
#include <algorithm>

namespace XJ
{
    // 灯光类型，与 XJSceneLightData::Type 约定一致。
    enum class XJLightType
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    class XJLightComponent : public XJComponent
    {
        public:
            XJLightType XJGetLightType() const { return mLightType; }
            void XJSetLightType(XJLightType type) { mLightType = type; }

            bool XJGetEnable() const { return mEnable; }
            void XJSetEnable(bool enable) { mEnable = enable; }

            const glm::vec3& XJGetColor() const { return mColor; }
            void XJSetColor(const glm::vec3& color) { mColor = color; }

            float XJGetIntensity() const { return mIntensity; }
            void XJSetIntensity(float intensity) { mIntensity = intensity; }

            float XJGetRange() const { return mRange; }
            void XJSetRange(float range) { mRange = range > 0.0f ? range : 0.001f; }

            // 聚光角为中心轴到锥面的半角（度），始终满足 0 <= inner < outer < 90。
            float XJGetInnerAngleDegrees() const { return mInnerAngleDegrees; }
            void XJSetInnerAngleDegrees(float angle)
            {
                if (!(angle >= 0.0f))
                    angle = 0.0f;
                mInnerAngleDegrees = std::clamp(angle, 0.0f, mOuterAngleDegrees - 0.01f);
            }

            float XJGetOuterAngleDegrees() const { return mOuterAngleDegrees; }
            void XJSetOuterAngleDegrees(float angle)
            {
                if (!(angle < 90.0f))
                    angle = 89.99f;
                mOuterAngleDegrees = std::clamp(angle, mInnerAngleDegrees + 0.01f, 89.99f);
            }

        private:
            XJLightType mLightType = XJLightType::Directional;
            bool mEnable = true;
            glm::vec3 mColor = glm::vec3(1.0f, 1.0f, 1.0f);
            float mIntensity = 1.0f;
            float mRange = 10.0f;
            float mInnerAngleDegrees = 20.0f;
            float mOuterAngleDegrees = 30.0f;
    };
}

#endif
