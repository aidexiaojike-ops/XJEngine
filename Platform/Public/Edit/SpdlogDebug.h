#ifndef SPDLOG_DEBUG_H
#define SPDLOG_DEBUG_H

#include "Edit/EditIncludes.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>// 基础文件 sink（支持普通文件输出）
#include <spdlog/sinks/stdout_color_sinks.h>// 带颜色的控制台 sink
#include <spdlog/sinks/rotating_file_sink.h> // 引入轮转文件 sink

namespace XJ
{
    class SpdlogDebug
    {
        private:
            /* data */
            void EngineLog();//整个引擎的Log
            void LogBuddha();
        public:
            SpdlogDebug();
            #ifdef _WIN32
            void enableConsoleColor(); // 声明为命名空间内的非static函数
            #endif
           
           
    };
  
    
}



#endif