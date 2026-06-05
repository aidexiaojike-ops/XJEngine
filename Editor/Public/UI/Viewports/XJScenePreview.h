#ifndef XJ_SCENE_PREVIEW_H
#define XJ_SCENE_PREVIEW_H

#include "UI/Viewports/XJViewport.h"
#include "UI/XJEditorDragPayload.h"

#include <functional>

namespace XJ
{
    class XJEntity;

    class XJScenePreview : public XJViewport
    {
        public:

            virtual bool Render(VkCommandBuffer cmd) override;

            void SetCamera(XJEntity* camera)
            {
                mPreviewCamera = camera;
            }

            using AssetDropCallback = std::function<void(const XJAssetDragPayload&)>;

            void SetAssetDropCallback(AssetDropCallback callback)
            {
                mAssetDropCallback = std::move(callback);
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
            
            bool mHovered = false;
            bool mFocused = false;
            
            XJEntity* mPreviewCamera = nullptr;//用于预览的摄像机实体

            bool CalculateDropPositionFromViewportRay(const ImVec2& imageMin, const ImVec2& imageSize, glm::vec3& outOrigin, glm::vec3& outDirection) const;//根据鼠标在视口中的位置计算拖放物体在世界空间中的位置
            AssetDropCallback mAssetDropCallback;

        protected:
            virtual void CreateRenderPass(VulkanPhysicalDevices* physicalDevices) override;//创建适用于场景预览的 Vulkan 渲染通道，配置颜色和深度附件，以及子通道的依赖关系

    };
}

#endif