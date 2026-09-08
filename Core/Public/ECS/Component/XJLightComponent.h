#ifndef XJ_LIGHT_COMPONENT_H
#define XJ_LIGHT_COMPONENT_H

#include "ECS/XJComponent.h"
#include "Edit/Mathinclude.h"

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

        private:
            XJLightType mLightType = XJLightType::Directional;
            bool mEnable = true;
            glm::vec3 mColor = glm::vec3(1.0f, 1.0f, 1.0f);
            float mIntensity = 1.0f;
    };
}

#endif