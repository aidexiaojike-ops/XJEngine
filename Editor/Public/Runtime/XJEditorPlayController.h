#ifndef XJ_EDITOR_PLAY_CONTROLLER_H
#define XJ_EDITOR_PLAY_CONTROLLER_H

#include "Runtime/XJEditorPlayState.h"

#include <functional>
#include <memory>

namespace XJ
{
    class XJAssetRegistry;
    class XJEntity;
    class XJScene;
    class XJSystem;
    class XJTexture;
    class XJSampler;

    class XJEditorPlayController
    {
        public:
            using SystemFactory = std::function<std::shared_ptr<XJSystem>(XJScene&)>;

            XJEditorPlayController();
            ~XJEditorPlayController();

            XJEditorPlayController(const XJEditorPlayController&) = delete;
            XJEditorPlayController& operator=(const XJEditorPlayController&) = delete;

            bool Start(
                const XJScene& editorScene,
                XJAssetRegistry& registry,
                const std::shared_ptr<XJTexture>& defaultTexture,
                const std::shared_ptr<XJSampler>& defaultSampler);
            bool Pause();
            bool Resume();
            void Update(float deltaTime);
            void Stop();

            bool RegisterSystemFactory(SystemFactory factory);
            void ClearSystemFactories();

            XJEditorPlayState GetState() const;
            XJScene* GetRuntimeScene() const;
            XJEntity* GetRuntimeCamera() const;

        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
    };
}

#endif
