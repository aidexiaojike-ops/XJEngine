# XJEngine

<div align="center">

**[English](README.md)** &nbsp;&nbsp;|&nbsp;&nbsp; **简体中文**

</div>

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-brightgreen)](https://github.com/aidexiaojike-ops/XJEngine)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.2+-orange.svg)](https://www.vulkan.org/)
[![CMake](https://img.shields.io/badge/CMake-3.10+-yellow.svg)](https://cmake.org/)

XJEngine 是一个基于 Vulkan 和 ECS 架构的轻量级现代游戏引擎，重点关注高性能实时渲染、模块化材质系统、资产管线和内置编辑器能力。

## ✨ 核心特性

| 特性 | 描述 |
|------|------|
| **Vulkan 渲染器** | 多平台现代图形 API，GPU 驱动渲染管线 |
| **稳健的 Swapchain 生命周期** | 结构化 acquire/present 结果、窗口尺寸变化重建、减少逐帧阻塞，并明确处理 device-lost 状态 |
| **ECS 架构** | 基于 EnTT 的高性能实体组件系统 |
| **事件驱动系统** | 完整的窗口、鼠标、键盘事件处理 |
| **模块化材质系统** | 可扩展的纹理、采样器、UBO 管线 |
| **Shader Schema 系统** | JSON 定义着色器参数，Schema 验证、绑定解析、描述符布局构建、SPIR-V 反射、材质资产序列化 |
| **Surface 材质系统** | Schema 驱动的 Surface 管线，共享 Frame/Light UBO、材质参数块、纹理绑定与动态描述符池扩容 |
| **场景灯光** | 方向光、点光和聚光灯支持场景持久化与逐帧 GPU 上传，并提供范围、强度、颜色和聚光锥角控制 |
| **Lit Shader** | Schema 驱动的前向光照，支持基础颜色/Albedo 纹理、高光强度、光泽度以及方向光/点光/聚光灯计算 |
| **运行时材质生成** | 支持程序化创建材质、随机颜色、纹理和 UV 变换 |
| **程序化纹理** | 从像素数据直接生成纹理，无需外部文件 |
| **摄像机系统** | 独立 Camera 模块，轨道/自由模式，编辑器摄像机管理器，ECS 摄像机系统 |
| **资产系统** | Asset/Resource 双层架构，glTF 2.0 导入，资产注册表扫描与引导，场景实例化/切换，网格/纹理/材质加载器 |
| **ImGui 编辑器** | MVVM 架构，支持 Docking、多视口、拖拽，内建面板（Hierarchy、Inspector、Content Browser、Console、Scene/Game Preview） |
| **着色器编译** | 构建时自动从 GLSL 编译到 SPIR-V |

## 📋 目录

- [核心特性](#-核心特性)
- [快速开始](#-快速开始)
- [引擎架构](#-引擎架构)
- [构建](#-构建)
- [项目结构](#-项目结构)
- [使用](#-使用)
- [开发](#-开发)
- [已知问题](#-已知问题)
- [路线图](#-路线图)
- [许可证](#-许可证)

## 🚀 快速开始

```bash
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ../bin
./XJEngine
```

**前置要求：**
- CMake 3.10+
- 支持 C++17 的编译器
- Vulkan SDK 可选手动安装；CMake 会自动按顺序解析：`VULKAN_SDK` 环境变量 -> `ThirdParty/VulkanSDK` 本地缓存 -> 自动下载 LunarG Vulkan SDK 1.3.283.0

## 🏗️ 引擎架构

```text
Scene
  -> ECS
  -> MaterialSystem
  -> RenderTarget
  -> Editor/Renderer
  -> CommandBuffer
  -> Swapchain
```

### 资产管线

```text
File -> Importer -> Asset (CPU) -> Factory -> Resource (GPU) -> Renderer
```

- **Asset 层**：纯 CPU 数据，例如顶点、像素、材质参数、场景数据
- **Resource 层**：GPU 资源，例如 `VkBuffer`、`VkImage`
- **资产扫描**：`XJAssetRegistryScanner` 自动扫描 Resource 目录，按扩展名注册资产
- **场景系统**：`.xjscene` -> `XJSceneAssetSerializer` -> `XJSceneInstantiator` -> ECS 实体；灯光序列化包含类型、颜色、强度、范围和聚光灯内外锥角
- **场景切换**：支持多个 `.xjscene` 文件，通过 ECS 生命周期创建/销毁实体
- **注册表**：`XJAssetRegistry` 用于持久化资产句柄与元数据；运行时生成的句柄使用高位命名空间，与稳定的注册表句柄隔离，避免冲突
- **资产元数据**：`.xjmeta` 旁置文件（`XJAssetMetadata`）存储每个资产的句柄/类型/导入器信息；`XJAssetMetadataSerializer` 负责读写；`XJPersistentAssetHandleGenerator` 确保唯一句柄生成
- **Submesh 渲染**：`XJMesh` 通过 `XJSubmesh` 索引范围共享顶点/索引缓冲，提供 `Bind`/`Draw`/`DrawSubmesh`；`XJMaterialRenderItem` 携带 `SubmeshIndex`，使每个 primitive 使用各自材质槽绘制
- **AABB 包围盒**：`XJBoundingBox`（Expand/Merge/Transformed）在 glTF 导入时逐 primitive 计算并存入 `XJMeshPrimitive`/`XJSubmesh`；`XJMesh` 通过 `GetBounds()` 暴露整体包围盒
- **CPU 拾取几何**：`XJMesh` 保留紧凑的 CPU 位置/索引副本，用于 AABB 粗筛后的精确射线-三角形检测
- **glTF 导入**：`XJModelImporter` 将 primitive 合并进共享顶点/索引缓冲，保存每个 primitive 的索引范围（`XJMeshPrimitive`），校验 accessor 与绘制模式（跳过非 TRIANGLES），数据无效时回滚
- **原子 JSON IO**：`XJJsonIO` 统一提供 JSON 读取辅助（float/vec2/vec3/vec4/uint64）与原子文件写入（临时文件 + rename），供所有资产序列化器使用
- **引导程序**：`XJAssetBootstrap` 管理默认资产注册和场景创建
- **运行时工具**：`XJSceneRuntimeUtil` 提供主摄像机查找等运行时辅助功能

- **Shader Schema 系统**：JSON 定义的着色器参数，`XJShaderSchemaValidator` 验证 + `XJShaderSchemaBindingResolver` 绑定解析 + `XJShaderDescriptorLayoutBuilder` 描述符布局构建
- **Lit 材质参数**：`Lit.schema` 提供基础颜色、Albedo 纹理、高光强度和光泽度，并复用通用 SurfaceMaterial 运行时
- **跨阶段 Shader 反射**：Shader stage 使用位掩码，将 Vertex/Fragment 反射出的同一描述符绑定合并进统一 Vulkan 布局
- **Surface 材质系统**：`XJSurfaceMaterialSystem` 渲染 `XJSurfaceMaterialComponent`，按 frame slot 分别跟踪材质参数和资源上传状态
- **共享 Frame/Light 数据**：`XJFrameUbo` 提供投影/视图矩阵、分辨率、帧号/时间与摄像机位置；`XJLightUbo` 使用 std140 布局和逐帧描述符集支持 1 个方向光、最多 8 个点光和 8 个聚光灯
- **场景灯光收集**：`XJLightSceneUtils` 从场景中的 `XJLightComponent` 与 Transform 构建每帧灯光 UBO
- **Shader 运行时布局**：`XJMaterialShaderRuntimeLayout`/`Builder`、`XJMaterialPipelineRuntime`/`Builder`/`Cache`/`Descriptor`、`XJMaterialRuntimeUploader`、`XJSurfaceMaterialBindingUtils` — 运行时 Shader-材质绑定、可选灯光描述符集（`set=3`）、管线缓存与 GPU 上传
- **材质序列化**：`XJMaterialAssetSerializer`、`XJShaderAssetSerializer`、`XJShaderSchemaSerializer`
- **材质工厂缓存**：`XJMaterialFactory` 按资产/默认材质键缓存材质（弱引用）、复用已加载纹理，并提供 `ClearExpiredMaterials`/`ClearCaches` 配合场景生命周期管理
- **Inspector 材质编辑**：通过 `XJEditorMaterialParameterType` 编辑 Float、Color3、Texture2D 等参数
- **Inspector 灯光编辑**：通过场景请求/ViewModel 数据流编辑 Directional/Point/Spot 类型、开关、颜色、强度、范围和聚光灯内外锥角

### 编辑器架构（MVVM）

- **编辑器运行时**：`XJEditorRuntime`（pimpl 封装）统一管理编辑器生命周期——项目配置（`XJEditorProjectConfig`）、工作区、视口系统、帧渲染、输入绑定与 UI Host，由 `Src/XJEditorApplication` 驱动
- **编辑器 Play Mode**：`XJEditorPlayController` 管理游戏模拟生命周期（Start/Pause/Resume/Stop），克隆编辑器场景为运行时场景并接入 `XJSystemScheduler`；`XJEditorPlayState`（`Edit`/`Playing`/`Paused`）跟踪当前模式；`XJEditorWorkspaceMode`（`Edit`/`PlayReadOnly`/`PlayRuntime`）控制工作区编辑权限
- **工作区**：`XJEditorWorkspace` 管理引擎内项目工作区（资源根目录、资产注册表、场景接线），支持 `SelectEntityFromViewportRay` 射线选择实体
- **帧渲染**：`XJEditorFrameRenderer` + `XJEditorRenderResources` 负责 ImGui/Vulkan 帧渲染与共享编辑器渲染资源；`XJEditorUIHost` 承载 UI 层
- **输入绑定**：`XJEditorInputBindings` 集中编辑器输入映射（摄像机、视口、操作）
- **视口系统**：`XJEditorViewportSystem` 统筹 Scene/Game 预览视口、摄像机解析、受保护编辑器实体以及场景挂载/卸载
- **实体拾取**：`XJScenePreview` 通过 `EntityPickCallback` 触发射线-AABB 相交检测，实现实体视口点击选择
- **精确场景拾取**：`XJEditorSceneService::RaycastClosestSceneEntity` 先进行世界空间 AABB 粗筛，再使用 Mesh 的 CPU 顶点/索引副本执行射线-三角形检测
- **选中点轨道旋转**：Scene Preview 可通过视口射线设置摄像机 orbit pivot，使轨道控制围绕命中的表面点旋转
- **灯光 Gizmo**：`XJLightGizmoMaterialSystem` 在 Scene Preview 中以编辑器专用线框显示方向光、点光和聚光灯
- **Controllers**：`XJEditorSceneController`（场景加载/保存/切换 + 基于快照的 Undo/Redo 历史，含场景与材质资产，最多 100 条）+ `XJEditorAssetController`（资产 CRUD）+ `XJEditorCameraManager`（基于实体 ID 的视口摄像机绑定与解析，避免悬垂指针）+ `XJEditorSceneAssetDropController`（资产拖放到场景）+ `XJEditorExternalDropController`（OS文件拖入）
- **Console 日志**：`XJEditorLog` 通过自定义 spdlog sink 将引擎日志桥接到 Console 面板（异步安全复制、级别映射、线程安全队列）
- **Viewport 渲染表面**：`XJViewport` 接口（`GetViewportTextureID`/`IsViewportTextureReady`/`OnViewportResized`）由 `XJViewportRenderSurface` 实现，负责离屏 render pass、render target、调整大小时的延迟 descriptor 释放以及 ImGui 纹理显示
- **Services**：`XJEditorSceneService` + `XJEditorAssetService`（Controller 与 ECS 之间的桥接层）
- **ViewModels**：`XJEditorSceneViewModel`、`XJEditorSelection`、`XJEditorComponentTypes`、`XJEditorAssetRequests`（UI 面板读取快照，写入请求）、`XJEditorAssetViewModel`（资产详情视图：基本信息、着色器验证、网格包围盒展示）
- **面板功能**：Inspector 支持 Transform/Camera/MeshRenderer 组件增删编辑，Hierarchy 支持右键菜单创建/删除/重命名
- **数据流**：`UI Panel → Request → Controller → Service → ECS`

### 主要模块

- **材质系统**：`XJBaseMaterialSystem`、`XJSurfaceMaterialSystem`、`XJMaterialRenderSystemBase`、`XJSurfaceMaterialComponent`、`XJMaterialParameterBlock`/`Builder`/`Writer`
- **摄像机系统**：`XJCameraController`（Core/Camera）、`XJCameraMath`（数学工具）、`XJCameraSystem`（ECS 适配）
- **资产系统**：`XJModelImporter`、`XJTextureImporter`、`XJAssetRegistry`、`XJAssetRegistryScanner`、`XJAssetBootstrap`、`XJSceneRuntimeUtil`、`XJMeshAssetLoader`、`XJJsonIO`
- **ECS 基础**：`XJEntity` 通过场景生命周期 token 校验避免悬垂访问；`XJReservedUUID` 定义引擎/编辑器保留 UUID 区间，用户 UUID 生成自动避开；`XJSystemScheduler` 管理系统生命周期（Start/Stop），驱动 `OnUpdate` + 固定步进 `OnFixedUpdate`；`XJInput` 输入单例每帧轮询 GLFW，提供键盘/鼠标按住/按下/释放查询、鼠标位置/增量/滚轮、WASD 轴向合成；`XJLightComponent` 支持 Directional/Point/Spot 灯光（开关、颜色、强度、范围及聚光灯内外锥角）
- **编辑器系统**：`XJEditorSceneController`、`XJEditorCameraManager`、`XJEditorSceneService`、`XJUIContext`、`XJEditorRenderer`、`XJEditorUILayer`、编辑器面板
- **Vulkan 平台层**：`XJSwapchainAcquireResult`/`XJSwapchainPresentResult` 区分成功、重建和设备丢失状态；`XJVulkanInstance` 自动选择最高支持到 Vulkan 1.3 的 API 版本；`XJVulkanSurface`、`XJGlfwWindow`、`XJVulkanTextureSampler` 增加句柄校验、生命周期顺序和 RAII 释放

## 🛠️ 构建

### Windows

```bash
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

首次配置如果本机没有 Vulkan SDK，CMake 会自动下载约 200MB+ 的 LunarG Vulkan SDK 1.3.283.0，耗时取决于网络。

### Linux

```bash
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 📁 项目结构

```text
XJEngine/
├── Core/                    # 引擎核心：ECS、渲染、资产、摄像机
│   ├── Public/ECS/          # 实体组件系统（Entity、Component、System、SystemScheduler）
│   ├── Public/Camera/       # 摄像机模块（独立于 ECS）
│   ├── Public/Geometry/     # 几何工具（AABB 包围盒、射线相交检测）
│   ├── Public/Input/        # 输入系统（键盘/鼠标/轴向轮询）
│   ├── Public/Asset/        # Asset 层（CPU）
│   │   ├── Importer/        # 模型/纹理/材质导入器
│   │   ├── Loader/          # 资产加载器
│   │   ├── Serialization/   # 场景/材质/Shader 序列化（XJJsonIO 原子写入）
│   │   ├── Instantiation/   # 场景实例化器
│   │   ├── Metadata/        # 资产元数据（.xjmeta、句柄生成器）
│   │   └── Register/        # 资产引导注册/扫描
│   ├── Public/Render/       # 渲染接口
│   │   ├── XJFrameUbo.h     # 共享逐帧摄像机/渲染数据
│   │   ├── XJLightUbo.h     # std140 方向光/点光/聚光灯数据
│   │   ├── XJLightSceneUtils.h # 场景灯光收集与 UBO 打包
│   │   ├── System/          # 渲染系统（材质系统 + RenderSystemBase）
│   │   ├── Material/        # 材质参数与管线运行时（Block/Layout/Builder/Writer/PipelineRuntime）
│   │   └── Shader/          # 着色器资产（Schema/Parameter/Asset/Validator/BindingResolver/Reflection）
│   └── Public/Render/Resource/ # GPU 资源
├── Platform/                # Vulkan、GLFW 封装
├── ThirdParty/              # 第三方库（glfw、imgui、glm、spdlog、entt、json、tinygltf 等）
├── Editor/                  # ImGui 编辑器（MVVM 架构）
│   ├── Public/Controllers/  # Camera/Scene 控制器
│   └── Public/Services/     # 编辑器服务层
├── Src/                     # 应用入口
├── cmake/                    # CMake 模块
├── Resource/                # Shader、Mesh、Material、Scenes、Config
└── bin/                     # 运行时输出
```

## 🎮 使用

### Surface 材质示例

```cpp
auto surfaceMat = XJ::XJMaterialFactory::GetInstance()->CreateMaterial<XJ::XJSurfaceMaterial>();
surfaceMat->SetBaseColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

auto& surfaceComp = entity->AddComponent<XJ::XJSurfaceMaterialComponent>();
surfaceComp.AddMesh(mesh, surfaceMat);
```

### 编辑器 UI 集成

```cpp
XJ::XJEditorRendererInitInfo kUIRendererInfo = {};
kUIRendererInfo.apiVersion = kRenderContext->XJGetInstance()->XJGetApiVersion();

mUIContext = std::make_unique<XJ::XJUIContext>();
mEditorRenderer = std::make_unique<XJ::XJEditorRenderer>();
mUIContext->Init(static_cast<GLFWwindow*>(XJGetWindow()->XJGetImplWindowPointer()));
mEditorRenderer->Init(kUIRendererInfo);
```

## 🔧 开发

### 主要依赖

- GLFW
- EnTT
- GLM
- spdlog
- stb_image
- tinyobjloader
- tinygltf
- Dear ImGui
- nlohmann/json
- SPIRV-Reflect
- Vulkan SDK（自动解析：环境变量 → 本地缓存 → 自动下载 1.3.283.0）

### Vulkan SDK 自动解析

CMake 会按以下顺序解析 Vulkan SDK：

1. `VULKAN_SDK` 环境变量
2. `ThirdParty/VulkanSDK` 本地缓存
3. 自动下载 LunarG Vulkan SDK `1.3.283.0`

首次配置可能下载 200MB+ 安装包。可通过以下参数覆盖 URL 或超时：

```bash
cmake .. -DXJ_VULKAN_SDK_URL=<url> -DXJ_VULKAN_DOWNLOAD_TIMEOUT=3600
```

### 开发方向

- 新组件：放在 `Core/Public/ECS/Component/`
- 新材质系统：继承 `XJMaterialSystem`
- 新资产导入器：放在 `Core/Public/Asset/Importer/`
- 新场景格式：扩展 `XJSceneAssetSerializer`

## 🐛 已知问题

1. 高层资源所有权路径仍需继续审查
2. macOS / MoltenVK 覆盖测试仍然有限
3. 编辑器面板体系还在持续补全中

## 🗺️ 路线图

### 短期

- [ ] 完善编辑器面板（Hierarchy / Inspector / Stats）
- [x] ~~完善 Vulkan Sampler 所有权与 RenderContext 销毁顺序~~
- [ ] 审查剩余高层资源所有权路径
- [ ] 增加核心系统单元测试
- [ ] 补充 API 文档

### 中期

- [ ] PBR 材质系统
- [ ] 阴影映射
- [ ] 后处理效果
- [ ] 场景编辑能力增强

## 📝 许可证

本项目基于 MIT License 开源，详见 [LICENSE](LICENSE)。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request。

如需英文文档，请访问 [README.md](README.md)。
