# AGENTS.md - LumeDemo Coding Guidelines

## Project Overview

**LumeDemo** is a minimal 3D rendering demo built on the Lume Engine architecture. It's a C++17 project using CMake as the build system, targeting Windows 11 x64 with Vulkan graphics backend.

- **Language**: C++17
- **Build System**: CMake 3.24+
- **Platform**: Windows 11 x64 (primary)
- **Graphics**: Vulkan
- **Architecture**: ECS (Entity Component System)

## Repository Structure

```
LumeDemo/
├── src/                    # Main source code
│   ├── application.cpp     # Application implementation (MinimalDemo class)
│   └── main.cpp           # Entry point with GLFW window
├── include/               # Project headers
│   ├── application_interface.h    # IApplication interface
│   ├── application_factory.h      # Factory functions
│   └── application_config.h       # Configuration
├── assets/                # Runtime assets
│   └── glTF/             # 3D models (DamagedHelmet sample)
├── third_party/          # External dependencies
│   └── glfw-3.4/        # Windowing library
├── LumeBase/            # Submodule - Core base library
├── LumeEngine/          # Submodule - Engine core
├── LumeRender/          # Submodule - Rendering system
└── Lume3D/              # Submodule - 3D module
```

## Build Commands

### Prerequisites
- Windows 11 x64
- CMake 3.24+
- Visual Studio 2022 Build Tools (Desktop development with C++ workload)
- Vulkan SDK from https://vulkan.lunarg.com/sdk/home
- Git with SSH authentication (HTTPS often times out)

### Configure
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
```

### Build
```powershell
# Release build
cmake --build build --config Release

# Debug build
cmake --build build --config Debug
```

### Run
```powershell
Push-Location build\Release; .\LumeDemo.exe; Pop-Location
```

### Clean
```powershell
cmake --build build --config Release --target clean
```

## Git Workflow

### Remotes
- **origin**: https://github.com/yangfei1223/LumeDemo.git (personal fork)
- **upstream**: https://github.com/jsnchng/LumeDemo.git (original)

### Branches
- **main**: Synced with upstream, stable base
- **win**: Windows development branch (current)
- **mac**: macOS port branch (experimental)

### Submodule Management

All submodules point to personal forks:
- LumeBase → yangfei1223/LumeBase
- LumeEngine → yangfei1223/LumeEngine
- LumeRender → yangfei1223/LumeRender
- Lume3D → yangfei1223/Lume3D

**Initialize submodules:**
```bash
git submodule update --init --recursive
```

**Sync submodule URLs:**
```bash
git submodule sync --recursive
```

**Update all submodules:**
```bash
git submodule update --force --init --recursive
```

**Push all submodule branches:**
```bash
git submodule foreach 'git push origin win'
```

## Code Style

### Naming Conventions
- **Classes/Structs**: PascalCase (e.g., `MinimalDemo`, `CameraComponentManager`)
- **Functions**: PascalCase for public API (e.g., `OnInit()`, `UpdateCamera()`)
- **Member variables**: snake_case with trailing underscore (e.g., `engine_`, `windowWidth_`)
- **Local variables**: camelCase (e.g., `swapchainCreateInfo`)
- **Constants**: UPPER_SNAKE_CASE or constexpr PascalCase
- **Namespaces**: PascalCase with _NS suffix (e.g., `BASE_NS`, `CORE_NS`, `RENDER_NS`)

### Include Order
1. Platform-specific headers (`#ifdef _WIN32`)
2. Third-party libraries (GLFW, Vulkan)
3. Standard library headers
4. Base library headers (`<base/math/vector.h>`)
5. Core engine headers (`<core/ecs/intf_entity_manager.h>`)
6. Render headers (`<render/device/intf_device.h>`)
7. 3D module headers (`<3d/ecs/components/camera_component.h>`)
8. Local project headers (`"application_config.h"`)

### Namespace Usage
```cpp
BASE_BEGIN_NAMESPACE()
// code here
BASE_END_NAMESPACE()

using namespace BASE_NS;
using namespace CORE_NS;
using namespace RENDER_NS;
using namespace CORE3D_NS;
```

### Code Formatting
- **Indentation**: 4 spaces (no tabs)
- **Braces**: Opening brace on same line for functions/classes
- **Line length**: ~120 characters
- **Pointer alignment**: East const (e.g., `IDevice* device`)

## Architecture Notes

### ECS Pattern
The project uses Entity Component System architecture:
- **Entities**: Lightweight IDs
- **Components**: Data containers (e.g., `CameraComponent`, `TransformComponent`)
- **Systems**: Logic processors (e.g., `INodeSystem`, `IAnimationSystem`)

### Key Classes
- **MinimalDemo**: Main application class implementing `IApplication`
- **IApplication**: Interface for application lifecycle (OnInit, OnStart, OnFrame, OnStop)
- **IGraphicsContext**: 3D graphics management
- **IRenderContext**: Rendering system interface

### Plugin System
Engine uses plugin-based architecture:
```cpp
constexpr Uid uidRender[] = { UID_RENDER_PLUGIN };
GetPluginRegister().LoadPlugins(uidRender);
```

## Dependencies

### External
- **Vulkan SDK**: Graphics API
- **GLFW 3.4**: Windowing and input

### Internal (Submodules)
- **LumeBase**: Math, containers, platform abstraction
- **LumeEngine**: Core ECS, resource management, plugins
- **LumeRender**: Vulkan rendering backend
- **Lume3D**: 3D components, scene loading, glTF support

## Development Tips

### Common Issues
1. **Submodule not found**: Run `git submodule update --init --recursive`
2. **Build fails**: Ensure Vulkan SDK is installed and environment variable set
3. **HTTPS timeout**: Use SSH URLs for submodules (already configured)

### Adding New Components
1. Include appropriate module headers
2. Use namespace macros
3. Follow naming conventions
4. Register with ECS if needed

### Testing
- No automated tests currently
- Manual testing: Run executable and verify 3D rendering output
- Sample model: DamagedHelmet.gltf

## Platform Notes

- **Windows-only**: Project uses Win32-specific code
- **Vulkan backend**: Only rendering backend supported
- **Exceptions disabled**: Engine compiled without exception handling
- **Custom containers**: Use `BASE_NS::vector` instead of `std::vector`

## Resources

- **Upstream**: https://github.com/jsnchng/LumeDemo
- **Fork**: https://github.com/yangfei1223/LumeDemo
- **Vulkan SDK**: https://vulkan.lunarg.com/sdk/home

---

## SR Training Project

### Goal

实现**可微纹理超分辨率训练系统**，基于 LumeEngine 的 Render Node Graph 架构。

**核心原则**: "RDG as the Loop" - 训练完全在渲染管线内完成，不使用 Autodiff 引擎。

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Render Node Graph                            │
├─────────────────────────────────────────────────────────────────┤
│  CameraController → MaterialRenderSlot → DeferredShading        │
│         ↓                                                        │
│  G-Buffer (color, depth, normal, base_color, uv, material)      │
│         ↓                                                        │
│  SR_RESOURCES (创建训练资源: lr_texture, gt_image, gradients)   │
│         ↓                                                        │
│  SR_TRAINING (Downsample → Forward → Loss → Backward → Adam)   │
│         ↓                                                        │
│  LR_DISPLAY → BACKBUFFER                                         │
└─────────────────────────────────────────────────────────────────┘
```

### Key Discoveries

#### 1. RNG Registration Mechanism (CRITICAL)

**错误理解**: 设置 `RenderConfigurationComponent.customRenderNodeGraphFile`
**正确方式**: 设置 `CameraComponent.customRenderNodeGraphFile`

```cpp
// Camera RNG is what actually renders the scene
auto cameraHandle = cameraManager_->Write(cameraEntity_);
cameraHandle->customRenderNodeGraphFile = "assets://app/renderNodeGraph_sr_simplified.json";
```

**原因**:
- `RenderConfigurationComponent.customRenderNodeGraphFile` → Scene RNG (后处理等，不直接渲染)
- `CameraComponent.customRenderNodeGraphFile` → Camera RNG (实际渲染场景)

#### 2. Post Process RNG Behavior

当设置 `customRenderNodeGraphFile` 后，内置的 post process RNG **不会**被创建。

**代码逻辑** (`render_util.cpp` line 481-488):
```cpp
if (renderCamera.customRenderNodeGraphFile.empty()) {
    // 只有当 customRenderNodeGraphFile 为空时才获取 post process
    desc = GetBasePostProcessDesc(renderCamera);
}
```

**解决方案**: 自定义 RNG 必须包含输出到 backbuffer 的节点（如 `RenderNodeDefaultFinalCompose`）。

#### 3. Render Slot System

| Render Slot | G-Buffer Outputs | Purpose |
|------------|------------------|---------|
| `CORE3D_RS_DM_DF_OPAQUE` | 4 outputs | Standard deferred |
| `CORE3D_RS_DM_DF_OPAQUE_UV` | 5 outputs (adds UV) | Deferred + UV coordinates |

材料通过 `MaterialComponent.materialShader.graphicsState.renderSlot` 或 `customRenderSlotId` 分配到 render slot。

### Current Status

#### ✅ Completed
- `RenderNodeSRTraining` C++ 实现（4-pass 结构）
- `sr_differentiable_render.comp` shader（Mega Kernel: Forward + Loss + Backward）
- `sr_adam_optimizer.comp` shader
- `texture_downsample.comp` shader
- `renderNodeGraph_sr_simplified.json` 配置
- C++ binding 修复（匹配 shader binding）
- 发现 RNG 注册机制问题
- 发现 post process RNG 行为

#### ⏳ In Progress
- 验证自定义 RNG 完整执行

#### ❌ Blocked
- 自定义 RNG 黑屏问题

### Gap Analysis

| 问题 | 状态 | 解决方案 |
|------|------|----------|
| RNG 注册位置错误 | ✅ 已解决 | 使用 `CameraComponent.customRenderNodeGraphFile` |
| render_system.cpp bug | ✅ 已解决 | `createNewRng` → `createNewCustomRng` |
| Post process RNG 未创建 | ✅ 已理解 | 自定义 RNG 必须包含 FinalCompose |
| 自定义 RNG 黑屏 | ⏳ 调试中 | 需要验证 RNG 结构与内置一致 |

### Relevant Files

```
LumeDemo/
├── src/application.cpp                    # RNG 注册 (CameraComponent)
├── assets/app/
│   ├── renderNodeGraph.json               # 基础 RNG (4 nodes + FinalCompose)
│   └── renderNodeGraph_sr_simplified.json # SR 训练 RNG
│
├── Lume3D/src/
│   ├── ecs/systems/render_system.cpp      # RNG 创建逻辑
│   └── util/render_util.cpp               # SelectBaseDesc, LoadRenderNodeGraph
│
└── LumeRender/src/postprocesses/
    ├── render_node_sr_training.h
    └── render_node_sr_training.cpp
```

### Debug Checklist

- [ ] 自定义 RNG 是否包含 FinalCompose 节点？
- [ ] `renderSlot` 是否与材料匹配？
- [ ] G-buffer GPU images 格式是否正确？
- [ ] `subpassCount` 和 `colorAttachmentIndices` 是否匹配？
- [ ] 资源绑定 `nodeName` 和 `usageName` 是否正确？

### Gap Analysis (Detailed)

#### 核心问题：自定义 RNG 架构设计

**当前理解**:

LumeEngine 的渲染管线分为两部分：
1. **Camera RNG** - 渲染场景到 G-buffer / color
2. **Post Process RNG** - 将 color 输出到 backbuffer

**内置管线结构**:
```
Camera RNG (deferred, 6 nodes)
  ↓ 输出到 color/depth/velocity_normal
Post Process RNG (1 node: RenderNodeDefaultCameraPostProcessController)
  ↓ 输出到 backbuffer
```

**问题**:
- 设置 `customRenderNodeGraphFile` 后，post process RNG **不会自动创建**
- `RenderNodeDefaultFinalCompose` 可能不是正确的输出节点
- 需要理解 `RenderNodeDefaultCameraPostProcessController` 的工作方式

**可能的解决方案**:
1. 方案A: 在自定义 RNG 中包含 post process 逻辑
2. 方案B: 同时设置 `customPostProcessRenderNodeGraphFile`
3. 方案C: 自定义 RNG 完全不依赖 post process，直接输出到 backbuffer

#### 待验证的技术问题

| 问题 | 影响 | 验证方法 |
|------|------|----------|
| `renderDataStore` 配置缺失 | FinalCompose 可能无法正确获取相机数据 | 对比内置节点配置 |
| Post process controller 依赖 | 可能需要特定的 render data store | 查看 RenderNodeDefaultCameraPostProcessController 源码 |
| 资源绑定方式 | FinalCompose 的 color 绑定是否正确 | 检查 shader 和资源绑定 |

### Next Steps

1. **理解 RenderNodeDefaultCameraPostProcessController** - 它如何工作，需要什么配置
2. **验证方案B** - 同时设置 `customPostProcessRenderNodeGraphFile`
3. **或验证方案A** - 完全自定义 RNG，包含所有输出逻辑
4. 逐步添加 SR 训练节点
5. 验证训练逻辑执行
