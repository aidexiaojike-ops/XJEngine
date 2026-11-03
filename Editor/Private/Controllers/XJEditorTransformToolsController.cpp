#include "Controllers/XJEditorTransformToolsController.h"
#include "Services/XJEditorSceneService.h"

#include "UI/XJEditorUIState.h"

#include <spdlog/spdlog.h>

#include <type_traits>
#include <utility>

namespace XJ
{
    void XJEditorTransformToolsController::Publish(XJEditorUIState& state, bool canChangeSpace) const
    {
        auto& view = state.TransformTools;

        view.Space = mSpace;
        view.CanChangeSpace = canChangeSpace;
        view.SettingsRevision = mSettingsRevision;

        if (!canChangeSpace)
            state.TransformGizmo = {};
    }

    void XJEditorTransformToolsController::ProcessRequests(XJEditorUIState& state, bool canChangeSpace)
    {
        auto requests = std::move(state.TransformToolRequests);

        state.TransformToolRequests.clear();
        // 即使当前禁止修改，也要消费并丢弃请求。
        // 避免 Play 中积压的请求在 Stop 后执行。
        if(!canChangeSpace)
        {
            Publish(state, false);
            return;
        }

        for (const auto& request : requests)
        {
            std::visit([this](const auto& command)
            {
                using T = std::decay_t<decltype(command)>;

                if constexpr (std::is_same_v<T, XJSetTransformSpace>)
                {
                    if (!IsValidTransformSpace(command.Space))
                    {
                        spdlog::warn(
                            "Ignored invalid Transform "
                            "tool space {}.",
                            static_cast<int>(
                                command.Space));
                        return;
                    }
                    // 重复请求不产生设置版本变化。
                    if (command.Space == mSpace)
                        return;
                    mSpace = command.Space;
                    ++mSettingsRevision;
                }
            },
            request);
        }
        Publish(state, true);
    }

    void XJEditorTransformToolsController::Reset(XJEditorUIState& state)
    {
        mSpace = XJEditorTransformSpace::Local;
        mSettingsRevision = 0;

        state.TransformToolRequests.clear();
        state.TransformGizmo = {};

        Publish(state, false);
    }

    void XJEditorTransformToolsController::RefreshGizmo(XJEditorUIState& state, XJScene* scene, bool canEdit)
    {
        state.TransformGizmo = {};

        if (!scene || !canEdit || state.WorkspaceReadOnly || state.RuntimeInspection)
        {
            return;
        }

        state.TransformGizmo = XJEditorSceneService::BuildTransformGizmoView(
                *scene,
                state.Selection.SelectedEntity,
                mSpace);
    }
}