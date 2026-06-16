#ifndef XJ_EDITOR_SCENE_CONTROLLER_H
#define XJ_EDITOR_SCENE_CONTROLLER_H

#include "Asset/XJAsset.h"
#include "Asset/XJSceneAsset.h"
#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "UI/XJEditorSelection.h"
#include "UI/XJEditorComponentTypes.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace XJ
{
    class XJAssetRegistry;
    class XJScene;
    class XJSampler;
    class XJTexture;

    struct XJEditorUIState;// 编辑器 UI 状态（例如选中、高亮等）
    /**
     * @brief 编辑器场景控制器
     *
     * 负责场景的加载、卸载、保存，以及场景内实体的增删改回调、
     * 默认资源设置、实例化控制等。是编辑器操作场景的核心管理类。
     */

    class XJEditorSceneController
    {
        public:
            //==========================================================
            // 回调类型定义
            //==========================================================
            using BeforeDeleteCallback = std::function<void(XJScene& scene, const std::vector<XJEditorEntityId>& ids)>;/// 场景中即将删除某些实体时的回调
            using AfterMutationCallback = std::function<void()>;//修改场景后刷新UI
           
            using BeforeOpenSceneCallback = std::function<void()>;//打开场景之前（保存之前场景，清理状态）
            using AfterOpenSceneCallback = std::function<void(XJScene& scene)>;//打开场景之后  加载ecs修改ui
            
            using CanDeleteEntityCallback = std::function<bool(XJEditorEntityId id)>;//是否能删除ID
            //using CanDeleteComponentCallback = std::function<bool(XJEditorEntityId id, XJEditorComponentType componentType)>;//是否能删除组件
            using ShouldExposeEntityCallback = std::function<bool(XJEditorEntityId id)>;//是否隐藏实体
            //==========================================================
            // 设置与注入
            //==========================================================
            void SetScene(XJScene* scene);//设置当前场景
            void SetAssetRegistry(XJAssetRegistry* registry);//设置资产注册表，用于查找/加载资产
            void SetDefaultResources(std::shared_ptr<XJTexture> defaultTexture, std::shared_ptr<XJSampler> defaultSampler);//设置默认资产
            void SetCurrentScenePath(const std::filesystem::path& path);//设置当前场景文件的磁盘路径（用于保存等操作）

            void SetBeforeDeleteCallback(BeforeDeleteCallback callback);/// 设置“删除前”回调
            void SetAfterMutationCallback(AfterMutationCallback callback);/// 设置“修改后”回调

            void SetBeforeOpenSceneCallback(BeforeOpenSceneCallback callback);/// 设置“打开场景前”回调
            void SetAfterOpenSceneCallback(AfterOpenSceneCallback callback); /// 设置“打开场景后”回调

            void SetCanDeleteEntityCallback(CanDeleteEntityCallback callback);
            //==========================================================
            // 场景生命周期
            //==========================================================
            bool LoadOrCreateDefaultScene(XJEditorUIState& uiState, XJAssetHandle defaultSceneHandle, XJAssetHandle defaultMeshHandle, const std::filesystem::path& scenePath);//加载默认场景
            bool OpenSceneAsset( XJEditorUIState& uiState,const std::filesystem::path& scenePath,XJAssetHandle sceneHandle);//打开场景

            void MarkSceneDirty();/// 标记当前场景已被修改（设为脏状态）
            bool SaveCurrentScene();/// 保存当前场景到磁盘    @return 是否保存成功
            //==========================================================
            // 编辑操作处理
            //==========================================================
            void RefreshViewModels(XJEditorUIState& uiState);//更新编辑器视口
            void ProcessRequests(XJEditorUIState& uiState);//处理编辑器请求
            //==========================================================
            // 访问器
            //==========================================================
            XJSceneInstantiateContext& GetInstantiateContext();//获取场景实例上下文
            const XJSceneInstantiateContext& GetInstantiateContext() const;

            std::shared_ptr<XJSceneAsset> GetSceneAsset() const;// 获取当前加载的场景资产（只读共享指针）
            const std::filesystem::path& GetCurrentScenePath() const; /// 获取当前场景文件路径
            bool IsSceneDirty() const;/// 查询当前场景是否已被修改（脏状态）

            void SetShouldExposeEntityCallback(ShouldExposeEntityCallback callback);
            void SetDefaultMeshHandle(XJAssetHandle handle);//设置默认mesh
        
        private:
            void ResetSceneRequestState(XJEditorUIState& uiState);// 重置场景请求状态
            void ResetSelectionForScene(XJEditorUIState& uiState, XJAssetHandle sceneHandle);// 重置与场景相关的选中状态
            bool InstantiateSceneAsset(std::shared_ptr<XJSceneAsset> sceneAsset, XJAssetHandle sceneHandle);// 将场景资产实例化为运行时的 XJScene
            void NotifyAfterMutation();// 触发“修改后”回调（通知外部 UI 更新）

            XJScene* mScene = nullptr;
            XJAssetRegistry* mAssetRegistry = nullptr;

            std::shared_ptr<XJTexture> mDefaultTexture;
            std::shared_ptr<XJSampler> mDefaultSampler;

            std::shared_ptr<XJSceneAsset> mSceneAsset;
            XJSceneInstantiateContext mInstantiateContext;// 实例化上下文

            std::filesystem::path mCurrentScenePath = "Resource/Scenes/Default.xjscene";
            bool mSceneDirty = false; // 场景是否已被修改（需要保存）
            // 回调函数
            BeforeDeleteCallback mBeforeDeleteCallback;// 删除实体前回调
            AfterMutationCallback mAfterMutationCallback; // 场景修改后回调

            BeforeOpenSceneCallback mBeforeOpenSceneCallback;// 打开场景前回调
            AfterOpenSceneCallback mAfterOpenSceneCallback; // 打开场景后回调

            CanDeleteEntityCallback mCanDeleteEntityCallback;

            ShouldExposeEntityCallback mShouldExposeEntityCallback;
            XJAssetHandle mDefaultMeshHandle = 0;//默认mesh 的ECS id

    };
}

#endif