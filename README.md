# XJEngine

<div align="center">

**English** &nbsp;&nbsp;|&nbsp;&nbsp; **[简体中文](README_CN.md)**

</div>

[![Platform](https://img.shields.io/badge/Platform-Windows-brightgreen)](https://github.com/aidexiaojike-ops/XJEngine)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Vulkan](https://img.shields.io/badge/Renderer-Vulkan-orange.svg)](https://www.vulkan.org/)
[![CMake](https://img.shields.io/badge/CMake-3.28+-yellow.svg)](https://cmake.org/)

XJEngine is an experimental C++20 game engine and editor built on Vulkan and EnTT. It includes a schema-driven material pipeline, asset and scene persistence, a built-in scripting language, and an ImGui editor.

**Current build path: Windows.** The SDK setup and runtime DLL handling expect a Windows Vulkan SDK layout; Linux and macOS are not ready-to-build targets. Transform Gizmos are currently **visualization only**, not draggable editing tools.

## Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Build Configuration](#build-configuration)
- [Editor Workflow](#editor-workflow)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [API Examples](#api-examples)
- [Tests](#tests)
- [Performance and Diagnostics](#performance-and-diagnostics)
- [Current Limitations](#current-limitations)
- [Roadmap](#roadmap)
- [Development and Contributions](#development-and-contributions)
- [License](#license)

## Features

| Area | Implemented capabilities |
|------|--------------------------|
| **Vulkan rendering** | Off-screen Scene/Game targets, depth buffers, GLSL-to-SPIR-V build steps, structured swapchain acquire/present and resize handling |
| **ECS and transforms** | EnTT entities/components, stable UUID lookup, hierarchy-aware local/world transforms, checked quaternion/TRS edits |
| **Materials and lighting** | JSON shader schemas, SPIR-V reflection, Unlit/Lit surface materials, directional/point/spot lights, material and pipeline caches |
| **Assets and scenes** | CPU Asset / GPU Resource separation, glTF 2.0 import, submesh material slots, AABBs, `.xjmeta` metadata, scene format v4 with v2/v3 migration |
| **Built-in scripting** | `.xjs` lexer/parser/compiler, bytecode verification, execution limits, typed values, host services, ECS lifecycle and live slot synchronization |
| **Editor panels** | Hierarchy, Inspector, Content Browser, Console, Scene/Game Preview, docking and detached ImGui windows |
| **Local/World editing** | Scene toolbar space switch, Inspector numeric editing, rejection feedback, snapshot-based Undo/Redo |
| **Transform Gizmo visualization** | Move arrows, rotation rings and scale boxes on all three axes; Local/World orientation and approximately constant screen size; no handle interaction yet |
| **Scene helpers** | Adaptive world grid, light wireframes, AABB + triangle picking, picked-surface orbit pivot |
| **Play mode** | Runtime scene cloning, Play/Pause/Resume/Stop, runtime inspection and script-slot mutations without editing the source scene |
| **Diagnostics** | spdlog-backed Console, viewport performance snapshots, frame history and material upload statistics |

## Quick Start

### Prerequisites

- Windows 10/11 x64 and a Vulkan-capable GPU with an installed graphics driver.
- A C++20 compiler. Visual Studio 2022 with **Desktop development with C++** is the recommended setup for the commands below.
- **CMake 3.28 or newer**. The top-level file still declares 3.10, but the bundled EnTT dependency requires 3.28.
- A Windows Vulkan SDK, or network access for CMake's automatic SDK download. Other dependencies are included under `ThirdParty/`.

Run these commands in PowerShell from the directory where you want to clone the project:

```powershell
git clone https://github.com/aidexiaojike-ops/XJEngine.git
cd XJEngine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\bin\XJEngine.exe
```

If `build/` already uses another generator, keep that generator or choose a different build directory instead of reusing an incompatible CMake cache. For Visual Studio's multi-configuration generator, select Release with `--config Release`, not `CMAKE_BUILD_TYPE`.

The first configure may download a large Vulkan SDK installer and take several minutes. The SDK does not replace the GPU vendor's graphics driver.

## Build Configuration

### Vulkan SDK resolution

[`cmake/SetupVulkanSDK.cmake`](cmake/SetupVulkanSDK.cmake) searches in this order:

1. A valid SDK at the `VULKAN_SDK` environment variable.
2. The local cache at `ThirdParty/VulkanSDK` (overridable with `XJ_VULKAN_SDK_LOCAL_DIR`).
3. Download and local installation of the Windows LunarG Vulkan SDK **1.3.283.0**.

The resolved SDK must contain `Include/vulkan/vulkan.h`, `Lib/vulkan-1.lib`, `Bin/glslc.exe`, and `Dll/vulkan-1.dll`. For offline builds, install a complete SDK first and set `VULKAN_SDK` or the local-cache path.

| Setting | Purpose | Default |
|---------|---------|---------|
| `XJ_BUILD_TESTS` | Build and register first-party CTest tests | `OFF` |
| `XJ_VULKAN_SDK_LOCAL_DIR` | Local Windows SDK cache | `ThirdParty/VulkanSDK` |
| `XJ_VULKAN_SDK_URL` | SDK installer/archive download URL | Windows SDK 1.3.283.0 installer |
| `XJ_VULKAN_DOWNLOAD_TIMEOUT` | Download timeout in seconds | `1800` |

For example, increase the download timeout with:

```powershell
cmake -S . -B build -DXJ_VULKAN_DOWNLOAD_TIMEOUT=3600
```

`XJ_BUILD_EXAMPLES`, `XJ_ENABLE_VALIDATION`, and `XJ_USE_IMGUI` are **not** current project build options.

### Output and project paths

| Path | Purpose |
|------|---------|
| `bin/XJEngine.exe` | Editor executable; Debug and Release share `bin/` |
| `Resource/Shader/*.spv` | Shader build output alongside source GLSL and shader assets |
| `Resource/` | Source assets read directly by the editor and runtime during development |
| `Resource/Config/AssetRegistry.json` | Project asset registry |
| `Resource/Scenes/Default.xjscene` | Default scene |
| `Resource/Config/EditorUI.json` | Initial editor UI configuration template |
| `bin/Saved/Config/EditorUI.json` | Per-user editor UI state |
| `bin/Saved/Config/imgui.ini` | Per-user ImGui layout |

Assets are **not copied to `bin/Resource`**. Copying only the executable is not a standalone game distribution; Export/Package is not implemented yet.

The editor resolves its project root in this order: `--project` > `XJ_PROJECT_ROOT` environment variable > source root recorded by CMake. Pass the directory **containing `Resource/`**, not `Resource/` itself:

```powershell
.\bin\XJEngine.exe --project "E:\Projects\MyXJProject"
```

On Windows, user UI state follows the executable directory rather than the launch working directory. Moving the source project requires rebuilding or specifying the new project root.

## Editor Workflow

### Scene and asset editing

1. Select entities in **Hierarchy** or click scene geometry in **Scene Preview**. Picking uses world-space AABB filtering followed by CPU ray-triangle tests.
2. Use **Inspector** for component values, material parameters, cameras, lights and script slots. Hierarchy supports entity creation, deletion and renaming.
3. Use **Content Browser** for asset operations and drag Mesh assets into Scene Preview to create entities. External file drops are handled separately by the editor's import controller.
4. Save scenes as `.xjscene`. Scene and material edits participate in snapshot-based Undo/Redo, with up to 100 history entries.

### Local/World Transform editing

- Click the **Local/World** button in the Scene toolbar to change the Inspector coordinate space. Switching is available only in Edit mode and changes neither scene data nor Undo history.
- Transform snapshots use double precision. Field-specific requests preserve untouched local components.
- World rotation is composed from normalized quaternions independently of scale. World scale is non-negative **lossy scale** (world matrix axis lengths), not signed local scale; world-scale edits preserve local reflection signs.
- Services validate a temporary candidate before applying an edit. Rejected edits show an entity/space-specific reason and leave the scene, dirty flag and Undo/Redo history unchanged. No-op edits also create no history entry.
- Component copy/paste always uses **Local TRS**. Transform editing remains read-only during Play/Pause runtime inspection.

### Transform Gizmo visualization

Selecting an entity with a valid Transform in an editable scene displays a unified nine-part Gizmo in **Scene Preview**:

- Three move arrows, three rotation rings and three scale boxes, colored by X/Y/Z axis.
- The origin follows the entity's **world position**. **Local** uses its pure world rotation; **World** aligns with global XYZ axes. Object scale does not distort the Gizmo geometry.
- Projection and viewport dimensions determine a uniform world scale, targeting an approximately **110-pixel screen radius** rather than a fixed world-space size.
- Solid, unlit, double-sided triangles render without depth testing or depth writes, so scene geometry does not occlude the Gizmo.
- It is hidden during Play/Pause, read-only/runtime inspection, invalid selection or missing Transform. Invalid frame/projection inputs skip drawing. It is not added to Game Preview.

**This is visualization only.** There is no handle hit-testing, hover selection, drag editing, or Move/Rotate/Scale operation switch yet. Viewport clicks still perform entity picking; use Inspector values to edit transforms.

### World grid and viewport display

- Scene Preview draws a world grid with adaptive **1/2/5-series spacing**, at least **1 meter**, X/Z coordinate labels, ticks, spacing text and edge fading.
- The grid uses depth testing but does not write depth. Layout follows the camera and viewport; invalid camera poses skip the grid rather than reuse a cached view.
- Grid, light wireframes and Transform Gizmos are Scene-only helpers; Game Preview renders without these editor passes.
- Only the **Final** render-view mode is implemented. BaseColor, LinearDepth, WorldNormal and Lighting controls remain capability-disabled.

### Play mode and scripts

- Play clones the editing scene into a runtime scene; Pause/Resume control simulation and Stop restores the editor selection.
- Runtime Hierarchy/Inspector read runtime snapshots. Script components/slots can be added or removed, enabled/disabled and replaced through runtime requests; synchronization occurs at scheduler safe points, including while paused.
- Script input requires **Playing**, focused Game and application windows, and no active UI control, text input or popup. Press/release edges are Update-only and suppressed on focus acquisition.
- Saving `.xjs` source reports the save result and compiler diagnostics to Content Browser. A successful disk save can still contain compile errors; Play preflight rejects invalid scripts. The script editor clears its dirty state only after a successful save response.

## Architecture

```text
Application -> Scene / ECS -> Camera and Material Systems
            -> Off-screen Render Targets -> Editor Frame Renderer -> Swapchain

UI View -> Request -> Controller / Service -> ECS or render state
        <- ViewModel snapshots <-----------+

File -> Scanner / Importer -> Asset (CPU) -> Factory -> Render Resource (GPU)
```

### ECS, transforms and cameras

- `XJEntity` validates scene lifetime tokens; scenes maintain UUID-to-EnTT lookup and reject duplicate UUIDs. Engine/editor entities use a reserved UUID range.
- `XJTransformComponent` keeps local TRS private, normalizes quaternion rotation and exposes degree-based XYZ Euler accessors (`Rx * Ry * Rz`). Checked setters return `XJTransformStatus`; failed writes are atomic.
- World evaluation checks parent chains for cycles and non-finite values. GPU and picking consumers check float representability after double-precision evaluation.
- Cameras use world position/rotation, local forward `-Z` and up `+Y`. View-matrix reads do not mutate ECS or orbit settings; Free/Orbit controllers submit checked world-pose changes.
- Scene format **v4** stores local TRS and camera orbit mode/target/radius. Loading v2/v3 migrates legacy yaw/pitch camera poses to parent-relative transforms and rejects unrepresentable poses.
- `XJSystemScheduler` provides transactional Start/Stop, fixed-step updates with a per-frame cap, and safe-point callbacks for deferred runtime mutations. `XJInput` exposes keyboard/mouse held/pressed/released snapshots and axis queries.

### Rendering and materials

- The Vulkan platform layer separates acquire/present success, swapchain recreation and device-lost states. Instance API selection supports up to Vulkan 1.3 and is passed to ImGui; samplers use RAII ownership.
- `XJBaseMaterialSystem` uses dynamic uniform buffers. `XJSurfaceMaterialSystem` renders schema-driven `XJSurfaceMaterialComponent` items through cached pipeline runtimes.
- JSON `.schema` / `.xjshader` assets define parameters and bindings. Validation, cross-stage SPIR-V reflection and descriptor-layout builders connect assets to runtime parameter blocks and texture bindings.
- `Unlit.schema` and `Lit.schema` share the surface runtime. Lit supports base color/albedo texture, specular strength and shininess with forward directional/point/spot lighting; this is not a full PBR renderer.
- Shared `XJFrameUbo` and `XJLightUbo` carry camera/frame data and **one directional, up to eight point and eight spot lights**. Lights persist type, enabled state, color, intensity, range and cone angles.
- Material instance IDs and parameter/resource revisions synchronize per-frame-slot uploads. Descriptor pools expand on demand; factory caches use weak references and reuse expired material slots.

### Assets and editor ownership

- Assets are CPU-side data; factories create GPU meshes, textures and materials. `.xjmeta` sidecars preserve asset handles/type/importer data, and the registry separates persistent from runtime-generated handles.
- glTF 2.0 import merges triangle primitives into shared buffers with per-primitive index ranges, material slots and bounds; unsupported non-triangle modes are skipped. Meshes retain CPU position/index copies for picking.
- `XJJsonIO` provides shared JSON readers and atomic writes. Scene serializers and instantiators convert between `.xjscene` assets and live ECS hierarchies.
- `Src/main.cpp` creates `XJEditorApplication`; `XJEditorRuntime` owns workspace, play controller, viewport system, frame renderer, input bindings and UI host.
- Views read ViewModels and submit requests. Controllers/Services apply mutations. `XJEditorTransformToolsController` publishes space settings and `XJEditorTransformGizmoViewModel`; the Gizmo render system consumes this display snapshot.
- `XJViewportRenderSurface` owns off-screen render targets and defers descriptor release on resize. `XJEditorLog` bridges spdlog into revision-based Console snapshots with clipped single-line rendering.

### Built-in scripting

- Pipeline: lexer -> parser -> semantic analyzer -> compiler -> bytecode verifier -> runtime. Execution uses instruction budgets, call-depth limits, type/overflow checks, fault isolation, stack traces and re-entrancy protection.
- `XJScriptValue` supports bool/int/float/string, `Vec3` and `EntityRef`. Vectors support arithmetic, scalar multiplication/division, writable components, compound assignment and prefix/postfix updates.
- `XJScriptSystem` dispatches `OnCreate`, `OnUpdate`, `OnFixedUpdate` and `OnDestroy`. Entities can hold multiple enabled/disabled slots with stable UUIDs and `[[FieldId("...")]]` field overrides.
- `XJScriptHostNativeInvoker` injects entity, transform, input and logging services with owner/slot/frame/phase context; services remain available through `OnDestroy`.
- Transform APIs read/write local position/Euler angles/scale and read world values; `TrySetWorldPosition`, `TrySetWorldEulerAngles` and `TrySetWorldScale` report rejected writes without partial changes. Calls support the owner or an explicit `EntityRef`; `Transform.RotateY` rotates about local Y.
- `Scene.FindByUUID` accepts decimal or `0x` hexadecimal strings. `Entity.Self`, `Entity.IsValid` and `Entity.GetUUID` use session-scoped handles with stale/cross-session protection; a valid UUID with no match returns null.
- The native signature catalogue is shared by compilation, verification and dispatch. Typed value serialization rejects non-finite numbers; `EntityRef` cannot be persisted, exposed as a public field or stored as a non-null bytecode constant.
- Script assets are compiled/cached and scene files retain slot IDs, enabled state, references and field overrides. `Log.Info`/`Warn`/`Error` use spdlog with per-slot/global quotas.

## Project Structure

```text
XJEngine/
├── Core/                    # ECS, camera, input, geometry, assets, scripting, rendering
│   ├── Public/              # Public interfaces
│   └── Private/             # Implementations
├── Editor/                  # ImGui editor and MVVM application services
│   ├── Public/Controllers/  # Scene, asset, camera, transform and display controllers
│   ├── Public/Services/     # Scene/asset/display services
│   ├── Public/Tools/        # Transform types, grid layout and Gizmo geometry
│   ├── Public/UI/           # ViewModels, requests, panels and viewports
│   └── Private/             # Controllers, services, UI and editor render systems
├── Platform/                # Vulkan, GLFW, events and platform wrappers
├── Src/                     # main.cpp and XJEditorApplication
├── Resource/                # Source assets, read directly during development
│   ├── Shader/              # GLSL, generated SPIR-V, .schema and .xjshader
│   ├── Mesh/                # Model assets
│   ├── Texture/             # Texture assets
│   ├── Material/            # .xjmat materials
│   ├── Script/              # .xjs examples and metadata
│   ├── Scenes/              # .xjscene scenes
│   └── Config/              # Asset registry and initial UI template
├── Tests/                   # First-party CTest targets
├── ThirdParty/              # Bundled dependencies; optional local Vulkan SDK cache
├── cmake/                   # SDK setup, shader compilation and dependency configuration
├── bin/                     # Generated executable, runtime DLLs and Saved/ user state
└── build/                   # Generated build tree
```

## API Examples

These are integration snippets, not standalone programs. Use a valid scene and initialized render context; keep GPU resources within the render host's lifetime. For the application entry and editor lifecycle, see [`Src/main.cpp`](Src/main.cpp) and [`Src/XJEditorApplication.cpp`](Src/XJEditorApplication.cpp).

### Create an entity and edit its Transform

```cpp
auto* entity = scene.CreateEntityWithTransform("Cube");
auto& transform = entity->GetComponent<XJ::XJTransformComponent>();

const auto status = transform.SetLocalTRSEuler(
    glm::dvec3(0.0), glm::dvec3(0.0, 45.0, 0.0), glm::dvec3(1.0));
if (status != XJ::XJTransformStatus::Success)
    spdlog::error("Cube transform could not be applied");
```

### Create a schema-initialized Surface material

Assume `entity` has no Surface material component, and `mesh`, `defaultTexture` and `defaultSampler` are valid render resources. The project resource root and shader assets must already be available.

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

`CreateDefaultMaterial` builds the Unlit shader layout and fallback texture bindings. Use `CreateFromAsset` for an existing `.xjmat` asset and its shader. Bare `CreateMaterial<T>()` only allocates a material; it does **not** initialize a schema-backed parameter layout.

### Script example

This is **`.xjs` syntax**, not a C++ translation unit:

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

[`Resource/Script/`](Resource/Script/) also includes `PlayerMove.xjs` (keyboard movement), `MoveTarget.xjs` (UUID lookup and cross-entity movement), `WorldPosition.xjs` / `WorldScale.xjs` (checked world writes), and `Lifecycle.xjs`.

## Tests

From the repository root, enable tests, build, then run CTest:

```powershell
cmake -S . -B build -DXJ_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Coverage includes:

- Script language, compiler, bytecode verifier/runtime, native contracts, typed values, asset compilation/loading, serialization and editor script operations.
- ECS script lifecycle, live slot synchronization, deferred destruction, entity session isolation, host transforms, input focus/phase rules and log quotas.
- Quaternion/TRS math, parent inheritance, failed-write atomicity, camera controls/read-only queries, world lighting/picking/bounds and scene v2/v3-to-v4 migration.
- Local/World acceptance behavior, field isolation, reflected/lossy scale, rejection feedback and Undo/Redo preservation.
- Material upload state and viewport requests, capabilities and statistics.
- CPU-only world-grid layout and Transform Gizmo geometry/frame calculations. `XJTransformGizmoGeometryTests` checks nine unique handles, finite/non-degenerate triangle lists, geometry bounds, perspective scaling and invalid viewport height.

These tests do not replace manual Vulkan validation or visual viewport checks.

## Performance and Diagnostics

- Viewport overlays show completed-frame time/FPS, script CPU time and active instances, draw calls and material uploads, with a **128-sample frame history**. These snapshots are not GPU timing measurements or a performance guarantee.
- Existing optimizations include UUID indexing, revision-based ViewModel/Console refresh, material/texture/pipeline reuse, per-frame-slot upload tracking and reusable script execution scratch storage.
- Diagnostics use **spdlog** and the editor Console. Normal statistics collection does not rely on per-frame info/debug logging.
- For startup/resource failures, check SDK resolution, GPU drivers, project-root selection and generated `.spv` files. For script input, check Game/application focus and Play state first.

## Current Limitations

- Transform Gizmos do not support picking, dragging, snapping or operation switching yet.
- Only Final viewport rendering is implemented; diagnostic render modes remain unavailable.
- The Windows SDK/DLL build setup still needs platform-specific adaptation for Linux/macOS.
- Lit is basic forward lighting; full PBR, shadows, post-processing and skeletal animation are not implemented.
- Export/Package is not implemented; development runs depend on the project's source `Resource/` directory.
- Resource ownership and larger-scene behavior still need continued validation. No fixed entity-count, loading-concurrency or frame-rate guarantees are made.

## Roadmap

Status, not a release schedule:

- [x] Editor panels, asset operations, scene persistence and Undo/Redo
- [x] Verified scripting pipeline, host services and runtime slot synchronization
- [x] Unified quaternion transforms, scene v4 migration and Local/World numeric editing
- [x] Adaptive world grid and Transform Gizmo visualization
- [ ] Gizmo handle picking, drag operations and editing constraints
- [ ] Additional viewport render modes
- [ ] PBR, shadows and post-processing
- [ ] Animation and broader core/render integration tests
- [ ] Standalone asset packaging and non-Windows build support
- [ ] Continued ownership review, profiling and API documentation

## Development and Contributions

- Add ECS components under `Core/Public/ECS/Component/` and implementations under `Core/Private/ECS/Component/`.
- Derive render systems from [`XJMaterialSystem`](Core/Public/Render/System/XJMaterialSystem.h); implement `OnInit` / `OnRender` / `OnDestroy` and register them with the render target or viewport host.
- Keep new editor Views limited to ViewModel reads and request submission. Apply ECS/render mutations through Controllers/Services; preserve existing render-host lifecycle interfaces.
- Put GLSL and shader assets in `Resource/Shader/`. Reconfigure CMake after adding shader files so the build-time shader list is refreshed.
- Follow surrounding naming/include-guard conventions. Log diagnostics with `spdlog::`; when propagation is required, log and then throw a standard exception.
- Submit focused pull requests with relevant tests and matching English/Chinese documentation. Report reproduction steps, build configuration and logs through [GitHub Issues](https://github.com/aidexiaojike-ops/XJEngine/issues).

Dependencies include GLFW, EnTT, GLM, spdlog, stb_image, tinyobjloader, tinygltf, Dear ImGui, nlohmann/json, SPIRV-Reflect and the Vulkan SDK. Thanks to their authors and maintainers; see `ThirdParty/` for the bundled projects.

## License

The repository currently contains no project-level `LICENSE` file. Confirm licensing with the maintainer before reuse or redistribution; do not assume an MIT grant from earlier README text. Third-party dependencies retain their own licenses.
