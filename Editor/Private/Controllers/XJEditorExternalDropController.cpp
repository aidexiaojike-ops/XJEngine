#include "Controllers/XJEditorExternalDropController.h"

#include "UI/XJEditorUIState.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <spdlog/spdlog.h>



namespace XJ
{
    namespace
    {
        void ClearPendingExternalDrop(XJEditorUIState& uiState)
        {
            uiState.PendingExternalDroppedFiles.clear();
            uiState.PendingExternalDropMousePos = glm::vec2(0.0f);
            uiState.HasPendingExternalDrop = false;
        }
    }

    void XJEditorExternalDropController::OnExternalFilesDropped(XJEditorUIState& uiState, GLFWwindow* window, int count, const char** paths)
    {
        // A new OS drop replaces any event that was not consumed in the previous frame.
        ClearPendingExternalDrop(uiState);

        if (!window || count <= 0 || !paths)
            return;

        for (int i = 0; i < count; ++i)
        {
            if (!paths[i] || paths[i][0] == '\0')
                continue;

            try
            {
                // GLFW supplies UTF-8 paths. Convert explicitly so non-ASCII
                // filenames become native filesystem paths correctly on Windows.
                uiState.PendingExternalDroppedFiles.emplace_back(
                    reinterpret_cast<const char8_t*>(paths[i]));
            }
            catch (const std::exception& exception)
            {
                // Do not let a malformed path escape through the GLFW callback.
                // Other valid files from the same drop can still be imported.
                spdlog::warn(
                    "External dropped path is invalid UTF-8 and was skipped: {}",
                    exception.what());
            }
        }

        if (uiState.PendingExternalDroppedFiles.empty())
            return;

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        int windowX = 0;
        int windowY = 0;
        glfwGetWindowPos(window, &windowX, &windowY);

        // GLFW cursor 是客户区坐标，Content Browser 使用屏幕坐标，这里统一到屏幕坐标。
        uiState.PendingExternalDropMousePos = glm::vec2(static_cast<float>(windowX + mouseX), static_cast<float>(windowY + mouseY));
        uiState.HasPendingExternalDrop = true;
    }

    void XJEditorExternalDropController::DiscardUnconsumedDrop(XJEditorUIState& uiState)
    {
        if (!uiState.HasPendingExternalDrop)
            return;

        // Every UI drop target has already run for this frame. A still-pending
        // event was released over an unsupported area and must not survive.
        ClearPendingExternalDrop(uiState);
    }
}
