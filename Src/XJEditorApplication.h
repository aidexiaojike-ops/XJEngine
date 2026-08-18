#ifndef XJ_EDITOR_APPLICATION_H
#define XJ_EDITOR_APPLICATION_H

#include "XJApplication.h"
#include "Runtime/XJEditorRuntime.h"

namespace XJ
{
    class XJEditorApplication final : public XJApplication
    {
        protected:
            //初始化场景
            void OnConfiguration(AppSettings* settings) override;
            void OnInit() override;
            void OnSceneInit(XJScene* scene) override;
            void OnSceneDestroy(XJScene* scene) override;
            //初始化ui
            void OnUIBegin() override;
            void OnUpdate(float deltaTime) override;
            void OnUIEnd() override;
            void OnRender() override;
            //释放内存
            void OnUIDestroy() override;
            void OnDestroy() override;

        private:
            // Runtime 必须在 XJApplication 的 RenderContext 销毁前 Shutdown。
            XJEditorRuntime mEditor;
    };
}

#endif