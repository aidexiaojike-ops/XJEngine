#ifndef XJ_ENTRY_POINT_H
#define XJ_ENTRY_POINT_H
// 入口点头文件
// #include "XJEngine.h"
#include "XJApplication.h"
#include <iostream>  // 添加这行

extern XJ::XJApplication* CreateApplicationEntryPoint();
//最好是定义一下这个宏
// #if  XJ_EBGINE_PLATFORM_WINDOWS || XJ_EBGINE_PLATFORM_LINUX || XJ_EBGINE_PLATFORM_MACOS 

int main(int argc, char* argv[])
{
    // 创建应用程序实例
    std::cout <<"--------开始执行-------"<< std::endl;
    XJ::XJApplication* app = CreateApplicationEntryPoint();
    //start
    app->Start(argc, argv);
    //main loop
    app->MainLoop();
    //stop
    app->Stop();
    //delete
    return EXIT_SUCCESS;
}

// #endif

#endif