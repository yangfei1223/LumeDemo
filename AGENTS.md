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
