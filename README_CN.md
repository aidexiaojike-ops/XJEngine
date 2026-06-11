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
| **ECS 架构** | 基于 EnTT 的高性能实体组件系统 |
| **事件驱动系统** | 完整的窗口、鼠标、键盘事件处理 |
| **模块化材质系统** | 可扩展的纹理、采样器、UBO 管线 |
| **Unlit 材质系统** | Frame UBO、材质参数 UBO、纹理混合与动态描述符池扩容 |
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
- Vulkan SDK
- CMake 3.10+
- 支持 C++17 的编译器

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
- **场景系统**：`.xjscene` -> `XJSceneAssetSerializer` -> `XJSceneInstantiator` -> ECS 实体
- **场景切换**：支持多个 `.xjscene` 文件，通过 ECS 生命周期创建/销毁实体
- **注册表**：`XJAssetRegistry` 用于持久化资产句柄与元数据
- **引导程序**：`XJAssetBootstrap` 管理默认资产注册和场景创建
- **运行时工具**：`XJSceneRuntimeUtil` 提供主摄像机查找等运行时辅助功能

### 编辑器架构（MVVM）

- **Controllers**：`XJEditorSceneController`（场景加载/保存/切换）+ `XJEditorCameraManager`（视口摄像机绑定）+ `XJEditorSceneAssetDropController`（资产拖放到场景）+ `XJEditorExternalDropController`（OS文件拖入）
- **Services**：`XJEditorSceneService`（Controller 与 ECS 之间的桥接层）
- **ViewModels**：`XJEditorSceneViewModel`、`XJEditorSelection`（UI 面板读取快照，写入请求）
- **数据流**：`UI Panel → Request → Controller → Service → ECS`

### 主要模块

- **材质系统**：`XJBaseMaterialSystem`、`XJUnlitMaterialSystem`
- **摄像机系统**：`XJCameraController`（Core/Camera）、`XJCameraSystem`（ECS 适配）
- **资产系统**：`XJModelImporter`、`XJTextureImporter`、`XJAssetRegistry`、`XJAssetRegistryScanner`、`XJAssetBootstrap`、`XJSceneRuntimeUtil`、`XJMeshAssetLoader`
- **编辑器系统**：`XJEditorSceneController`、`XJEditorCameraManager`、`XJEditorSceneService`、`XJUIContext`、`XJEditorRenderer`、`XJEditorUILayer`、编辑器面板

## 🛠️ 构建

### Windows

```bash
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

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
│   ├── Public/Camera/       # 摄像机模块（独立于 ECS）
│   ├── Public/Asset/        # Asset 层（CPU）
│   │   ├── Importer/        # 模型/纹理/材质导入器
│   │   ├── Loader/          # 资产加载器
│   │   ├── Serialization/   # 场景序列化（.xjscene）
│   │   ├── Instantiation/   # 场景实例化器
│   │   └── Register/        # 资产引导注册/扫描
│   ├── Public/Render/       # 渲染接口
│   └── Public/Render/Resource/ # GPU 资源
├── Platform/                # Vulkan、GLFW、External
├── Editor/                  # ImGui 编辑器（MVVM 架构）
│   ├── Public/Controllers/  # Camera/Scene 控制器
│   └── Public/Services/     # 编辑器服务层
├── Src/                     # 应用入口
├── Resource/                # Shader、Mesh、Scenes、Config
└── bin/                     # 运行时输出
```

## 🎮 使用

### Unlit 材质示例

```cpp
XJ::XJUnlitMaterial* unlitMat = XJ::XJMaterialFactory::GetInstance()->CreateMaterial<XJ::XJUnlitMaterial>();
unlitMat->XJSetBaseColorA(glm::vec3(1.0f, 0.0f, 0.0f));
unlitMat->XJSetBaseColorB(glm::vec3(0.0f, 0.0f, 1.0f));
unlitMat->XJSetMixValue(0.5f);
```

### 编辑器 UI 集成

```cpp
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

### 开发方向

- 新组件：放在 `Core/Public/ECS/Component/`
- 新材质系统：继承 `XJMaterialSystem`
- 新资产导入器：放在 `Core/Public/Asset/Importer/`
- 新场景格式：扩展 `XJSceneAssetSerializer`

## 🐛 已知问题

1. 内存管理和资源释放仍有进一步优化空间
2. macOS / MoltenVK 覆盖测试仍然有限
3. 编辑器面板体系还在持续补全中

## 🗺️ 路线图

### 短期

- [ ] 完善编辑器面板（Hierarchy / Inspector / Stats）
- [ ] 优化资源生命周期管理
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
