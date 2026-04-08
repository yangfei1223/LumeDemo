# macOS 移植开发进度归档

**归档日期**: 2026-04-08  
**状态**: 基础框架完成，暂停开发  
**分支**: mac (所有仓库)

---

## 已完成工作

### 1. 平台架构搭建 ✅

#### LumeEngine macOS 平台层
- **文件位置**: `LumeEngine/src/os/macos/`
- **实现内容**:
  - `platform_macos.h/cpp` - 平台抽象类
  - `library_macos.h/cpp` - 动态库加载 (dlopen/dlsym)
  - 使用 POSIX 标准 API
  - 支持 `.dylib` 动态库格式

#### LumeRender Vulkan macOS 层
- **文件位置**: `LumeRender/src/vulkan/macos/`
- **实现内容**:
  - `platform_vk.h/cpp` - Vulkan 平台检测
  - `platform_create_functions_vk.cpp` - Metal Surface 创建
  - 使用 `VK_EXT_metal_surface` 扩展
  - 基于 MoltenVK 的 Vulkan 到 Metal 转换

### 2. 构建系统修改 ✅

#### 根 CMakeLists.txt
- 添加平台检测逻辑 (APPLE/WIN32/UNIX)
- 条件设置 Vulkan 平台宏:
  - macOS: `VK_USE_PLATFORM_METAL_EXT`
  - Windows: `VK_USE_PLATFORM_WIN32_KHR`
  - Linux: `VK_USE_PLATFORM_XLIB_KHR`

#### LumeEngine/CMakeLists.txt
- 平台特定的源文件过滤
- 跨平台 DLL 导出宏:
  - Windows: `__declspec(dllexport/dllimport)`
  - macOS/Linux: `__attribute__((visibility("default")))`
- 编译器选项适配

#### LumeRender/CMakeLists.txt
- 平台特定的 include 目录
- 着色器编译器条件禁用 (macOS 上跳过)
- 导出宏和编译选项适配

#### Lume3D/CMakeLists.txt
- 着色器编译器条件禁用
- 跨平台编译选项

### 3. 依赖安装 ✅

已安装:
- ✅ MoltenVK (via Homebrew)
  - 位置: `/opt/homebrew/Cellar/molten-vk/1.4.1/`
  - 版本: 1.4.334
- ✅ CMake
- ✅ GLFW (通过子模块，支持 Cocoa)

### 4. 构建脚本 ✅

**文件**: `build_macos.sh`
- 自动检测 macOS 环境
- 检查 Vulkan SDK
- 配置 CMake 参数
- 构建 Release 版本

---

## 已知问题

### 🔴 严重问题

1. **着色器编译器缺失**
   - **问题**: `LumeShaderCompiler.exe` 是 Windows 可执行文件，macOS 无法运行
   - **影响**: 无法编译 `.vert`/`.frag`/`.comp` 为 `.spv` 二进制
   - **状态**: 已禁用 macOS 上的着色器编译
   - **解决方案**: 需要实现使用 `glslangValidator` 的替代方案

2. **插件系统汇编代码**
   - **问题**: 使用 Windows 特定的 `.weak` 汇编指令
   - **错误**: `<inline asm>:3:2: error: unknown directive`
   - **文件**: `Lume3D/src/plugin/dynamic_plugin.cpp`
   - **状态**: 未修复
   - **解决方案**: 需要 macOS 版本的汇编代码或改用 C++ 实现

### 🟡 中等问题

3. **文件系统路径处理**
   - **问题**: Windows 使用盘符 (C:/)，macOS 使用绝对路径 (/Users/...)
   - **状态**: 基础实现完成，可能需要测试调整

4. **动态库扩展名**
   - **Windows**: `.dll`
   - **macOS**: `.dylib`
   - **状态**: 已实现，需要测试验证

---

## 待完成工作

### Phase 1: 修复关键问题 (必须)

- [ ] 实现 macOS 着色器编译 (glslangValidator)
- [ ] 修复插件系统汇编代码
- [ ] 测试文件系统路径处理

### Phase 2: 功能完善 (重要)

- [ ] 完整的 macOS 文件系统实现
- [ ] 日志输出适配
- [ ] 错误处理优化
- [ ] 性能测试和调优

### Phase 3: 测试验证 (必要)

- [ ] CMake 配置测试
- [ ] 编译测试
- [ ] 运行时测试
- [ ] 渲染测试
- [ ] Windows 兼容性回归测试

---

## 技术债务

### 需要清理

1. **硬编码路径**
   - 某些路径可能仍假设 Windows 格式
   - 需要全面检查

2. **条件编译**
   - 大量 `#ifdef _WIN32` 需要补充 `__APPLE__`
   - 需要系统性检查

3. **第三方库**
   - 某些 Windows 特定的第三方库需要 macOS 替代
   - 需要评估影响

---

## 构建指南

### 环境要求

```bash
# 必需
- macOS 11.0+ (Big Sur 或更高)
- Xcode Command Line Tools
- CMake 3.24+
- MoltenVK (brew install molten-vk)

# 可选但推荐
- glslang (brew install glslang) - 用于着色器编译
- Ninja (brew install ninja) - 更快的构建
```

### 构建步骤

```bash
# 1. 克隆仓库
git clone https://github.com/yangfei1223/LumeDemo.git
cd LumeDemo
git checkout mac

# 2. 更新子模块
git submodule update --init --recursive

# 3. 运行构建脚本
./build_macos.sh

# 或手动构建:
export VULKAN_SDK=/opt/homebrew/Cellar/molten-vk/1.4.1/libexec
cmake -S . -B build_macos \
    -DVulkan_INCLUDE_DIR="$VULKAN_SDK/include" \
    -DVulkan_LIBRARY="/opt/homebrew/Cellar/molten-vk/1.4.1/lib/libMoltenVK.dylib"
cmake --build build_macos --config Release
```

---

## 文件清单

### 新增文件

```
LumeDemo/
├── MACOS_PORT_PLAN.md          # 移植计划文档
├── MACOS_DEV_ARCHIVE.md        # 本归档文档
├── build_macos.sh              # macOS 构建脚本
└── CMakeLists.txt              # 修改: 添加平台检测

LumeEngine/
└── src/os/macos/
    ├── platform_macos.h        # macOS 平台类声明
    ├── platform_macos.cpp      # macOS 平台实现
    ├── library_macos.h         # 动态库加载声明
    └── library_macos.cpp       # 动态库加载实现

LumeRender/
└── src/vulkan/macos/
    ├── platform_vk.h           # Vulkan 平台头文件
    ├── platform_vk.cpp         # Vulkan 平台实现
    └── platform_create_functions_vk.cpp  # Surface 创建
```

### 修改文件

```
LumeDemo/CMakeLists.txt
LumeEngine/CMakeLists.txt
LumeRender/CMakeLists.txt
Lume3D/CMakeLists.txt
```

---

## 分支状态

所有更改已推送到个人仓库的 `mac` 分支:

- **LumeDemo**: https://github.com/yangfei1223/LumeDemo/tree/mac
- **LumeEngine**: https://github.com/yangfei1223/LumeEngine/tree/mac
- **LumeRender**: https://github.com/yangfei1223/LumeRender/tree/mac
- **Lume3D**: https://github.com/yangfei1223/Lume3D/tree/mac

---

## 恢复开发

当你想继续开发时:

```bash
# 1. 拉取最新代码
git checkout mac
git pull myrepo mac
git submodule update

# 2. 检查待完成任务 (见上文)

# 3. 建议优先处理:
#    - 着色器编译器实现
#    - 插件系统修复
#    - 完整测试
```

---

## 备注

1. **Windows 兼容性**: 所有修改都保留了 Windows 支持，通过条件编译实现
2. **Linux 支持**: 部分修改也为 Linux 移植奠定了基础
3. **MoltenVK**: 当前使用 Homebrew 版本，生产环境建议使用官方 Vulkan SDK
4. **着色器**: 当前禁用编译，需要手动从 Windows 构建复制或使用 glslang

---

**最后更新**: 2026-04-08  
**开发者**: Agent (Sisyphus)
