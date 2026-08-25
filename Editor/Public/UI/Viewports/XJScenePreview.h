#ifndef XJ_SCENE_PREVIEW_H
#define XJ_SCENE_PREVIEW_H

#include "UI/Viewports/XJViewport.h"
#include "UI/Viewports/XJViewportRenderSurface.h"
#include "UI/XJEditorDragPayload.h"

#include <functional>

namespace XJ
{
    class XJEntity;

    class XJScenePreview : public XJViewport
    {
        public:

            bool Init(XJRenderContext* renderContext);
            void Shutdown();
            void PrepareBeforeRender();
            void PostRender();
            bool Render(VkCommandBuffer cmd);

            template<typename T, typename... Args>
            void AddMaterialSystem(Args&&... args)
            {
                mSurface.template AddMaterialSystem<T>(std::forward<Args>(args)...);
            }

            void SetCamera(XJEntity* camera)
            {
                mPreviewCamera = camera;
            }

            using AssetDropCallback = std::function<void(const XJAssetDragPayload&)>;
            using EntityPickCallback = std::function<void(
                const glm::vec3& rayOrigin,
                const glm::vec3& rayDirection,
                float maxDistance)>;

            void SetAssetDropCallback(AssetDropCallback callback)
            {
                mAssetDropCallback = std::move(callback);
            }

            void SetEntityPickCallback(EntityPickCallback callback)
            {
                mEntityPickCallback = std::move(callback);
            }

            void DrawUI() override;

            bool IsHovered() const
            {
                return mHovered;
            }

            bool IsFocused() const
            {
                return mFocused;
            }

            bool ShouldControlCamera() const
            {
                return mHovered || mFocused;
            }

        private:
            XJViewportRenderSurface mSurface;
            XJEntity* mPreviewCamera = nullptr;//用于预览的摄像机实体

            bool CalculateDropPositionFromViewportRay(const ImVec2& imageMin, const ImVec2& imageSize, glm::vec3& outOrigin, glm::vec3& outDirection, float& outMaxDistance) const;//根据鼠标在视口中的位置计算拖放物体在世界空间中的位置
            AssetDropCallback mAssetDropCallback;
            EntityPickCallback mEntityPickCallback;

        protected:
            ImTextureID GetViewportTextureID() const override;
            bool IsViewportTextureReady() const override;
            void OnViewportResized(uint32_t width, uint32_t height) override;

    };
}

#endif
