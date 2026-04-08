# AGENTS.md - LumeDemo Coding Guidelines

This document provides guidelines for AI coding agents working on the LumeDemo codebase.

## Project Overview

LumeDemo is a minimal 3D rendering demo built on the Lume Engine. It's a C++17 project using CMake as the build system, targeting Windows 11 x64 with Vulkan graphics backend.

## Build Commands

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

## Code Style Guidelines

### Naming Conventions

- **Classes/Structs**: PascalCase (e.g., `CameraComponentManager`, `MinimalDemo`)
- **Functions**: PascalCase for public API, camelCase for private (e.g., `OnInit()`, `updateCamera()`)
- **Variables**: 
  - Member variables: snake_case with trailing underscore (e.g., `engine_`, `windowWidth_`)
  - Local variables: camelCase (e.g., `swapchainCreateInfo`, `width`)
  - Constants: UPPER_SNAKE_CASE or constexpr with PascalCase (e.g., `invalidSurfaceHandle`)
- **Macros**: UPPER_SNAKE_CASE with underscores (e.g., `BASE_PUBLIC`, `IMPLEMENT_MANAGER`)
- **Namespaces**: PascalCase (e.g., `BASE_NS`, `CORE_NS`, `RENDER_NS`)
- **Enums**: PascalCase for type, UPPER_SNAKE_CASE for values (e.g., `CameraComponent::Projection::PERSPECTIVE`)
- **Template parameters**: PascalCase (e.g., `typename T`, `typename InputIt`)

### Namespaces

Always use namespace macros:
```cpp
BASE_BEGIN_NAMESPACE()
// code here
BASE_END_NAMESPACE()

CORE_BEGIN_NAMESPACE()
// code here
CORE_END_NAMESPACE()
```

Use namespace aliases:
```cpp
using namespace BASE_NS;
using namespace CORE_NS;
using namespace RENDER_NS;
using namespace CORE3D_NS;
```

### Include Order

1. Platform-specific headers (e.g., `#ifdef _WIN32`)
2. Third-party library headers (e.g., `<GLFW/glfw3.h>`, Vulkan headers)
3. Standard library headers (e.g., `<memory>`, `<algorithm>`)
4. Base library headers (e.g., `<base/math/vector.h>`)
5. Core engine headers (e.g., `<core/ecs/intf_entity_manager.h>`)
6. Render headers (e.g., `<render/device/intf_device.h>`)
7. 3D module headers (e.g., `<3d/ecs/components/camera_component.h>`)
8. Local project headers (e.g., `"application_config.h"`)

### Code Formatting

- **Indentation**: 4 spaces (no tabs)
- **Braces**: Opening brace on same line for functions/classes, new line for namespace contents
- **Line length**: ~120 characters
- **Pointer/reference alignment**: East const, pointer attached to type (e.g., `IDevice* device`)
- **Comments**: Use `//` for single-line, `/* */` for multi-line

### Type Safety

- Use custom container types from `base/containers/` instead of std:: equivalents:
  - `BASE_NS::vector` instead of `std::vector`
  - `BASE_NS::string` instead of `std::string`
  - `BASE_NS::array_view` for array views
- Use `constexpr` for compile-time constants
- Use `auto` only when type is obvious from context
- Use trailing return type syntax for complex return types

### Error Handling

- Use assertions: `BASE_ASSERT()`, `BASE_ASSERT_MSG()`
- Check Vulkan results: `if (result != VK_SUCCESS)`
- Return early on errors with cleanup
- Use `RenderResultCode` for render-related errors

### Class Design

- Use `final` for classes that shouldn't be inherited
- Use `override` for virtual function overrides
- Use `= default` for trivial destructors
- Use `Ptr` typedef for smart pointers (e.g., `IEngine::Ptr`)
- Prefer composition over inheritance

### Memory Management

- Use custom allocators from `base/containers/allocator.h`
- Use `unique_ptr` for exclusive ownership
- Use `shared_ptr`/`refcnt_ptr` for shared ownership
- Avoid raw `new`/`delete` when possible

### Platform Abstraction

- Wrap platform-specific code in `#ifdef _WIN32` / `#endif`
- Use `WIN32_LEAN_AND_MEAN` before including Windows headers
- Use `NOMINMAX` to prevent Windows macro conflicts

### Best Practices

- Mark unused parameters with `(void)parameter;`
- Use `[[nodiscard]]` for functions returning important values
- Prefer `const` correctness
- Use `noexcept` for functions that don't throw
- Initialize member variables in-class or in constructor initializer list
- Use `enum class` for strongly typed enums

## File Organization

- Headers: `include/` for public API, `src/` for private headers
- Implementation: `src/` directory
- Assets: `assets/` directory
- Third-party: `third_party/` directory
- Module structure: `LumeBase/`, `LumeEngine/`, `LumeRender/`, `Lume3D/`

## Testing

This project does not currently have automated tests. Manual testing is done by running the executable and verifying 3D rendering output.

## Dependencies

- CMake 3.24+
- Visual Studio 2022 Build Tools
- Vulkan SDK
- GLFW 3.4
- Git with SSH authentication

## Git Configuration

### User Information
```bash
git config --global user.name "Fei Yang"
git config --global user.email "yangfei92516@163.com"
```

### Remote Repositories
- **origin**: https://github.com/jsnchng/LumeDemo.git (upstream)
- **myrepo**: https://github.com/yangfei1223/LumeDemo.git (personal fork)

### Submodule Repositories (All forked to personal account)
| Submodule | Original | Personal Fork |
|-----------|----------|---------------|
| LumeBase | jsnchng/LumeBase | yangfei1223/LumeBase |
| LumeEngine | jsnchng/LumeEngine | yangfei1223/LumeEngine |
| LumeRender | jsnchng/LumeRender | yangfei1223/LumeRender |
| Lume3D | jsnchng/Lume3D | yangfei1223/Lume3D |

### Branch Strategy
- **main**: Synced with upstream (origin/main)
- **mac**: Personal development branch with custom changes

### Common Git Operations

#### Sync with upstream
```bash
git checkout main
git pull origin main
git checkout mac
git merge main
```

#### Push to personal repo
```bash
git push myrepo mac
```

#### Update submodules
```bash
git submodule update --init --recursive
git submodule sync
```

#### Push all submodule changes
```bash
git submodule foreach 'git push origin mac'
```

### Submodule Workflow
When modifying submodules:

```bash
# 1. Enter submodule and switch to mac branch
cd LumeBase
git checkout mac

# 2. Make changes, commit and push
git add .
git commit -m "Your changes"
git push origin mac

# 3. Return to main repo and update reference
cd ..
git add LumeBase
git commit -m "Update LumeBase submodule"
git push myrepo mac
```

## Notes

- This is a Windows-only project
- Uses C++17 standard
- Exceptions are disabled in the engine
- Custom STL-like containers are used throughout
- Heavy use of ECS (Entity Component System) architecture
