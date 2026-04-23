# XJEngine

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-brightgreen)](https://github.com/aidexiaojike-ops/XJEngine)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.2+-orange.svg)](https://www.vulkan.org/)
[![CMake](https://img.shields.io/badge/CMake-3.10+-yellow.svg)](https://cmake.org/)

**XJEngine** is a lightweight modern game engine built with Vulkan and ECS architecture. The engine focuses on high-performance graphics rendering, providing a flexible material system, event-driven architecture, and modular rendering pipeline.

**XJEngine** 是一个基于 Vulkan 的现代化实时渲染引擎，采用实体组件系统（ECS）架构设计。引擎专注于高性能图形渲染，提供灵活的材质系统、事件驱动架构和模块化的渲染管线。

## ✨ Key Features

| Feature | Description |
|---------|-------------|
| **Vulkan Renderer** | Modern graphics API with multi-platform support, GPU-driven rendering pipeline |
| **ECS Architecture** | High-performance Entity Component System using EnTT library |
| **Event Driven System** | Complete input handling for window, mouse, keyboard events |
| **Modular Material System** | Extensible material pipeline with textures, samplers and uniform buffers |
| **Unlit Material System** | Dedicated unlit shader pipeline with material parameter UBO and texture support |
| **Dynamic Instancing** | Support for large-scale entity rendering with dynamic uniform buffers |
| **Camera Controller** | Orbit and free camera modes with intuitive mouse interaction |
| **Multisampling Anti-aliasing** | MSAA support for improved visual quality |
| **Depth Testing** | Complete depth buffer management |
| **Shader Compilation** | Automatic GLSL to SPIR-V compilation at build time |
| **Resource Management** | Automatic resource copying to runtime directory |

## 📋 Table of Contents
- [Features](#-key-features)
- [Architecture](#-engine-architecture)
- [Quick Start](#-quick-start)
- [Building](#-building)
- [Project Structure](#-project-structure)
- [Usage](#-usage)
- [Development](#-development)
- [Performance](#-performance)
- [Known Issues](#-known-issues)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)
- [License](#-license)

## 🚀 Quick Start

Get XJEngine running in under 5 minutes:

```bash
# Clone the repository
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine

# Create build directory
mkdir build && cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run the engine
cd ../bin
./XJEngine
```

**Prerequisites:**
- [Vulkan SDK](https://vulkan.lunarg.com/) installed
- CMake 3.10+ and C++17 compatible compiler

## 🏗️ Engine Architecture

```
+-------------------+
|    Application    |
+-------------------+
          |
          v
+-------------------+
|       Scene       |
+-------------------+
          |
          v
+-------------------+
|        ECS        |
| Entity Component  |
| System            |
+-------------------+
          |
          v
+-------------------+
|     Renderer      |
+-------------------+
          |
          v
+-------------------+
|      Vulkan       |
+-------------------+
```

### Rendering Pipeline
```
Scene
 ↓
CameraSystem
 ↓
MaterialSystem
 ↓
RenderTarget
 ↓
CommandBuffer
 ↓
Swapchain
```

### 🏛️ Technical Architecture Details

#### **Vulkan Rendering Pipeline**
- **Swapchain Management**: Automatic recreation on window resize
- **Frame Buffer Management**: Multi-buffer support with depth and color attachments
- **Command Buffer Pooling**: Efficient command buffer allocation and reuse
- **Synchronization**: Semaphores and fences for proper GPU synchronization
- **Render Pass Design**: Configurable attachments and subpasses for flexible rendering

#### **ECS Implementation**
- **Entity Management**: Lightweight entity handles with automatic lifetime tracking
- **Component Storage**: Dense array storage for optimal cache performance
- **System Scheduling**: Flexible system registration and execution order
- **XJMaterialSystem Base Class**: Dedicated base class for material systems with helper methods for device, scene, and camera matrix access
- **Query System**: Efficient entity queries based on component composition

#### **Material System**
- **XJMaterialSystem Base Class**: Provides helper methods (`XJGetDevice`, `XJGetProjMat`, `XJGetViewMat`, `XJGetScene`) for material systems
- **Base Material System**: Dynamic uniform buffer instancing with global/per-instance UBOs
- **Unlit Material System**: Dedicated unlit pipeline with frame UBO, material parameter UBO, and combined image samplers
- **Dynamic Descriptor Pool**: Automatic expansion of material descriptor sets on demand (up to 2048)
- **Texture Management**: Per-material texture views with sampler state
- **Push Constants**: `ModelPC` struct for per-draw model and normal matrix updates
- **Shader Pipeline**: SPIR-V shader compilation and pipeline state management

#### **Event System**
- **Event Types**: Window, keyboard, mouse, and custom events
- **Event Dispatcher**: Efficient event routing and handling
- **Observer Pattern**: Flexible callback registration for event processing
- **Thread Safety**: Safe event handling in multi-threaded scenarios

## 🛠️ System Requirements

### Development Environment
- **CMake** 3.10 or higher
- **C++17** compatible compiler (MSVC, GCC, Clang)
- **Vulkan SDK** 1.2 or higher

### Supported Platforms
- **Windows** (10/11) with Visual Studio 2019/2022
- **Linux** (Ubuntu 20.04+, Fedora, etc.) with GCC/Clang
- **macOS** (10.15+) with Xcode/Clang (Vulkan via MoltenVK)

### Dependencies
- **GLFW**: Window and input management
- **EnTT**: High-performance ECS library
- **GLM**: Mathematics library for graphics
- **spdlog**: Fast logging library
- **stb_image**: Image loading library
- **Dear ImGui**: Debug UI (optional)

## 🚀 Building

### Windows
```bash
# Clone repository
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine

# Create build directory
mkdir build && cd build

# Configure CMake (ensure VULKAN_SDK environment variable is set)
cmake .. -G "Visual Studio 17 2022" -A x64

# Build using CMake
cmake --build . --config Release
```

### Linux
```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt install build-essential cmake libglfw3-dev libvulkan-dev

# Clone and build
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### macOS
```bash
# Install dependencies using Homebrew
brew install cmake glfw vulkan-headers

# Clone and build
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Build Options
| Option | Description | Default |
|--------|-------------|---------|
| `-DXJ_BUILD_TESTS=ON` | Build unit tests | `OFF` |
| `-DXJ_BUILD_EXAMPLES=ON` | Build example applications | `OFF` |
| `-DXJ_ENABLE_VALIDATION=ON` | Enable Vulkan validation layers | `OFF` |
| `-DXJ_USE_IMGUI=ON` | Enable Dear ImGui integration | `ON` |

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
│   │   │   │   ├── XJTransformComponent.h
│   │   │   │   └── Material/       # 材质组件
│   │   │   │       ├── XJBaseMaterialComponent.h
│   │   │   │       └── XJUnlitMaterialComponent.h
│   │   │   └── System/             # 具体系统
│   │   │       ├── XJMaterialSystem.h       # 材质系统基类
│   │   │       ├── XJBaseMaterialSystem.h
│   │   │       ├── XJUnlitMaterialSystem.h
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
│   ├── Shader/             # GLSL 着色器 (BaseVertex, Descriptor, Unlit)
│   ├── Texture/            # 纹理图像
│   ├── Mesh/               # 网格数据
│   └── Config/             # 配置文件
│
├── bin/                    # 运行时输出目录（构建后生成）
│   ├── Resource/           # 复制的资源文件
│   └── log/                # 运行时日志输出
│
└── build/                  # 构建目录（临时）
```

## ⚡ Performance Characteristics

XJEngine is designed for high-performance real-time rendering with the following optimizations:

### **Rendering Performance**
- **GPU-Driven Pipeline**: Minimal CPU overhead with efficient command buffer management
- **Instance Rendering**: Support for thousands of entities with dynamic uniform buffers
- **Efficient Memory Usage**: Proper buffer alignment and memory pooling
- **Multi-threaded Resource Loading**: Asynchronous texture and mesh loading

### **ECS Performance**
- **Cache-Friendly Layout**: Components stored in contiguous arrays for optimal cache performance
- **Fast Entity Queries**: O(1) entity lookups and efficient component iteration
- **Minimal Overhead**: Lightweight entity handles and efficient component storage

### **Vulkan Optimizations**
- **Pipeline State Caching**: Reusable pipeline states to reduce driver overhead
- **Descriptor Set Management**: Efficient descriptor allocation and binding
- **Memory Coalescing**: Optimal buffer and image memory allocation
- **Synchronization Minimization**: Careful use of barriers and synchronization primitives

## 🎮 Usage

### Basic Example
Check the `XJEngineApp` class in `Src/main.cpp` to learn how to:
1. Configure application settings (window size, title)
2. Initialize render context and render target
3. Create scene entities and components
4. Set up event handlers
5. Implement update and render loops

### Creating Entities
```cpp
XJ::XJEntity* entity = scene->CreateEntity("Cube");
auto& transform = entity->GetComponent<XJ::XJTransformComponent>();
auto& material = entity->AddComponent<XJ::XJBaseMaterialComponent>();

transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
transform.UpdateModelMatrix();
```

### Using Unlit Materials
```cpp
// Create unlit material with custom parameters
XJ::XJUnlitMaterial* unlitMat = XJ::XJMaterialFactory::GetInstance()->CreateMaterial<XJ::XJUnlitMaterial>();
unlitMat->XJSetBaseColorA(glm::vec3(1.0f, 0.0f, 0.0f));
unlitMat->XJSetBaseColorB(glm::vec3(0.0f, 0.0f, 1.0f));
unlitMat->XJSetMixValue(0.5f);

// Set texture
unlitMat->XJSetTextureView(0, texture, sampler);

// Add to entity
auto& unlitComp = entity->AddComponent<XJ::XJUnlitMaterialComponent>();
unlitComp.AddMesh(mesh, unlitMat);
```

### Using Multiple Material Instances
```cpp
// Create different material instances for varied rendering
auto kMaterialA = XJ::XJMaterialFactory::GetInstance()->CreateMaterial<XJ::XJBaseMaterial>();
auto kMaterialB = XJ::XJMaterialFactory::GetInstance()->CreateMaterial<XJ::XJBaseMaterial>();

// Assign different materials to entities
for (auto& entity : entities) {
    auto& matComp = entity->AddComponent<XJ::XJBaseMaterialComponent>();
    matComp.AddMesh(mMesh, index == 0 ? kMaterialA : kMaterialB);
    index = (index + 1) % 2;
}
```

### Event Handling
```cpp
mObserver->OnEvent<XJ::XJFrameBufferResizeEvent>([this](const XJ::XJFrameBufferResizeEvent& event) {
    spdlog::info("Window resized: {}x{}", event.mWidth, event.mHeight);
    mRenderTarget->SetExtent({event.mWidth, event.mHeight});
});
```

### Advanced Example: Camera Controller
```cpp
// Create a camera entity
XJ::XJEntity* camera = scene->CreateEntity("MainCamera");
camera->AddComponent<XJ::XJCameraComponent>();
auto& cameraComp = camera->GetComponent<XJ::XJCameraComponent>();
cameraComp.XJSetCameraMode(XJ::CameraMode::Orbit);
cameraComp.XJSetRadius(5.0f);
cameraComp.XJSetTarget(glm::vec3(0.0f, 0.0f, 0.0f));

// Set camera as active camera in render target
mRenderTarget->XJSetCamera(camera);
```

## 🔧 Development

### Adding New Components
1. Create component header file in `Core/Public/ECS/Component/`
2. Inherit from `XJComponent` base class
3. Implement component logic in `Core/Private/ECS/Component/` (if needed)
4. Add component to entities using `AddComponent<T>()`

### Creating New Material Systems
1. Inherit from `XJMaterialSystem` base class in `Core/Public/ECS/System/XJMaterialSystem.h`
2. Implement `OnInit`, `OnRender`, and `OnDestroy` methods
3. Define descriptor set layouts, pipeline layout, and pipeline in `OnInit`
4. Use helper methods `XJGetDevice()`, `XJGetProjMat()`, `XJGetViewMat()` for camera data
5. Register system in render target using `AddMaterialSystem<T>()`

### Shader Development
- Shader files are located in `Resource/Shader/`
- Automatic compilation from GLSL to SPIR-V at build time
- Use `XJ_RES_SHADER_DIR` macro to reference shader paths

### Code Style Guidelines
- **Naming**: PascalCase for classes, camelCase for methods and variables
- **Headers**: Include guards with `#ifndef XJ_..._H` pattern
- **Documentation**: Document public APIs with clear comments
- **Error Handling**: Use spdlog for logging and proper Vulkan error checking

### Building Custom Applications
1. Create a new application class inheriting from `XJ::XJApplication`
2. Override virtual methods: `OnInit`, `OnSceneInit`, `OnUpdate`, `OnRender`, `OnDestroy`
3. Link against XJEngine libraries (Platform and Core)
4. Use CMake to integrate with the engine build system

## 🐛 Known Issues

1. **Vulkan Validation Layer Errors**: Push constant range and descriptor set synchronization issues need further investigation
2. **Memory Management**: Some resource cleanup logic needs improvement
3. **Platform Support**: Limited testing on macOS with MoltenVK

## 🗺️ Roadmap

### Short-term (Next 3 months)
- [ ] Fix Vulkan validation layer errors
- [ ] Improve memory management and resource cleanup
- [ ] Add unit tests for core systems
- [ ] Enhance documentation with API references
- [ ] Optimize rendering performance for large scenes

### Medium-term (3-6 months)
- [ ] Add PBR material system
- [ ] Implement shadow mapping
- [ ] Add post-processing effects (bloom, FXAA, etc.)
- [ ] Support for skeletal animation
- [ ] Scene serialization/deserialization

### Long-term (6+ months)
- [ ] Editor tools and scene editor
- [ ] Physics integration
- [ ] Networking support
- [ ] VR/AR support
- [ ] Mobile platform support (Android/iOS)

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details.

### How to Contribute
1. **Report Bugs**: Open an issue with detailed bug reports
2. **Request Features**: Suggest new features or improvements
3. **Submit Code**: Fork the repository and submit pull requests
4. **Improve Documentation**: Help improve documentation and examples

### Code Review Guidelines
- Ensure code follows the project's style guidelines
- Add tests for new functionality
- Update documentation for API changes
- Keep pull requests focused and manageable

## 📧 Contact

- **GitHub Issues**: [https://github.com/aidexiaojike-ops/XJEngine/issues](https://github.com/aidexiaojike-ops/XJEngine/issues)
- **Project Discussions**: Use GitHub Discussions for questions and ideas

## 🙏 Acknowledgments

- **Vulkan**: Khronos Group for the Vulkan API
- **EnTT**: skypjack for the amazing ECS library
- **GLFW**: The GLFW team for window and input management
- **GLM**: g-truc for the mathematics library
- **spdlog**: gabime for the fast logging library
- **stb**: Sean Barrett for the single-file libraries

---

**XJEngine** - Built for high-performance real-time rendering 🚀

**XJEngine** - 为高性能实时渲染而生 🚀