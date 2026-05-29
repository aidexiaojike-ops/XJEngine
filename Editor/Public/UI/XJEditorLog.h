#ifndef XJ_EDITOR_LOG_H
#define XJ_EDITOR_LOG_H

#include <string>
#include <vector>
#include <deque>
#include <mutex>

namespace XJ
{

    enum class XJEditorLogLevel //日志级别，定义不同严重程度的日志类型
    {
        Trace,
        Info,
        Warning,
        Error,
        Critical
    };

    struct XJEditorLogEntry//日志条目结构，包含日志级别、消息内容和时间戳
    {
        XJEditorLogLevel Level;
        std::string Message;
        std::string Timestamp;
    };

    // 编辑器日志系统，支持多线程安全的日志记录和显示
    class XJEditorLog
    {
        public:
            static XJEditorLog& XJGet();//获取日志系统的单例实例

            void Push(XJEditorLogLevel level, const std::string& message);//添加日志条目到日志系统，线程安全
            void Clear();

            std::deque<XJEditorLogEntry> XJGetEntriesCopy() const;

            void SetMaxLines(int maxLines);//设置日志系统中最大保留的日志条目数量，超过后会丢弃最旧的条目
            int GetMaxLines() const { return mMaxLines; }//获取当前设置的最大日志条目数量

        private:
            XJEditorLog() = default;

            std::deque<XJEditorLogEntry> mEntries;//使用双端队列存储日志条目，方便在达到最大行数时丢弃最旧的条目
            int mMaxLines = 1000;
            mutable std::mutex mMutex;//保护日志条目列表的互斥锁，确保多线程访问时的安全性
    };
}

#endif