#ifndef XJ_APPLICATION_H
#define XJ_APPLICATION_H

#include "Edit/SpdlogDebug.h"
#include "Edit/XJGlfwWindow.h"
#include "XJApplicationContext.h"
// XJApplication.h
#include "Render/XJRenderContext.h" // 确保包含
//应用程序基类

namespace XJ
{
    struct AppSettings
    {
        // 可以在这里添加应用程序的配置选项，例如窗口标题、初始窗口大小等
        int windowWidth = 800;
        int windowHeight = 600;
        const char *title = "XJEngine Application";
    };

    class XJApplication
    {
        public:
            static XJAppContext* XJGetAppContext() { return &sAppContext; }

            std::chrono::steady_clock::time_point XJGetStartTimePoint() const { return mStartTimePoint; }

            void Start(int argc, char* argv[]);
            void Stop();
            void MainLoop();

            void Pause() { bPaused = true; }
            void Resume() {if(bPaused) bPaused = false; }
            bool XJGetPaused() const { return bPaused; }

            float XJGetStartTimeSecond() const { return std::chrono::duration<float>(std::chrono::steady_clock::now() - mStartTimePoint).count(); }
            uint64_t XJGetFrameIndex() const {return mFrameIndex;}//帧的Index

            XJGlfwWindow* XJGetWindow() const { return mWindow.get(); }
        protected:
            virtual void OnConfiguration(AppSettings *appSettings){}
            virtual void OnInit(){}
            virtual void OnUpdate(float deltaTime){}
            virtual void OnRender(){}
            virtual void OnDestroy(){}

            virtual void OnSceneInit(XJScene *scene){}//场景初始化
            virtual void OnSceneDestroy(XJScene *scene){}//场景销毁
            
            std::chrono::steady_clock::time_point mStartTimePoint;//程序开始时间点
            std::chrono::steady_clock::time_point mLastTimePoint;//上次更新时间点
            std::unique_ptr<XJRenderContext> mRenderContext; // 渲染上下文

            std::unique_ptr<XJGlfwWindow>      mWindow;//窗口对象
            std::unique_ptr<XJScene>      mScene;//场景对象
        private:
            void ParseArgs(int argc, char* argv[]);//解析命令行参数
            bool LoadScene(const std::string &filePath = "");//是否加载场景，加载场景 文件夹
            void UnLoadScene();//卸载场景

            std::unique_ptr<SpdlogDebug>       mSpdlogDebug;//日志对象
            AppSettings mAppSettings;//应用程序设置

          
            uint64_t mFrameIndex = 0;//帧索引
            bool bPaused = false;//是否暂停

            static XJAppContext sAppContext;//全局应用程序上下文
    };
}

#endif