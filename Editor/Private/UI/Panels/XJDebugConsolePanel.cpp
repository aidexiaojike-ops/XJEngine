#include "UI/Panels/XJDebugConsolePanel.h"

#include "UI/XJEditorUILayer.h"
#include "UI/XJEditorLog.h"

#include <imgui.h>
#include <cstring>
#include <string>


namespace XJ
{
     XJDebugConsolePanel::XJDebugConsolePanel(XJEditorUIState& state, XJEditorPanelConfig_DebugConsole* config)
        :mState(state), mConfig(config)
    {
        if (mConfig)
            XJEditorLog::XJGet().SetMaxLines(mConfig->maxLines);
    }

    XJDebugConsolePanel::~XJDebugConsolePanel()
    {
    }


    void XJDebugConsolePanel::DrawUI()
    {
        if (!mState.ShowDebugConsole)
            return;

        const char* title = mConfig ? mConfig->title.c_str() : "Output Log";
        ImGui::Begin(title);

        auto& log = XJEditorLog::XJGet();

        //filter checkboxes 日志级别过滤选项
        bool showInfo = mConfig ? mConfig->showInfo : true;
        bool showWarning = mConfig ? mConfig->showWarning : true;
        bool showError = mConfig ? mConfig->showError : true;
        bool autoScroll = mConfig ? mConfig->autoScroll : true;

        if (ImGui::Checkbox("Info", &showInfo) && mConfig)
            mConfig->showInfo = showInfo;

        ImGui::SameLine();

        if (ImGui::Checkbox("Warning", &showWarning) && mConfig)
            mConfig->showWarning = showWarning;

        ImGui::SameLine();

        if (ImGui::Checkbox("Error", &showError) && mConfig)
            mConfig->showError = showError;

        ImGui::SameLine();

        ImGui::Checkbox("Critical", &mShowCritical);
        ImGui::SameLine();

        ImGui::Checkbox("Trace", &mShowTrace);
        ImGui::SameLine();

        if (ImGui::Button("Clear"))
            log.Clear();
        ImGui::SameLine();

        if (ImGui::Checkbox("Auto Scroll", &autoScroll) && mConfig)
            mConfig->autoScroll = autoScroll;

        // search filter 输入框，允许用户输入文本过滤日志条目
        ImGui::InputTextWithHint("##Search", "Search...", mSearchBuffer, sizeof(mSearchBuffer));
        bool hasFilter = mSearchBuffer[0] != '\0';//判断是否有搜索过滤条件
        // log area 显示日志条目的区域，支持滚动
        ImGui::Separator();
        ImGui::BeginChild("LogArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        auto entries = log.XJGetEntriesCopy();
        for (const auto& entry : entries)
        {
             // level filter  根据日志级别过滤日志条目，如果用户取消了某个级别的显示选项，则跳过该级别的日志条目
            if (entry.Level == XJEditorLogLevel::Info    && !showInfo)    continue;
            if (entry.Level == XJEditorLogLevel::Warning && !showWarning) continue;
            if (entry.Level == XJEditorLogLevel::Error   && !showError)   continue;
            if (entry.Level == XJEditorLogLevel::Critical && !mShowCritical) continue;
            if (entry.Level == XJEditorLogLevel::Trace && !mShowTrace) continue;

            // search filter 根据用户输入的搜索关键词过滤日志条目，如果日志消息中不包含搜索关键词，则跳过该日志条目
            if(hasFilter)
            {
                if (entry.Message.find(mSearchBuffer) == std::string::npos)
                    continue;
            }
            
            //color by level 根据日志级别设置不同的文本颜色，增强日志的可读性和区分度
            ImVec4 color;
            switch(entry.Level)
            {
                case XJEditorLogLevel::Trace:    color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
                case XJEditorLogLevel::Info:     color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
                case XJEditorLogLevel::Warning:  color = ImVec4(1.0f, 1.0f, 0.5f, 1.0f); break;
                case XJEditorLogLevel::Error:    color = ImVec4(1.0f, 0.5f, 0.5f, 1.0f); break;
                case XJEditorLogLevel::Critical: color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);//设置文本颜色
            std::string line = "[" + entry.Timestamp + "] " + entry.Message;
            ImGui::TextUnformatted(line.c_str());//显示日志条目，格式为 [时间戳] 消息内容，使用不同颜色区分日志级别
            ImGui::PopStyleColor();//恢复默认文本颜色
        }

        // auto scroll to bottom 如果启用了自动滚动，并且当前滚动位置已经接近底部，则在新日志条目添加时自动滚动到底部
        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

}