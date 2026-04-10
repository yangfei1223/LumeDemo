#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(RENDER_HAS_VULKAN_BACKEND)
#define GLFW_INCLUDE_VULKAN
#include <render/vulkan/intf_device_vk.h>
#else
#define GLFW_INCLUDE_NONE
#endif

#include <GLFW/glfw3.h>

#include <memory>
#include <cstring>
#include <cstdio>

#include <core/io/intf_file_manager.h>
#include <core/log.h>
#include <core/os/platform_create_info.h>
#include <core/plugin/intf_plugin_register.h>
#include <render/device/intf_device.h>

#include "application_config.h"
#include "application_factory.h"
#include "application_interface.h"
#include "screenshot.h"

namespace {
    uint64_t CreateSurface(std::unique_ptr<IApplication>& app, GLFWwindow* windowHandle, RENDER_NS::IDevice* device)
    {
        constexpr uint64_t invalidSurfaceHandle = 0;
#if RENDER_HAS_VULKAN_BACKEND
        if (device->GetBackendType() != RENDER_NS::DeviceBackendType::VULKAN) {
            app->OnStop();
            return invalidSurfaceHandle;
        }
        const auto& platformData = static_cast<const RENDER_NS::DevicePlatformDataVk&>(device->GetPlatformData());
        VkSurfaceKHR surface { VK_NULL_HANDLE };
        if (glfwCreateWindowSurface(platformData.instance, windowHandle, nullptr, &surface) != VK_SUCCESS) {
            app->OnStop();
            return invalidSurfaceHandle;
        }
        return reinterpret_cast<uint64_t>(surface);
#else
        app->OnStop();
        return invalidSurfaceHandle;
#endif
    }

    void DestroySurface(RENDER_NS::IDevice* device, uint64_t surfaceHandle)
    {
#if RENDER_HAS_VULKAN_BACKEND
        if (surfaceHandle == 0) {
            return;
        }
        const auto& platformData = static_cast<const RENDER_NS::DevicePlatformDataVk&>(device->GetPlatformData());
        vkDestroySurfaceKHR(platformData.instance, reinterpret_cast<VkSurfaceKHR>(surfaceHandle), nullptr);
#else
        (void)device;
        (void)surfaceHandle;
#endif
    }
}

void RegisterAppPaths(CORE_NS::IEngine& engine)
{
    auto& fileManager = engine.GetFileManager();

    const BASE_NS::string appDirectory = "file://app/";
    fileManager.RegisterPath("app", appDirectory, true);
    if (fileManager.OpenDirectory(appDirectory) == nullptr) {
        const auto _ = fileManager.CreateDirectory(appDirectory);
    }

    const BASE_NS::string cacheDirectory = "app://cache/";
    fileManager.RegisterPath("cache", cacheDirectory, true);
    if (fileManager.OpenDirectory(cacheDirectory) == nullptr) {
        const auto _ = fileManager.CreateDirectory(cacheDirectory);
    }

    const BASE_NS::string sharedDirectory = "app://shared/";
    fileManager.RegisterPath("shared", sharedDirectory, true);
    if (fileManager.OpenDirectory(sharedDirectory) == nullptr) {
        const auto _ = fileManager.CreateDirectory(sharedDirectory);
    }

    const BASE_NS::string assetsDirectory = "app://assets/";
    fileManager.RegisterPath("assets", assetsDirectory, true);
    if (fileManager.OpenDirectory(assetsDirectory) == nullptr) {
        const auto _ = fileManager.CreateDirectory(assetsDirectory);
    }
}

// GLFW input callbacks
static void MouseMoveCallback(GLFWwindow* window, double x, double y)
{
    auto* app = static_cast<IApplication*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->OnMouseMove(x, y);
    }
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* app = static_cast<IApplication*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->OnMouseButton(button, action, mods);
    }
}

static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* app = static_cast<IApplication*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->OnMouseScroll(xoffset, yoffset);
    }
}

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Write to file for debugging
    FILE* f = fopen("key_debug.txt", "a");
    if (f) {
        fprintf(f, "KeyCallback: key=%d scancode=%d action=%d mods=%d\n", key, scancode, action, mods);
        fclose(f);
    }
    
    auto* app = static_cast<IApplication*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->OnKey(key, scancode, action, mods);
    }
}

int main(int argc, char* argv[]) {
    constexpr int width = 1600;
    constexpr int height = 900;
    
    // Parse command line arguments
    bool autoTest = false;
    int testFrames = 10;
    const char* outputFile = "test_output.ppm";
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            autoTest = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            testFrames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputFile = argv[++i];
        }
    }
    
    if (autoTest) {
        CORE_LOG_I("=== AUTO TEST MODE ===");
        CORE_LOG_I("Will render %d frames and exit", testFrames);
    }

    const CORE_NS::PlatformCreateInfo platformCreateInfo{};
    CORE_NS::CreatePluginRegistry(platformCreateInfo);

    if (!glfwInit()) {
        return -1;
    }

#if RENDER_HAS_VULKAN_BACKEND
    if (!glfwVulkanSupported()) {
        glfwTerminate();
        return -1;
    }
#endif

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(width, height, applicationName, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    std::unique_ptr<IApplication> app(createApplication());
    
    // Set up input callbacks
    glfwSetWindowUserPointer(window, app.get());
    glfwSetCursorPosCallback(window, MouseMoveCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetKeyCallback(window, KeyCallback);
    
    RENDER_NS::IDevice* device = app->OnInit(platformCreateInfo);

    RENDER_NS::SwapchainCreateInfo swapchainCreateInfo;
    swapchainCreateInfo.surfaceHandle = CreateSurface(app, window, device);
    swapchainCreateInfo.swapchainFlags = RENDER_NS::SwapchainFlagBits::CORE_SWAPCHAIN_COLOR_BUFFER_BIT |
                                         RENDER_NS::SwapchainFlagBits::CORE_SWAPCHAIN_DEPTH_BUFFER_BIT |
                                         RENDER_NS::SwapchainFlagBits::CORE_SWAPCHAIN_VSYNC_BIT |
                                         RENDER_NS::SwapchainFlagBits::CORE_SWAPCHAIN_SRGB_BIT;
    
    app->OnWindowUpdate(swapchainCreateInfo, width, height);
    app->OnStart();

    int frameCount = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        app->OnFrame();
        frameCount++;
        
        // Auto test mode: exit after N frames
        if (autoTest && frameCount >= testFrames) {
            CORE_LOG_I("=== AUTO TEST COMPLETE: Rendered %d frames ===", frameCount);
            CORE_LOG_I("=== Note: Use Vulkan readback for proper screenshot ===");
            break;
        }
    }

    app->OnStop();
    app->OnWindowDestroy();
    DestroySurface(device, swapchainCreateInfo.surfaceHandle);
    app.reset();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}