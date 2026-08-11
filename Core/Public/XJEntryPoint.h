#ifndef XJ_ENTRY_POINT_H
#define XJ_ENTRY_POINT_H
// 入口点头文件
// #include "XJEngine.h"
#include "XJApplication.h"
#include <iostream>  
#include <memory>

#include <exception>
#include <cstdlib>
#include <spdlog/spdlog.h>

extern XJ::XJApplication* CreateApplicationEntryPoint();
//最好是定义一下这个宏
// #if  XJ_EBGINE_PLATFORM_WINDOWS || XJ_EBGINE_PLATFORM_LINUX || XJ_EBGINE_PLATFORM_MACOS 


int main(int argc, char* argv[])
{
    // 创建应用程序实例
    std::unique_ptr<XJ::XJApplication> app;
    bool started = false;   // 标记应用程序是否已启动，便于在异常情况下进行清理

    try
    {
        std::cout <<"--------开始执行-------"<< std::endl;
        app.reset(CreateApplicationEntryPoint());

        if (!app)
        {
            std::cerr << "CreateApplicationEntryPoint failed" << std::endl;
            return EXIT_FAILURE;
        }

        app->Start(argc, argv);
        started = true;
        //main loop
        app->MainLoop();

        //stop
        app->Stop();
        started = false;
        //delete
        return EXIT_SUCCESS;

    }
    catch (const std::exception& e)
    {
        // spdlog 可能还没初始化，stderr 是最后兜底输出。
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        spdlog::error("Unhandled exception: {}", e.what());
    }
    catch (...)
    {
        std::cerr << "Unhandled unknown exception" << std::endl;
        spdlog::error("Unhandled unknown exception");
    }


    if(started && app)
    {
        try
        {
            // 异常退出时仍尝试释放 Vulkan / ImGui / scene 等资源。
            app->Stop();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception during shutdown: " << e.what() << std::endl;
            spdlog::error("Exception during shutdown: {}", e.what());
        }
        catch (...)
        {
            std::cerr << "Unknown exception during shutdown" << std::endl;
            spdlog::error("Unknown exception during shutdown");
        }
        
    }

    return EXIT_FAILURE;
}

// #endif

#endif