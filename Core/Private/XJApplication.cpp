#include "XJApplication.h"
#include "Edit/SpdlogDebug.h"
#include "ECS/XJScene.h"  // 新增：提供XJScene完整定义
#include "Input/XJInput.h"

#include <Windows.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>

namespace XJ
{

    XJAppContext XJApplication::sAppContext{};//全局应用程序上下文


    XJApplication::XJApplication() = default;
    XJApplication::~XJApplication() = default;

    void XJApplication::Start(int argc, char** argv)
    {
        mStopped = false;

        std::error_code launchDirectoryError;
        mLaunchWorkingDirectory = std::filesystem::current_path(launchDirectoryError);
        if (launchDirectoryError)
            mLaunchWorkingDirectory.clear();

        // 先解析命令行（--project），再据此确定工作目录，
        // 让所有 Resource/... 相对路径都解析到正确的项目根。
        ParseArgs(argc, argv);
        ConfigureWorkingDirectory();

        // 在这里可以添加应用程序启动时的初始化代码
        mSpdlogDebug = std::make_unique<SpdlogDebug>();//创建日志对象
        spdlog::info("应用程序启动");

        sAppContext.renderThreadId = std::this_thread::get_id();
        
        OnConfiguration(&mAppSettings);

        mWindow = std::make_unique<XJGlfwWindow>(mAppSettings.windowWidth, mAppSettings.windowHeight, mAppSettings.title);
        XJInput::XJGetInstance().SetWindow(mWindow.get());

         // 创建渲染上下文（假设构造函数接受窗口指针）
        mRenderContext = std::make_unique<XJRenderContext>(mWindow.get());
        sAppContext.renderContext = mRenderContext.get();

        sAppContext.app = this;
        OnInit();//调用初始化函数
        LoadScene();//加载场景

        mStartTimePoint = std::chrono::steady_clock::now();//记录程序开始时间点
    }

    void XJApplication::Stop()
    {
        if (mStopped)
            return;

        mStopped = true;

        // 在这里可以添加应用程序停止时的清理代码
        spdlog::info("应用程序停止");

        auto runShutdownStage = [](const char* name, auto&& stage)
        {
            try
            {
                stage();
            }
            catch (const std::exception& e)
            {
                spdlog::error("{} failed during shutdown: {}", name, e.what());
            }
            catch (...)
            {
                spdlog::error("{} failed during shutdown with an unknown exception.", name);
            }
        };

        // 每个阶段独立执行，单个失败不能阻断 Vulkan/window 的最终清理。
        runShutdownStage("OnUIDestroy", [this]() { OnUIDestroy(); });
        runShutdownStage("UnLoadScene", [this]() { UnLoadScene(); });
        runShutdownStage("OnDestroy", [this]() { OnDestroy(); });

        // 用户资源释放后，再销毁渲染上下文和窗口。
        mRenderContext.reset();
        mWindow.reset();

        // 最后清空全局上下文，避免后续代码拿到悬垂指针。
        sAppContext.scene = nullptr;
        sAppContext.renderContext = nullptr;
        sAppContext.app = nullptr;
        sAppContext.renderFrameSlot = 0;
        sAppContext.renderThreadId = {};
    }

    void XJApplication::MainLoop()
    {
        mLastTimePoint = std::chrono::steady_clock::now();//记录上次更新时间点
        // 在这里可以添加应用程序的主循环代码
        spdlog::info("进入主循环");
        while (!mWindow->ShouldClose()) 
        {
            mWindow->PollEvents();

            float deltaTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - mLastTimePoint).count();
            mLastTimePoint = std::chrono::steady_clock::now();//记录上次更新时间点
            mFrameIndex++;

            // 每帧轮询输入状态，供运行时系统在 OnUpdate/OnFixedUpdate 中查询。
            XJInput::XJGetInstance().Update(deltaTime);

            OnUIBegin(); // UI 渲染开始
            
            if(!bPaused)
            {
                OnUpdate(deltaTime);
            }
            
            OnUIEnd();// UI 渲染结束

            OnRender();
        }
        spdlog::info("退出主循环");
    } 

    void XJApplication::ConfigureWorkingDirectory()
    {
        // 优先使用 --project 指定的项目根；否则退回 exe 所在目录。
        std::filesystem::path targetDir;
        if (!mProjectPath.empty())
        {
            targetDir = mProjectPath;
        }
        else
        {
            char exePath[MAX_PATH] = {};
            const DWORD length = GetModuleFileNameA(nullptr, exePath, MAX_PATH);

            if (length == 0 || length >= MAX_PATH)
            {
                spdlog::error("Failed to resolve executable path; current working directory is unchanged.");
                return;
            }

            targetDir = std::filesystem::path(exePath).parent_path();
            if (targetDir.empty())
            {
                spdlog::error("Failed to resolve executable directory; current working directory is unchanged.");
                return;
            }
        }

        std::error_code ec;
        std::filesystem::current_path(targetDir, ec);
        if (ec)
        {
            spdlog::error(
                "Failed to set working directory to '{}': {}",
                targetDir.string(),
                ec.message());
            return;
        }

        spdlog::debug("Working directory set to '{}'", targetDir.string());
    }

    void XJApplication::ParseArgs(int argc, char** argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--project" && i + 1 < argc)
            {
                mProjectPath = argv[++i];
                if (mProjectPath.is_relative() && !mLaunchWorkingDirectory.empty())
                    mProjectPath = mLaunchWorkingDirectory / mProjectPath;
                mProjectPath = mProjectPath.lexically_normal();
                continue;
            }

            constexpr std::string_view kPrefix = "--project=";
            if (arg.rfind(kPrefix, 0) == 0)
            {
                mProjectPath = arg.substr(kPrefix.size());
                if (mProjectPath.is_relative() && !mLaunchWorkingDirectory.empty())
                    mProjectPath = mLaunchWorkingDirectory / mProjectPath;
                mProjectPath = mProjectPath.lexically_normal();
            }
        }
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
