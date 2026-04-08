# LumeDemo macOS 移植计划

## 项目目标
在保留原有 Windows 平台支持的基础上，添加完整的 macOS 平台支持，使 LumeDemo 能够在 macOS 上成功编译和运行。

## 技术方案

### 1. Vulkan 支持
- 使用 **MoltenVK** - Vulkan 到 Metal 的移植层
- 通过 GLFW 创建 Metal Surface
- 使用 `VK_EXT_metal_surface` 扩展

### 2. 平台抽象
参考已有实现：
- Windows: `LumeEngine/src/os/windows/`
- OpenHarmony: `LumeEngine/src/os/ohos/`

新增 macOS 实现：
- `LumeEngine/src/os/macos/`
- `LumeRender/src/vulkan/macos/`

### 3. 构建系统
- CMake 条件编译
- 自动检测平台
- 保留 Windows 构建不变

## 详细实施计划

### Phase 1: 基础框架 (2-3 小时)

#### 1.1 创建目录结构
```
LumeEngine/src/os/macos/
├── platform_macos.h
├── platform_macos.cpp
├── library_macos.h
├── library_macos.cpp
├── logger_output.cpp
└── CMakeLists.txt

LumeRender/src/vulkan/macos/
├── platform_vk.cpp
├── platform_create_functions_vk.cpp
├── platform_device_vk.cpp
├── platform_gpu_buffer_vk.cpp
├── platform_gpu_image_vk.cpp
├── platform_hardware_buffer_util_vk.cpp
└── CMakeLists.txt
```

#### 1.2 修改根 CMakeLists.txt
- 添加平台检测逻辑
- 条件设置 Vulkan 平台宏
- 添加 macOS 编译选项

### Phase 2: LumeEngine macOS 平台层 (3-4 小时)

#### 2.1 PlatformMacOS 类实现
参考 `PlatformWindows` 和 `PlatformOHOS`：
- 构造函数/析构函数
- 平台数据管理
- 插件位置注册
- 默认路径注册

#### 2.2 文件系统实现
- 路径处理（无盘符的绝对路径）
- 目录操作
- 文件操作

#### 2.3 动态库加载
- 使用 `dlopen`/`dlsym`（POSIX 标准）
- 处理 `.dylib` 文件

#### 2.4 日志输出
- macOS 系统日志或控制台输出

### Phase 3: LumeRender Vulkan macOS 层 (3-4 小时)

#### 3.1 平台 Vulkan 实现
参考 `windows/platform_vk.cpp`：
- `CanDevicePresent()` - 检测设备呈现支持
- `GetPlatformSurfaceName()` - 返回 Metal surface 扩展名

#### 3.2 Surface 创建
参考 `windows/platform_create_functions_vk.cpp`：
- `CreateSurface()` - 使用 `vkCreateMetalSurfaceEXT`
- 通过 GLFW 获取 Metal layer

#### 3.3 其他平台功能
- GPU Buffer 管理
- GPU Image 管理
- 硬件缓冲区工具

### Phase 4: 构建系统修改 (2-3 小时)

#### 4.1 各模块 CMakeLists.txt
- `LumeEngine/CMakeLists.txt` - 添加 macOS 源文件
- `LumeRender/CMakeLists.txt` - 添加 macOS 源文件
- `Lume3D/CMakeLists.txt` - 处理导出宏
- `LumeBase/CMakeLists.txt` - 如有需要

#### 4.2 跨平台导出宏
创建统一的头文件处理：
```cpp
#ifdef _WIN32
    #define API_EXPORT __declspec(dllexport)
    #define API_IMPORT __declspec(dllimport)
#else
    #define API_EXPORT __attribute__((visibility("default")))
    #define API_IMPORT
#endif
```

### Phase 5: 主程序修改 (1-2 小时)

#### 5.1 src/main.cpp
- 移除或条件编译 Windows.h
- 保持跨平台入口点
- GLFW 已经跨平台，无需修改

### Phase 6: 构建脚本和文档 (1 小时)

#### 6.1 创建构建脚本
`build_macos.sh`:
```bash
#!/bin/bash
# macOS 构建脚本
```

#### 6.2 更新文档
- 在 README.md 中添加 macOS 构建说明
- 更新 AGENTS.md 的 Git 配置部分

### Phase 7: 测试和修复 (2-4 小时)

#### 7.1 编译测试
- 解决编译错误
- 处理链接问题
- 验证 MoltenVK 集成

#### 7.2 运行时测试
- 验证窗口创建
- 验证 Vulkan 初始化
- 验证渲染循环

## 文件修改清单

### 新增文件（约 15 个）
1. `LumeEngine/src/os/macos/platform_macos.h`
2. `LumeEngine/src/os/macos/platform_macos.cpp`
3. `LumeEngine/src/os/macos/library_macos.h`
4. `LumeEngine/src/os/macos/library_macos.cpp`
5. `LumeEngine/src/os/macos/logger_output.cpp`
6. `LumeEngine/src/os/macos/CMakeLists.txt`
7. `LumeRender/src/vulkan/macos/platform_vk.cpp`
8. `LumeRender/src/vulkan/macos/platform_create_functions_vk.cpp`
9. `LumeRender/src/vulkan/macos/platform_device_vk.cpp`
10. `LumeRender/src/vulkan/macos/platform_gpu_buffer_vk.cpp`
11. `LumeRender/src/vulkan/macos/platform_gpu_image_vk.cpp`
12. `LumeRender/src/vulkan/macos/platform_hardware_buffer_util_vk.cpp`
13. `LumeRender/src/vulkan/macos/CMakeLists.txt`
14. `include/platform_export.h` (跨平台导出宏)
15. `build_macos.sh`

### 修改文件（约 8 个）
1. `CMakeLists.txt` - 根目录
2. `LumeEngine/CMakeLists.txt`
3. `LumeRender/CMakeLists.txt`
4. `Lume3D/CMakeLists.txt`
5. `src/main.cpp`
6. `README.md`
7. `AGENTS.md`

## 依赖安装

### macOS  prerequisites
```bash
# 安装 Homebrew（如果还没有）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装依赖
brew install cmake
brew install molten-vk
brew install glfw
```

### Vulkan SDK
从 https://vulkan.lunarg.com/sdk/home 下载 macOS 版本的 Vulkan SDK

## 构建命令

```bash
# 配置
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build build --config Release

# 运行
./build/Release/LumeDemo
```

## 风险评估

### 低风险
- ✅ CMake 构建系统成熟
- ✅ GLFW 完全支持 macOS
- ✅ 已有 OHOS 实现作为参考
- ✅ C++17 完全支持

### 中风险
- ⚠️ MoltenVK 可能有性能损失
- ⚠️ 某些 Vulkan 扩展可能不支持
- ⚠️ 文件路径处理需要仔细测试

### 高风险
- ❌ 无（技术方案可行）

## 时间估计

| 阶段 | 人工开发 | Agent 开发 |
|------|---------|-----------|
| Phase 1: 基础框架 | 2-3 小时 | 30 分钟 |
| Phase 2: LumeEngine | 3-4 小时 | 1 小时 |
| Phase 3: LumeRender | 3-4 小时 | 1 小时 |
| Phase 4: 构建系统 | 2-3 小时 | 30 分钟 |
| Phase 5: 主程序 | 1-2 小时 | 15 分钟 |
| Phase 6: 脚本文档 | 1 小时 | 15 分钟 |
| Phase 7: 测试修复 | 2-4 小时 | 1-2 小时 |
| **总计** | **14-21 小时** | **4-6 小时** |

## 成功标准

- [ ] 在 macOS 上成功配置 CMake
- [ ] 无编译错误
- [ ] 成功链接 MoltenVK
- [ ] 窗口正常创建
- [ ] Vulkan 成功初始化
- [ ] 3D 场景正常渲染
- [ ] Windows 版本仍然正常工作

## 下一步

确认开始移植后，我将：
1. 立即开始 Phase 1
2. 并行处理多个文件
3. 每完成一个阶段提交到 git
4. 最后统一测试

**准备好了吗？我可以立即开始！**
