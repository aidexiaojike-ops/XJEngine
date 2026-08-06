#ifndef XJ_APPLICATION_CONTEXT_H
#define XJ_APPLICATION_CONTEXT_H

#include <cstdint>
#include <thread>

namespace XJ
{
    class XJApplication;
    class XJScene;
    class XJRenderContext;

    struct XJAppContext
    {
        XJApplication *app = nullptr;
        XJScene *scene = nullptr;
        XJRenderContext *renderContext = nullptr;
        
        uint32_t renderFrameSlot = 0;
         // 当前渲染线程。GPU 资源创建和默认 command pool/queue 提交必须在该线程执行。
        std::thread::id renderThreadId{};
    };
}


#endif
