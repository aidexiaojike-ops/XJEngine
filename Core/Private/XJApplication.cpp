#include "XJApplication.h"
#include "Edit/SpdlogDebug.h"
#include "ECS/XJScene.h"  // 新增：提供XJScene完整定义

namespace XJ
{

    XJAppContext XJApplication::sAppContext{};//全局应用程序上下文

    void XJApplication::Start(int argc, char** argv)
    {
        // 在这里可以添加应用程序启动时的初始化代码
        mSpdlogDebug = std::make_unique<SpdlogDebug>();//创建日志对象
        spdlog::info("应用程序启动");

        
        ParseArgs(argc, argv);
        OnConfiguration(&mAppSettings);

        mWindow = std::make_unique<XJGlfwWindow>(mAppSettings.windowWidth, mAppSettings.windowHeight, mAppSettings.title);
       

         // 创建渲染上下文（假设构造函数接受窗口指针）
        mRenderContext = std::make_unique<XJRenderContext>(mWindow.get());
        sAppContext.renderContext = mRenderContext.get();

        sAppContext.app = this;
        OnInit();//调用初始化函数
        LoadScene();;//加载场景

        mStartTimePoint = std::chrono::steady_clock::now();//记录程序开始时间点
    }

    void XJApplication::Stop()
    {
        // 在这里可以添加应用程序停止时的清理代码
        spdlog::info("应用程序停止");
        UnLoadScene();//卸载场景
        OnDestroy();
    }

    void XJApplication::MainLoop()
    {
        mLastTimePoint = std::chrono::steady_clock::now();//记录上次更新时间点
        // 在这里可以添加应用程序的主循环代码
        spdlog::info("进入主循环");
        while (!mWindow->ShouldClose()) {
            mWindow->PollEvents();

            float deltaTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - mLastTimePoint).count();
            mLastTimePoint = std::chrono::steady_clock::now();//记录上次更新时间点
            mFrameIndex++;
            
            if(!bPaused)
            {
                OnUpdate(deltaTime);
            }

            OnRender();

            mWindow->SwapBuffer();
        }
        spdlog::info("退出主循环");
    } 
    void XJApplication::ParseArgs(int argc, char** argv)
    {
         // 此处可以实现对命令行参数的解析，例如设置应用的一些特性
        // TODO: 在此函数中实现相关的逻辑
    }

    bool XJApplication::LoadScene(const std::string &filePath)//是否加载场景，加载场景 文件夹
    {
        if(mScene){UnLoadScene();};//是否有场景 有就卸载 
        mScene = std::make_unique<XJScene>();//创建一个空场景
        // mScene->OnInit();
        OnSceneInit(mScene.get());//初始
        sAppContext.scene = mScene.get();
        return true;
    }
    void XJApplication::UnLoadScene()//卸载场景
    {
        if(mScene)
        {
           
            OnSceneDestroy(mScene.get());//卸载场景
            mScene.reset();//释放内存
            sAppContext.scene = nullptr;//设置场景为空
        }
    }

    //void XJApplication::OnSceneDestroy(XJScene *scene)
    //{
    //}
}