#include "Input/XJEditorInputBindings.h"

#include "Controllers/XJEditorExternalDropController.h"
#include "Edit/XJGlfwWindow.h"
#include "Event/XJEventObserver.h"
#include "Event/XJMouseEvent.h"
#include "UI/Viewports/XJEditorViewportSystem.h"
#include "UI/Viewports/XJScenePreview.h"
#include "Workspace/XJEditorWorkspace.h"

namespace XJ
{
    class XJEditorInputBindings::Impl
    {
        public:
            XJGlfwWindow* Window = nullptr;
            XJEditorWorkspace* Workspace = nullptr;
            XJEditorViewportSystem* Viewports = nullptr;

            std::unique_ptr<XJEventObserver> Observer;
            XJEditorExternalDropController ExternalDropController;

            bool Initialized = false;
    };

    XJEditorInputBindings::XJEditorInputBindings()
        : mImpl(std::make_unique<Impl>())
    {
    }

    XJEditorInputBindings::~XJEditorInputBindings()
    {
        Shutdown();
    }

    bool XJEditorInputBindings::Init(const XJEditorInputBindingsInitInfo& info)
    {
        Shutdown();

        if (!info.Window || !info.Workspace || !info.Viewports)
        {
            return false;
        }

        mImpl->Window = info.Window;
        mImpl->Workspace = info.Workspace;
        mImpl->Viewports = info.Viewports;

        mImpl->Observer = std::make_unique<XJEventObserver>();

        mImpl->Observer->OnEvent<XJMouseScrollEvent>(
            [this](const XJMouseScrollEvent& event)
            {
                if (mImpl->Viewports)
                    mImpl->Viewports->OnMouseScroll(
                        event.mYOffset);
            });

        mImpl->Window->SetDropCallback(
            [this](int count, const char** paths)
            {
                if (!mImpl->Workspace ||
                    !mImpl->Window)
                {
                    return;
                }

                mImpl->ExternalDropController
                    .OnExternalFilesDropped(
                        mImpl->Workspace->GetUIState(),
                        mImpl->Window
                            ->XJGetImplWindowPointer(),
                        count,
                        paths);
            });

        if (XJScenePreview* preview = mImpl->Viewports->GetScenePreview())
        {
            preview->SetAssetDropCallback(
                [this](const XJAssetDragPayload& payload)
                {
                    if (mImpl->Workspace)
                        mImpl->Workspace
                            ->HandleSceneAssetDrop(payload);
                });
        }

        mImpl->Initialized = true;
        return true;
    }

    void XJEditorInputBindings::EndUIFrame()
    {
        if (!mImpl->Initialized ||!mImpl->Workspace)
        {
            return;
        }

        mImpl->ExternalDropController.DiscardUnconsumedDrop(mImpl->Workspace->GetUIState());
    }

    void XJEditorInputBindings::Shutdown()
    {
        if (!mImpl)
            return;

        // 先清除所有捕获 this 的回调。
        if (mImpl->Window)
            mImpl->Window->SetDropCallback({});

        if (mImpl->Viewports)
        {
            if (XJScenePreview* preview =
                    mImpl->Viewports->GetScenePreview())
            {
                preview->SetAssetDropCallback({});
            }
        }

        // Observer 析构会从全局 EventDispatcher 注销。
        mImpl->Observer.reset();

        mImpl->Window = nullptr;
        mImpl->Workspace = nullptr;
        mImpl->Viewports = nullptr;
        mImpl->Initialized = false;
    }
}