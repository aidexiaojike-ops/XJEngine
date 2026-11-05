# XJEngine

<div align="center">

**[English](README.md)** &nbsp;&nbsp;|&nbsp;&nbsp; **简体中文**

</div>

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-brightgreen)](https://github.com/aidexiaojike-ops/XJEngine)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Vulkan](https://img.shields.io/badge/Renderer-Vulkan-orange.svg)](https://www.vulkan.org/)
[![CMake](https://img.shields.io/badge/CMake-3.28+-yellow.svg)](https://cmake.org/)

XJEngine 是一个基于 Vulkan 和 EnTT 的实验性 C++20 游戏引擎与编辑器，包含 Schema 驱动的材质管线、资产与场景持久化、内置脚本语言和 ImGui 编辑器。

**当前构建路径面向 Windows。** SDK 配置与运行时 DLL 处理依赖 Windows Vulkan SDK 目录布局，Linux 和 macOS 尚不能直接按现有流程构建。Transform Gizmo 已支持**手柄拾取、悬停高亮和单轴移动拖拽**，旋转与缩放拖拽尚未实现。

## 目录

- [核心特性](#核心特性)
- [快速开始](#快速开始)
- [构建配置](#构建配置)
- [编辑器工作流](#编辑器工作流)
- [引擎架构](#引擎架构)
- [项目结构](#项目结构)
- [API 示例](#api-示例)
- [测试](#测试)
- [性能与诊断](#性能与诊断)
- [当前限制](#当前限制)
- [路线图](#路线图)
- [开发与贡献](#开发与贡献)
- [许可证](#许可证)

## 核心特性

| 领域 | 已实现能力 |
|------|------------|
| **Vulkan 渲染** | Scene/Game 离屏目标、深度缓冲、构建期 GLSL → SPIR-V、结构化 Swapchain acquire/present 与窗口尺寸变化处理 |
| **ECS 与变换** | EnTT 实体/组件、稳定 UUID 查找、支持父子层级的 Local/World 变换、带校验的四元数/TRS 编辑 |
| **材质与光照** | JSON Shader Schema、SPIR-V 反射、Unlit/Lit Surface 材质、方向光/点光/聚光灯、材质与管线缓存 |
| **资产与场景** | CPU Asset / GPU Resource 分层、glTF 2.0 导入、Submesh 材质槽、AABB、`.xjmeta` 元数据、场景格式 v4 与 v2/v3 迁移 |
| **内置脚本** | `.xjs` 词法/语法/编译管线、字节码验证、执行限制、带类型的值、宿主服务、ECS 生命周期与运行时槽同步 |
| **编辑器面板** | Hierarchy、Inspector、Content Browser、Console、Scene/Game Preview、Docking 与独立 ImGui 窗口 |
| **Local/World 编辑** | Scene 工具栏坐标空间切换、Inspector 数值编辑、拒绝反馈、基于快照的 Undo/Redo |
| **Transform Gizmo 交互** | 九部件 Local/World Gizmo、近似固定屏幕尺寸、手柄拾取、悬停/活动高亮，以及可提交/取消的单轴移动拖拽；旋转与缩放手柄暂不可编辑 |
| **Scene 辅助显示** | 自适应世界网格、灯光线框、AABB + 三角形拾取、以命中表面点为轨道旋转中心 |
| **Play 模式** | 运行时场景克隆、Play/Pause/Resume/Stop、运行时检查与脚本槽修改，不直接编辑源场景 |
| **诊断** | 基于 spdlog 的 Console、视口性能快照、帧历史与材质上传统计 |

## 快速开始

### 前置要求

- Windows 10/11 x64，支持 Vulkan 的 GPU 与已安装的显卡驱动。
- 支持 C++20 的编译器。以下命令推荐使用安装了**“使用 C++ 的桌面开发”**工作负载的 Visual Studio 2022。
- **CMake 3.28 或更新版本**。顶层文件仍声明 3.10，但仓库内的 EnTT 依赖实际要求 3.28。
- Windows Vulkan SDK，或允许 CMake 自动下载 SDK 的网络环境。其他依赖已包含在 `ThirdParty/` 中。

在准备克隆项目的目录打开 PowerShell，执行：

```powershell
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\bin\XJEngine.exe
```

如果 `build/` 已使用其他生成器，请沿用该生成器或另选构建目录，不要复用不兼容的 CMake 缓存。Visual Studio 属于多配置生成器，应通过 `--config Release` 选择配置，而非 `CMAKE_BUILD_TYPE`。

首次配置可能下载较大的 Vulkan SDK 安装包，耗时数分钟。SDK 不能替代 GPU 厂商的显卡驱动。

## 构建配置

### Vulkan SDK 解析

[`cmake/SetupVulkanSDK.cmake`](cmake/SetupVulkanSDK.cmake) 按以下顺序查找：

1. `VULKAN_SDK` 环境变量指向的有效 SDK。
2. `ThirdParty/VulkanSDK` 本地缓存，可通过 `XJ_VULKAN_SDK_LOCAL_DIR` 覆盖。
3. 下载并在本地安装 Windows LunarG Vulkan SDK **1.3.283.0**。

解析后的 SDK 必须包含 `Include/vulkan/vulkan.h`、`Lib/vulkan-1.lib`、`Bin/glslc.exe` 和 `Dll/vulkan-1.dll`。离线构建前应先安装完整 SDK，并设置 `VULKAN_SDK` 或本地缓存路径。

| 配置项 | 用途 | 默认值 |
|--------|------|--------|
| `XJ_BUILD_TESTS` | 构建并注册第一方 CTest 测试 | `OFF` |
| `XJ_VULKAN_SDK_LOCAL_DIR` | 本地 Windows SDK 缓存 | `ThirdParty/VulkanSDK` |
| `XJ_VULKAN_SDK_URL` | SDK 安装包/归档下载地址 | Windows SDK 1.3.283.0 安装包 |
| `XJ_VULKAN_DOWNLOAD_TIMEOUT` | 下载超时，单位为秒 | `1800` |

例如，增加下载超时：

```powershell
cmake -S . -B build -DXJ_VULKAN_DOWNLOAD_TIMEOUT=3600
```

`XJ_BUILD_EXAMPLES`、`XJ_ENABLE_VALIDATION` 和 `XJ_USE_IMGUI` **不是**当前项目提供的构建选项。

### 输出与项目路径

| 路径 | 用途 |
|------|------|
| `bin/XJEngine.exe` | 编辑器可执行文件；Debug 与 Release 共用 `bin/` |
| `Resource/Shader/*.spv` | Shader 构建产物，与源 GLSL 和 Shader 资产同目录 |
| `Resource/` | 开发阶段编辑器与运行时直接读取的源资产 |
| `Resource/Config/AssetRegistry.json` | 项目资产注册表 |
| `Resource/Scenes/Default.xjscene` | 默认场景 |
| `Resource/Config/EditorUI.json` | 编辑器 UI 初始配置模板 |
| `bin/Saved/Config/EditorUI.json` | 用户编辑器 UI 状态 |
| `bin/Saved/Config/imgui.ini` | 用户 ImGui 布局 |

资产**不会复制到 `bin/Resource`**。仅复制可执行文件不能得到独立游戏发行包，Export/Package 尚未实现。

编辑器项目根目录的优先级为：`--project` > `XJ_PROJECT_ROOT` 环境变量 > CMake 记录的源码根目录。参数应指向**包含 `Resource/` 的目录**，而非 `Resource/` 本身：

```powershell
.\bin\XJEngine.exe --project "E:\Projects\MyXJProject"
```

Windows 下用户 UI 状态跟随可执行文件目录，不依赖启动工作目录。移动源项目后，应重新构建或显式指定新的项目根目录。

## 编辑器工作流

### 场景与资产编辑

1. 在 **Hierarchy** 中选择实体，或在 **Scene Preview** 中点击场景几何体。拾取先进行世界空间 AABB 粗筛，再执行 CPU 射线-三角形检测。
2. 在 **Inspector** 中编辑组件值、材质参数、相机、灯光和脚本槽。Hierarchy 支持实体创建、删除与重命名。
3. 在 **Content Browser** 中管理资产，将 Mesh 资产拖入 Scene Preview 可创建实体。外部文件拖放由独立的编辑器导入控制器处理。
4. 场景保存为 `.xjscene`。场景和材质编辑使用基于快照的 Undo/Redo，最多保留 100 条历史。

### Local/World 变换编辑

- 点击 Scene 工具栏的 **Local/World** 按钮切换 Inspector 坐标空间与 Gizmo 朝向。仅 Edit 模式允许切换，Gizmo 拖动期间禁用；切换不修改场景数据，也不产生 Undo。
- Transform 快照使用双精度，按字段提交请求，保留未编辑的本地分量。
- 世界旋转使用归一化四元数独立于缩放合成。世界缩放是非负的**有损缩放**（世界矩阵轴长），不是带符号的本地缩放；世界缩放编辑保留本地反射符号。
- Service 先在临时候选对象上验证，再应用编辑。拒绝时显示对应实体/空间的原因，不修改场景、dirty 标记或 Undo/Redo 历史；无变化编辑同样不创建历史。
- 组件复制粘贴始终使用 **Local TRS**。Play/Pause 运行时检查中的 Transform 仍只读。

### Transform Gizmo 交互

在可编辑场景中选中具有有效 Transform 的实体后，**Scene Preview** 显示统一的九部件 Gizmo：

- 三个移动箭头、三个旋转环和三个缩放方块，按 X/Y/Z 轴着色。
- 原点跟随实体的**世界位置**。**Local** 使用实体的纯世界旋转；**World** 对齐全局 XYZ 轴。物体缩放不会拉伸 Gizmo 几何。
- 根据投影与视口尺寸计算统一的世界尺度，目标为近似 **110 像素的屏幕半径**，而非固定世界尺寸。
- 使用无光照纯色、双面三角形，不启用深度测试或深度写入，因此不会被场景几何体遮挡。
- Play/Pause、只读/运行时检查、无效选择或缺少 Transform 时隐藏；帧/投影输入无效时跳过绘制。Game Preview 不挂载该通道。

移动选中实体的操作：

1. 选择 **Local** 或 **World**，将鼠标悬停在手柄上，对应部件变为**黄色**。
2. 在移动箭头上按住**鼠标左键**，沿对应轴拖动，活动手柄变为**橙色**。开始时不要按住 Alt 或鼠标右键/中键，以免与相机手势冲突。
3. 松开左键提交。整次拖动仅在最终本地位置确实变化时产生**一条 Undo** 并标记场景 dirty；中间预览只刷新 UI，不写入历史或修改 dirty，无变化操作也不创建历史。
4. 按 **Escape** 取消，恢复原本地位置，不新增历史或改变 dirty 标记。应用失焦、弹窗、指针/图像/按钮状态无效、选择/设置失配、相机或图像区域变化也会取消；切换/卸载场景和进入 Play 前同样先取消交互。

- 拖动冻结开始时的相机/投影、移动轴和拖动平面，每次从初始位置计算目标，不累积位移误差。移动轴几乎正对相机时拒绝启动，请按提示调整视角；临时射线-平面求交失败时保持最后有效位置。
- Gizmo 占用输入时阻止场景选择、相机控制和视口资产拖放；占用手势期间产生的普通场景/资产/空间切换请求直接丢弃，不积压到松开后执行，保存也不会持久化未提交的预览。
- 旋转环与缩放方块可以拾取和高亮，点击会被消费，不会穿透选择后方几何体，但**不会修改旋转或缩放**，这些编辑仍需使用 Inspector 数值。吸附、平面/自由移动和 Move/Rotate/Scale 操作切换尚未实现。

### 世界网格与视口显示

- Scene Preview 绘制自适应 **1/2/5 系列格距**的世界网格，最小 **1 米**，包含 X/Z 坐标标签、刻线、格距说明和边缘淡出。
- 网格启用深度测试但不写深度。布局随相机和视口变化；相机姿态无效时跳过网格，不复用缓存视图。
- 世界网格、灯光线框和 Transform Gizmo 均为 Scene 专用辅助显示，Game Preview 不包含这些编辑器通道。
- 当前仅实现 **Final** 视图模式，BaseColor、LinearDepth、WorldNormal 和 Lighting 仍按能力禁用。

### Play 模式与脚本

- Play 将编辑场景克隆为运行时场景；Pause/Resume 控制模拟，Stop 恢复编辑器选择。
- 运行时 Hierarchy/Inspector 读取运行场景快照。可通过运行时请求增删脚本组件/槽、启停与替换脚本；调度器在安全点同步，暂停期间也可处理。
- 脚本输入要求处于 **Playing**，Game 与应用窗口聚焦，且无活动 UI 控件、文本输入或弹窗。按下/释放边沿仅在 Update 可见，重新获得焦点首帧抑制边沿。
- 保存 `.xjs` 后，Content Browser 接收保存结果与编译诊断。磁盘保存成功仍可能存在编译错误，Play 预检会拒绝无效脚本；脚本编辑器仅在收到保存成功响应后清除 dirty 状态。

## 引擎架构

```text
Application -> Scene / ECS -> Camera and Material Systems
            -> Off-screen Render Targets -> Editor Frame Renderer -> Swapchain

UI View -> Request -> Controller / Service -> ECS or render state
        <- ViewModel snapshots <-----------+

File -> Scanner / Importer -> Asset (CPU) -> Factory -> Render Resource (GPU)
```

### ECS、变换与相机

- `XJEntity` 校验场景生命周期 token；Scene 维护 UUID → EnTT 查找索引并拒绝重复 UUID，引擎/编辑器实体使用保留 UUID 区间。
- `XJTransformComponent` 将本地 TRS 私有化，归一化四元数旋转，提供角度制 XYZ 欧拉角接口（`Rx * Ry * Rz`）。带校验的 setter 返回 `XJTransformStatus`，失败写入保持原子性。
- 世界求值检查父链环和非有限数值；以双精度计算后，GPU 与拾取消费端进一步检查 float 可表示性。
- 相机使用世界位置/旋转，本地前方为 `-Z`、上方为 `+Y`。View Matrix 读取不修改 ECS 或轨道参数，Free/Orbit 控制器提交带校验的世界姿态修改。
- 场景格式 **v4** 保存本地 TRS 与相机轨道模式/目标/半径。加载 v2/v3 时将旧 yaw/pitch 相机姿态迁移为父级相对变换，无法表示的姿态会被拒绝。
- `XJSystemScheduler` 提供事务式 Start/Stop、带逐帧上限的固定步进和延迟运行时修改安全点；`XJInput` 提供键盘/鼠标保持、按下、释放快照与轴向查询。

### 渲染与材质

- Vulkan 平台层区分 acquire/present 成功、Swapchain 重建与设备丢失状态。Instance 自动选择最高到 Vulkan 1.3 的 API 版本并传给 ImGui；Sampler 使用 RAII 所有权管理。
- `XJBaseMaterialSystem` 使用动态 Uniform Buffer；`XJSurfaceMaterialSystem` 通过缓存的管线运行时绘制 Schema 驱动的 `XJSurfaceMaterialComponent`。
- JSON `.schema` / `.xjshader` 资产定义参数与绑定；验证、跨阶段 SPIR-V 反射和描述符布局构建将资产连接到运行时参数块与纹理绑定。
- `Unlit.schema` 与 `Lit.schema` 共用 Surface 运行时。Lit 支持基础颜色/Albedo 纹理、高光强度、光泽度与方向光/点光/聚光灯前向光照，尚不是完整 PBR 渲染器。
- 共享 `XJFrameUbo`、`XJLightUbo` 提供相机/帧数据及 **1 个方向光、最多 8 个点光和 8 个聚光灯**。灯光持久化类型、启用状态、颜色、强度、范围和锥角。
- 材质实例 ID 与参数/资源修订号控制逐 frame slot 上传同步；描述符池按需扩容，工厂缓存使用弱引用并复用过期材质槽。

### 资产与编辑器所有权

- Asset 保存 CPU 数据，Factory 创建 GPU 网格、纹理与材质；`.xjmeta` 旁置文件保留资产句柄/类型/导入器信息，注册表隔离持久化句柄与运行时生成句柄。
- glTF 2.0 导入将三角形 primitive 合并进共享缓冲，保留逐 primitive 索引范围、材质槽和包围盒，跳过不支持的非三角形模式；Mesh 保留 CPU 顶点位置/索引用于拾取。
- `XJJsonIO` 提供共享 JSON 读取与原子写入；场景序列化器和实例化器负责 `.xjscene` 资产与 ECS 层级之间的转换。
- `Src/main.cpp` 创建 `XJEditorApplication`；`XJEditorRuntime` 管理工作区、Play Controller、视口系统、帧渲染、输入绑定与 UI Host。
- View 读取 ViewModel 并提交请求/输入快照，Controller/Service 应用修改；`XJEditorTransformToolsController` 发布空间设置与 `XJEditorTransformGizmoViewModel`，Gizmo 渲染系统消费显示快照，并发布实际绘制的 `XJGizmoRenderedFrame` 供交互使用。
- `XJGizmoInteractionController` 在使用绘制帧前验证 epoch、实体、空间和设置修订号；CPU 拾取按绘制倒序检测双面三角形，精确命中优先，并以邻近像素采样辅助细手柄命中。单轴目标经 Scene Controller/Service 的带校验 World Position 预览事务应用，每次拖动统一提交或回滚。
- `XJViewportRenderSurface` 管理离屏渲染目标与 resize 时的延迟描述符释放；`XJEditorLog` 将 spdlog 桥接为按修订号更新的 Console 快照，并裁剪单行日志绘制范围。

### 内置脚本

- 管线：Lexer → Parser → 语义分析 → 编译器 → 字节码验证器 → Runtime。执行受指令预算、调用深度、类型/溢出检查、故障隔离、堆栈跟踪和重入保护约束。
- `XJScriptValue` 支持 bool/int/float/string、`Vec3` 与 `EntityRef`。向量支持算术、标量乘除、分量写入、复合赋值和前后缀增减。
- `XJScriptSystem` 调用 `OnCreate`、`OnUpdate`、`OnFixedUpdate` 和 `OnDestroy`；每实体可挂载多个可启停脚本槽，使用稳定 UUID 与 `[[FieldId("...")]]` 字段覆盖。
- `XJScriptHostNativeInvoker` 注入实体、Transform、输入与日志服务，携带 Owner/槽/帧/阶段上下文，服务保持到 `OnDestroy` 完成。
- Transform API 读写本地位置/欧拉角/缩放并读取世界值；`TrySetWorldPosition`、`TrySetWorldEulerAngles`、`TrySetWorldScale` 报告写入失败且不部分修改状态。支持当前 Owner 或显式 `EntityRef`，`Transform.RotateY` 绕本地 Y 轴旋转。
- `Scene.FindByUUID` 接收十进制或 `0x` 十六进制字符串；`Entity.Self`、`Entity.IsValid`、`Entity.GetUUID` 使用会话隔离句柄并保护失效/跨会话引用，格式有效但未找到实体的 UUID 返回 null。
- 编译、验证与分发共用 Native 签名目录；带类型的值序列化拒绝非有限数值。`EntityRef` 不允许持久化、暴露为 public 字段或保存为非空字节码常量。
- 脚本资产支持编译/缓存；场景保留槽 ID、启用状态、资产引用和字段覆盖。`Log.Info`/`Warn`/`Error` 通过 spdlog 输出，受每槽/全局配额限制。

## 项目结构

```text
XJEngine/
├── Core/                    # ECS、相机、输入、几何、资产、脚本与渲染
│   ├── Public/              # 公共接口
│   └── Private/             # 实现
├── Editor/                  # ImGui 编辑器与 MVVM 应用服务
│   ├── Public/Controllers/  # 场景、资产、相机、变换与显示控制器
│   ├── Public/Services/     # 场景/资产/显示服务
│   ├── Public/Tools/        # 变换类型、网格布局、Gizmo 几何与交互数学
│   ├── Public/UI/           # ViewModel、请求、面板与视口
│   └── Private/             # Controller、Service、UI 与编辑器渲染系统
├── Platform/                # Vulkan、GLFW、事件与平台封装
├── Src/                     # main.cpp 与 XJEditorApplication
├── Resource/                # 开发阶段直接读取的源资产
│   ├── Shader/              # GLSL、生成的 SPIR-V、.schema 与 .xjshader
│   ├── Mesh/                # 模型资产
│   ├── Texture/             # 纹理资产
│   ├── Material/            # .xjmat 材质
│   ├── Script/              # .xjs 示例与元数据
│   ├── Scenes/              # .xjscene 场景
│   └── Config/              # 资产注册表与初始 UI 模板
├── Tests/                   # 第一方 CTest 目标
├── ThirdParty/              # 内置依赖与可选本地 Vulkan SDK 缓存
├── cmake/                   # SDK 配置、Shader 编译与依赖配置
├── bin/                     # 生成的可执行文件、运行时 DLL 与 Saved/ 用户状态
└── build/                   # 生成的构建目录
```

## API 示例

以下为集成代码片段，不是独立程序。需要有效场景与已初始化的渲染上下文，并保证 GPU 资源生命周期不超出渲染宿主。应用入口与编辑器生命周期见 [`Src/main.cpp`](Src/main.cpp) 和 [`Src/XJEditorApplication.cpp`](Src/XJEditorApplication.cpp)。

### 创建实体并编辑 Transform

```cpp
auto* entity = scene.CreateEntityWithTransform("Cube");
auto& transform = entity->GetComponent<XJ::XJTransformComponent>();

const auto status = transform.SetLocalTRSEuler(
    glm::dvec3(0.0), glm::dvec3(0.0, 45.0, 0.0), glm::dvec3(1.0));
if (status != XJ::XJTransformStatus::Success)
    spdlog::error("Cube transform could not be applied");
```

### 创建已初始化 Schema 的 Surface 材质

假设 `entity` 尚无 Surface 材质组件，`mesh`、`defaultTexture` 与 `defaultSampler` 为有效渲染资源，项目资源根目录与 Shader 资产已就绪。

```cpp
auto material = XJ::XJMaterialFactory::GetInstance()->CreateDefaultMaterial(
    defaultTexture, defaultSampler);

if (material && material->SetParameterValue(
        "BaseColor", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)))
{
    auto& component = entity->AddComponent<XJ::XJSurfaceMaterialComponent>();
    for (uint32_t submesh = 0; submesh < mesh->GetSubmeshCount(); ++submesh)
        component.AddMesh(mesh, material, submesh);
}
else
{
    spdlog::error("Surface material runtime parameters are unavailable");
}
```

`CreateDefaultMaterial` 会构建 Unlit Shader 布局与回退纹理绑定；已有 `.xjmat` 资产及其 Shader 应使用 `CreateFromAsset`。单独调用 `CreateMaterial<T>()` 只分配材质，**不会**初始化基于 Schema 的参数布局。

### 脚本示例

以下是 **`.xjs` 语法**，不是 C++ 编译单元：

```cpp
class Rotate : ScriptBehaviour
{
    public:
        [[FieldId("1000000001")]]
        float speed = 180.0f;

        void OnUpdate(float dt)
        {
            Transform.RotateY(speed * dt);
        }
};
```

[`Resource/Script/`](Resource/Script/) 还提供 `PlayerMove.xjs`（键盘移动）、`MoveTarget.xjs`（UUID 查找与跨实体移动）、`WorldPosition.xjs` / `WorldScale.xjs`（检查世界空间写入结果）和 `Lifecycle.xjs`。

## 测试

在仓库根目录启用测试、构建并运行 CTest：

```powershell
cmake -S . -B build -DXJ_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

覆盖范围包括：

- 脚本语言、编译器、字节码验证器/运行时、Native 契约、带类型的值、资产编译/加载、序列化与编辑器脚本操作。
- ECS 脚本生命周期、运行时槽同步、延迟销毁、实体会话隔离、宿主 Transform、输入焦点/阶段规则与日志配额。
- 四元数/TRS 数学、父级继承、失败写入原子性、相机控制/只读查询、世界空间灯光/拾取/包围盒与场景 v2/v3 → v4 迁移。
- Local/World 验收行为、字段隔离、反射/有损缩放、拒绝反馈与 Undo/Redo 保留。
- 材质上传状态和视口请求、能力与统计。
- 无需 GPU 的世界网格布局、Transform Gizmo 几何与帧计算：九个唯一手柄、有限且非退化的三角形列表、几何边界、透视尺寸调整与无效视口高度。
- `XJTransformGizmoGeometryTests` 还验证 X 轴手柄拾取、无效帧拒绝与单轴移动数学：沿 X 轴移动、重复计算不累积、正对视线轴拒绝，以及旋转后的 Local X 跟随其世界方向。

这些测试不能替代手动 Vulkan 验证或视口视觉检查。Gizmo 预览事务、输入占用和取消/生命周期流程仍需手动集成检查；新增交互测试覆盖拾取与移动数学，并非上述端到端行为。

## 性能与诊断

- 视口叠层显示已完成帧耗时/FPS、脚本 CPU 时间与活动实例数、Draw Call 和材质上传，保留 **128 个帧历史采样**；这些快照不是 GPU 计时或性能保证。
- 已有优化包括 UUID 索引、按修订号刷新 ViewModel/Console、材质/纹理/管线复用、逐 frame slot 上传跟踪与可复用的脚本执行 scratch 存储。
- 诊断使用 **spdlog** 与编辑器 Console，常规统计采集不依赖逐帧 info/debug 日志。
- 启动或资源失败时先检查 SDK 解析、显卡驱动、项目根目录与生成的 `.spv`；脚本无输入时先检查 Game/应用焦点和 Play 状态。

## 当前限制

- Transform Gizmo 拖动仅支持单轴移动；旋转/缩放手柄仅支持拾取与高亮，旋转/缩放拖动、平面/自由移动、吸附和操作切换尚未实现。移动轴接近正对视线时需要调整视角。
- 仅实现 Final 视口渲染，诊断渲染模式尚不可用。
- Windows SDK/DLL 构建流程仍需针对 Linux/macOS 适配。
- Lit 为基础前向光照；完整 PBR、阴影、后处理和骨骼动画尚未实现。
- Export/Package 尚未实现，开发运行依赖项目源 `Resource/` 目录。
- 资源所有权与较大场景行为仍需持续验证，不承诺固定实体数量、加载并发能力或帧率。

## 路线图

以下为状态概览，不是发布时间表：

- [x] 编辑器面板、资产操作、场景持久化与 Undo/Redo
- [x] 带验证的脚本管线、宿主服务与运行时槽同步
- [x] 统一四元数变换、场景 v4 迁移与 Local/World 数值编辑
- [x] 自适应世界网格与 Transform Gizmo 可视化
- [x] Gizmo 手柄拾取/高亮与可提交/取消的 Local/World 单轴移动拖拽
- [ ] Gizmo 旋转/缩放拖拽、平面/自由移动、吸附与操作切换
- [ ] 更多视口渲染模式
- [ ] PBR、阴影与后处理
- [ ] 动画与更广泛的核心/渲染集成测试
- [ ] 独立资产打包与非 Windows 构建支持
- [ ] 持续审查所有权、性能分析与 API 文档

## 开发与贡献

- ECS 组件放在 `Core/Public/ECS/Component/`，实现在 `Core/Private/ECS/Component/`。
- 渲染系统继承 [`XJMaterialSystem`](Core/Public/Render/System/XJMaterialSystem.h)，实现 `OnInit` / `OnRender` / `OnDestroy`，由渲染目标或视口宿主注册。
- 新编辑器 View 仅读取 ViewModel、提交请求；通过 Controller/Service 修改 ECS/渲染状态，并保留已有渲染宿主生命周期接口。
- GLSL 与 Shader 资产放在 `Resource/Shader/`；新增 Shader 文件后重新配置 CMake，刷新构建期 Shader 列表。
- 遵循周边命名与 include guard 风格。诊断日志统一使用 `spdlog::`；需要异常传播时先记录，再抛出标准异常。
- 提交聚焦的 Pull Request，补充相关测试并同步中英文文档；通过 [GitHub Issues](https://github.com/aidexiaojike-ops/XJEngine/issues) 提供复现步骤、构建配置与日志。

依赖包括 GLFW、EnTT、GLM、spdlog、stb_image、tinyobjloader、tinygltf、Dear ImGui、nlohmann/json、SPIRV-Reflect 和 Vulkan SDK。感谢这些项目的作者与维护者，内置项目见 `ThirdParty/`。

## 许可证

XJEngine 基于 [MIT 许可证](LICENSE) 开源。任何人均可使用、修改和再分发，包括商业用途，但须按许可证要求保留版权与许可声明。

许可文本：[English（英文原文）](LICENSE) | [简体中文（参考译文）](LICENSE_CN.md)。中文译文仅供参考，如有解释差异，以英文原文为准。

**合法使用提醒（不构成额外许可条件）：** 使用本项目须遵守适用法律。MIT 许可证不使违法犯罪行为合法化，也不免除相应法律责任。使用者应对自身使用行为负责。

第三方依赖各自保留其许可证，项目的 MIT 许可证不替代其授权条款。
