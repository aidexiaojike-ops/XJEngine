#include "Controllers/XJEditorExternalDropController.h"

#include "UI/XJEditorUIState.h"

#include <GLFW/glfw3.h>



namespace XJ
{
    void XJEditorExternalDropController::OnExternalFilesDropped(XJEditorUIState& uiState, GLFWwindow* window, int count, const char** paths)
    {
        //清理缓存
        uiState.PendingExternalDroppedFiles.clear();
        //添加缓存
        for(int i = 0; i < count; ++i)
        {
            if(paths[i]){
                uiState.PendingExternalDroppedFiles.emplace_back(paths[i]);
            }
        }
        //获取鼠标位置
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        int windowX = 0;
        int windowY = 0;
        glfwGetWindowPos(window, &windowX, &windowY);
        // GLFW cursor 是客户区坐标，Content Browser 使用屏幕坐标，这里统一到屏幕坐标。
        uiState.PendingExternalDropMousePos = glm::vec2(static_cast<float>(windowX + mouseX), static_cast<float>(windowY + mouseY));
        //把缓存添加到ui
        uiState.HasPendingExternalDrop = !uiState.PendingExternalDroppedFiles.empty();
    }
}