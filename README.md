# XJEngine

**XJEngine** 是一个基于 Vulkan 的现代化实时渲染引擎，采用实体组件系统（ECS）架构设计。引擎专注于高性能图形渲染，提供灵活的材质系统、事件驱动架构和模块化的渲染管线。

## ✨ 特性

### 🎮 核心架构
- **实体组件系统（ECS）**：使用 EnTT 库实现的高性能 ECS 架构
- **事件驱动设计**：完整的事件系统支持窗口、鼠标、键盘等输入事件
- **模块化系统**：可扩展的材质系统、摄像机控制系统等

### 🖼️ 图形渲染
- **Vulkan 后端**：现代图形 API，支持多平台高性能渲染
- **材质系统**：灵活的材质管线，支持纹理、采样器和统一缓冲区
- **动态统一缓冲区**：支持大量实体的实例化渲染
- **多重采样抗锯齿**：支持 MSAA 提升图像质量
- **深度测试**：完整的深度缓冲管理

### 🎥 摄像机系统
- **轨道模式**：围绕目标点旋转的摄像机
- **自由模式**：独立控制位置和朝向
- **鼠标交互**：支持鼠标滚轮缩放、拖动旋转
- **透视投影**：可配置的视野、宽高比和裁剪面

### 📁 资源管理
- **Shader 编译**：构建时自动编译 GLSL 到 SPIR-V
- **纹理加载**：支持常见图像格式
- **网格系统**：支持顶点和索引缓冲
- **资源目录**：自动复制纹理、网格、配置等资源到运行目录

## 🛠️ 系统要求

### 开发环境
- **CMake** 3.10 或更高版本
- **C++17** 兼容编译器
- **Vulkan SDK** 1.2 或更高版本

### 依赖库
- **GLFW**：窗口和输入管理
- **EnTT**：ECS 架构
- **GLM**：数学库
- **spdlog**：日志系统
- **stb_image**：图像加载
- **Dear ImGui**：调试界面（可选）

## 🚀 构建指南

### Windows (Visual Studio)
```bash
# 克隆仓库
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine

# 创建构建目录
mkdir build && cd build

# 配置 CMake（确保 Vulkan SDK 已安装并设置 VULKAN_SDK 环境变量）
cmake .. -G "Visual Studio 17 2022" -A x64

# 打开生成的解决方案文件进行构建
# 或使用 CMake 构建
cmake --build . --config Release
```

### Linux/macOS
```bash
# 克隆仓库
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建
make -j$(nproc)
```

## 📁 项目结构

```
XJEngine/
├── Core/                    # 引擎核心模块
│   ├── Public/             # 公共头文件
│   │   ├── ECS/            # 实体组件系统
│   │   │   ├── XJEntity.h          # 实体类
│   │   │   ├── XJComponent.h       # 组件基类
│   │   │   ├── XJSystem.h          # 系统基类
│   │   │   ├── Component/          # 具体组件
│   │   │   │   ├── XJCameraComponent.h
│   │   │   │   └── XJTransformComponent.h
│   │   │   └── System/             # 具体系统
│   │   │       ├── XJBaseMaterialSystem.h
│   │   │       └── XJCameraControllerSystem.h
│   │   └── Render/         # 渲染相关
│   │       ├── XJRenderTarget.h
│   │       └── XJRenderer.h
│   └── Private/            # 私有实现
│
├── Platform/               # 平台相关代码
│   ├── External/           # 第三方库
│   ├── Public/             # 平台公共接口
│   │   ├── Graphic/        # 图形 API 封装
│   │   ├── Edit/           # 编辑器和工具
│   │   └── Event/          # 事件系统
│   └── Private/            # 平台具体实现
│
├── Src/                    # 应用程序源代码
│   └── main.cpp            # 主程序入口
│
├── Resource/               # 资源文件
│   ├── Shader/             # GLSL 着色器
│   ├── Texture/            # 纹理图像
│   ├── Mesh/               # 网格数据
│   └── Config/             # 配置文件
│
├── bin/                    # 运行时输出目录（构建后生成）
│   └── Resource/           # 复制的资源文件
│
└── build/                  # 构建目录（临时）
```

## 🎮 使用方法

### 基本示例
查看 `Src/main.cpp` 中的 `XJEngineApp` 类，了解如何：
1. 配置应用程序设置（窗口大小、标题）
2. 初始化渲染上下文和渲染目标
3. 创建场景实体和组件
4. 设置事件处理程序
5. 实现更新和渲染循环

### 创建实体
```cpp
XJ::XJEntity* entity = scene->CreateEntity("Cube");
auto& transform = entity->GetComponent<XJ::XJTransformComponent>();
auto& material = entity->AddComponent<XJ::XJBaseMaterialComponent>();

transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
transform.UpdateModelMatrix();
```

### 事件处理
```cpp
mObserver->OnEvent<XJ::XJFrameBufferResizeEvent>([this](const XJ::XJFrameBufferResizeEvent& event) {
    spdlog::info("窗口大小改变: {}x{}", event.mWidth, event.mHeight);
    mRenderTarget->SetExtent({event.mWidth, event.mHeight});
});
```

## 🔧 开发说明

### 添加新组件
1. 在 `Core/Public/ECS/Component/` 中创建组件头文件
2. 继承 `XJComponent` 基类
3. 在 `Core/Private/ECS/Component/` 中实现组件逻辑（如果需要）
4. 在实体上使用 `AddComponent<T>()` 添加组件

### 添加新材质系统
1. 继承 `XJMaterialSystem` 基类
2. 实现 `OnInit`、`OnRender`、`OnDestroy` 方法
3. 在渲染目标中使用 `AddMaterialSystem<T>()` 注册系统

### Shader 开发
- 着色器文件位于 `Resource/Shader/`
- 构建时自动编译为 SPIR-V
- 使用 `XJ_RES_SHADER_DIR` 宏引用着色器路径

## 🐛 已知问题

1. **窗口调整大小导致段错误**：在某些情况下，窗口调整大小时可能触发段错误，正在调查中
2. **Vulkan 验证层错误**：存在推送常量范围和描述符集同步问题，需要进一步修复
3. **内存管理**：某些资源释放逻辑需要完善

## 📝 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！在提交代码前，请确保：
1. 代码符合项目的代码风格
2. 添加相应的测试（如果适用）
3. 更新相关文档

## 📧 联系

如有问题或建议，请通过 GitHub Issues 联系。

---

**XJEngine** - 为高性能实时渲染而生 🚀