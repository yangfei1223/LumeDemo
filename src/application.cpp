#include <algorithm>
#include <cinttypes>

#include <base/math/mathf.h>
#include <base/math/quaternion.h>
#include <base/math/quaternion_util.h>
#include <base/math/vector.h>
#include <base/math/vector_util.h>
#include <core/ecs/intf_entity_manager.h>
#include <core/ecs/intf_system_graph_loader.h>
#include <core/engine_info.h>
#include <core/image/intf_image_loader_manager.h>
#include <core/implementation_uids.h>
#include <core/intf_engine.h>
#include <core/io/intf_file_manager.h>
#include <core/log.h>
#include <core/plugin/intf_plugin.h>
#include <core/plugin/intf_plugin_register.h>
#include <render/datastore/intf_render_data_store_manager.h>
#include <render/datastore/intf_render_data_store_pod.h>
#include <render/device/intf_device.h>
#include <render/device/intf_gpu_resource_manager.h>
#include <render/implementation_uids.h>
#include <render/intf_render_context.h>
#include <render/intf_renderer.h>
#include <render/nodecontext/intf_render_node_graph_manager.h>
#include <render/resource_handle.h>
#if RENDER_HAS_VULKAN_BACKEND
#include <render/vulkan/intf_device_vk.h>
#endif
#include <3d/ecs/components/camera_component.h>
#include <3d/ecs/components/environment_component.h>
#include <3d/ecs/components/fog_component.h>
#include <3d/ecs/components/light_component.h>
#include <3d/ecs/components/material_component.h>
#include <3d/ecs/components/name_component.h>
#include <3d/ecs/components/render_configuration_component.h>
#include <3d/ecs/components/render_handle_component.h>
#include <3d/ecs/components/transform_component.h>
#include <3d/ecs/systems/intf_animation_system.h>
#include <3d/ecs/systems/intf_node_system.h>
#include <3d/implementation_uids.h>
#include <3d/intf_graphics_context.h>
#include <3d/loaders/intf_scene_loader.h>
#include <3d/util/intf_mesh_util.h>
#include <3d/util/intf_picking.h>
#include <3d/util/intf_scene_util.h>

#include "application_config.h"
#include "application_factory.h"
#include "application_interface.h"
#include "diff_texture_sr.h"

using namespace BASE_NS;
using namespace CORE_NS;
using namespace RENDER_NS;
using namespace CORE3D_NS;
using namespace LumeDemo;

class MinimalDemo : public IApplication {
public:
    MinimalDemo() {}
    
    ~MinimalDemo() override = default;

    // Texture Super-Resolution Manager
    DiffTextureSRManager srManager_;
    bool isTraining_ = false;
    uint32_t frameCount_ = 0;
    bool gtTextureInitialized_ = false;  // Flag to track GT texture initialization

    IDevice* OnInit(PlatformCreateInfo platformCreateInfo) override
    {
        // Engine
        const EngineCreateInfo engineCreateInfo{};
        auto factory = GetInstance<IEngineFactory>(UID_ENGINE_FACTORY);
        engine_ = factory->Create(engineCreateInfo);
        RegisterAppPaths(*engine_);
        engine_->Init();
        
        ecs_ = engine_->CreateEcs();

        // Render
        constexpr Uid uidRender[] = { UID_RENDER_PLUGIN };
        GetPluginRegister().LoadPlugins(uidRender);
        renderContext_ = static_cast<IRenderContext::Ptr>(engine_->GetInterface<IClassFactory>()->CreateInstance(UID_RENDER_CONTEXT));
        DeviceCreateInfo deviceCreateInfo;
#if RENDER_HAS_VULKAN_BACKEND
        BackendExtraVk vkExtra;
        deviceCreateInfo.backendType = DeviceBackendType::VULKAN;
        deviceCreateInfo.backendConfiguration = &vkExtra;
#endif
        RenderCreateInfo renderCreateInfo{};
        renderCreateInfo.deviceCreateInfo = deviceCreateInfo;
        IDevice* device = nullptr;
        const RenderResultCode rcc = renderContext_->Init(renderCreateInfo);
        if (rcc == RenderResultCode::RENDER_SUCCESS) {
            device = &(renderContext_->GetDevice());
        }

        // 3D
        constexpr Uid uid3D[] = { UID_3D_PLUGIN };
        GetPluginRegister().LoadPlugins(uid3D);
        graphicsContext_ = CreateInstance<IGraphicsContext>(*renderContext_->GetInterface<IClassFactory>(), UID_GRAPHICS_CONTEXT);
        graphicsContext_->Init();

        // Initialize Texture Super-Resolution Manager
        srManager_.Initialize(*renderContext_, *ecs_, *graphicsContext_);
        srManager_.SetTraining(false);  // Training off by default
        CORE_LOG_I("DiffTextureSRManager initialized");

        // Run self-test and write results to file
        srManager_.RunSelfTest("sr_test_results.txt");

        // Run Phase 4 compute shader test
        srManager_.RunComputeShaderTest("sr_compute_test_results.txt");

        return device;
    }

    void OnWindowUpdate(SwapchainCreateInfo swapchainCreateInfo, int width, int height) override
    {
        if (width > 0 && height > 0) {
            windowWidth_ = width;
            windowHeight_ = height;
            const auto& sceneUtil = graphicsContext_->GetSceneUtil();
            sceneUtil.UpdateCameraViewport(*ecs_, activeCamera_, { windowWidth_, windowHeight_ }, autoAspect_, originalFov_, orthoScale_);
            renderContext_->GetDevice().CreateSwapchain(swapchainCreateInfo);
        }
        else {
            renderContext_->GetDevice().DestroySwapchain();
        }
    }

    void OnWindowDestroy() override
    {
        renderContext_->GetDevice().DestroySwapchain();
    }

    void OnStart() override
    {
        ecs_->Initialize();
        transformManager_ = GetManager<ITransformComponentManager>(*ecs_);
        cameraManager_ = GetManager<ICameraComponentManager>(*ecs_);
        renderNodeGraph_ = CreateRenderNodeGraph("assets://app/renderNodeGraph.json");
        {
            auto* nodeSystem = GetSystem<INodeSystem>(*ecs_);
            auto rootNode = nodeSystem->CreateNode();
            rootNodeEntity_ = rootNode->GetEntity();
            auto rccm = GetManager<IRenderConfigurationComponentManager>(*ecs_);
            rccm->Create(rootNodeEntity_);
            auto handle = rccm->Write(rootNodeEntity_);
            RenderConfigurationComponent& renderConfig = *handle;
            renderConfig.environment = ecs_->GetEntityManager().Create();
            auto ecm = GetManager<IEnvironmentComponentManager>(*ecs_);
            ecm->Create(renderConfig.environment);
            auto envHandle = ecm->Write(renderConfig.environment);
            EnvironmentComponent& envComponent = *envHandle;
            envComponent.background = EnvironmentComponent::Background::CUBEMAP;
        }
        {
            const auto& sceneUtil = graphicsContext_->GetSceneUtil();
            cameraEntity_ = sceneUtil.CreateCamera(*ecs_, Math::Vec3(0.f, 0.f, 3.f), {}, 0.1f, 1000.f, 60.f);
            activeCamera_ = cameraEntity_;
        }
        {
            const char* filename = "assets://glTF/DamagedHelmet/glTF/DamagedHelmet.gltf";
            auto loader = graphicsContext_->GetSceneUtil().GetSceneLoader(filename);
            auto result = loader->Load(filename);
            auto importer = loader->CreateSceneImporter(*ecs_);
            importer->ImportResources(result.data, CORE_IMPORT_RESOURCE_FLAG_BITS_ALL);
            const auto& importResult = importer->GetResult();
            importedResources_.push_back(importResult.data);
            importer->ImportScene(0, rootNodeEntity_, CORE_IMPORT_COMPONENT_FLAG_BITS_ALL);
        }
    }

    void OnStop() override {}

    void OnFrame() override
    {
        // Training loop
        if (srManager_.IsTraining()) {
            srManager_.OptimizeStep();
            
            // Update camera every 10 frames
            frameCount_++;
            if (frameCount_ % 10 == 0) {
                srManager_.UpdateCameraTransform(cameraEntity_);
            }
        }
        
        UpdateCamera();
        auto* ecs = ecs_.get();
        const bool needRender = engine_->TickFrame(array_view(&ecs, 1));
        if (needRender) {
            IRenderer& renderer = renderContext_->GetRenderer();
            const auto ecsRngs = graphicsContext_->GetRenderNodeGraphs(*ecs);
            vector<RenderHandleReference> rngs(ecsRngs.begin(), ecsRngs.end());
            renderer.RenderFrame(rngs);
            
            // Initialize GT texture from base_color output after first frame
            if (!gtTextureInitialized_ && renderNodeGraph_) {
                IRenderNodeGraphManager& graphManager = renderContext_->GetRenderNodeGraphManager();
                auto resourceInfo = graphManager.GetRenderNodeGraphResources(renderNodeGraph_);
                
                // base_color is at index 3 in output resources (output, color, depth, base_color)
                if (resourceInfo.outputResources.size() > 3) {
                    auto baseColorHandle = resourceInfo.outputResources[3];
                    if (baseColorHandle && baseColorHandle.GetHandle().id != 0) {
                        srManager_.SetGTTextureFromRenderOutput(baseColorHandle);
                        gtTextureInitialized_ = true;
                        CORE_LOG_I("OnFrame: GT texture initialized from base_color output");
                    }
                }
            }
        }
    }

    // Input handling
    void OnMouseMove(double x, double y) override
    {
        if (mouseLeftDown_) {
            float dx = static_cast<float>(x - lastMouseX_) * rotateSensitivity_;
            float dy = static_cast<float>(y - lastMouseY_) * rotateSensitivity_;
            
            cameraYaw_ += dx;
            cameraPitch_ += dy;
            cameraPitch_ = Math::clamp(cameraPitch_, -Math::PI / 2.0f + 0.1f, Math::PI / 2.0f - 0.1f);
            
            UpdateCameraTransform();
        } else if (mouseRightDown_) {
            float dx = static_cast<float>(x - lastMouseX_) * panSensitivity_ * cameraDistance_;
            float dy = static_cast<float>(y - lastMouseY_) * panSensitivity_ * cameraDistance_;
            
            // Calculate right and up vectors
            Math::Vec3 forward(
                Math::cos(cameraPitch_) * Math::sin(cameraYaw_),
                Math::sin(cameraPitch_),
                Math::cos(cameraPitch_) * Math::cos(cameraYaw_)
            );
            forward = Math::Normalize(forward);
            
            Math::Vec3 right = Math::Normalize(Math::Cross(forward, Math::Vec3(0.0f, 1.0f, 0.0f)));
            Math::Vec3 up = Math::Normalize(Math::Cross(right, forward));
            
            cameraTarget_ += right * dx - up * dy;
            UpdateCameraTransform();
        }
        
        lastMouseX_ = x;
        lastMouseY_ = y;
    }

    void OnMouseButton(int button, int action, int mods) override
    {
        if (button == 0) { // GLFW_MOUSE_BUTTON_LEFT
            mouseLeftDown_ = (action == 1); // GLFW_PRESS
        } else if (button == 1) { // GLFW_MOUSE_BUTTON_RIGHT
            mouseRightDown_ = (action == 1); // GLFW_PRESS
        }
    }

    void OnMouseScroll(double xoffset, double yoffset) override
    {
        cameraDistance_ -= static_cast<float>(yoffset) * zoomSensitivity_ * cameraDistance_;
        cameraDistance_ = Math::clamp(cameraDistance_, 0.1f, 100.0f);
        UpdateCameraTransform();
    }

    void OnKey(int key, int scancode, int action, int mods) override
    {
        // Reset camera position (R key = 82)
        if (key == 82 && action == 1) { // GLFW_KEY_R, GLFW_PRESS
            cameraDistance_ = 3.0f;
            cameraYaw_ = 0.0f;
            cameraPitch_ = 0.0f;
            cameraTarget_ = Math::Vec3(0.0f, 0.0f, 0.0f);
            UpdateCameraTransform();
        }
        
        // Training control: T key = 84 - Toggle training
        if (key == 84 && action == 1) { // GLFW_KEY_T
            isTraining_ = !isTraining_;
            srManager_.SetTraining(isTraining_);
            CORE_LOG_I("Training: %s (iteration %d)", isTraining_ ? "ON" : "OFF", srManager_.GetIteration());
        }
        
        // S key = 83 - Single step training
        if (key == 83 && action == 1) { // GLFW_KEY_S
            srManager_.SetTraining(true);
            srManager_.OptimizeStep();
            srManager_.SetTraining(false);
            CORE_LOG_I("Single step training executed (iteration %d)", srManager_.GetIteration());
        }
    }

private:
    RenderHandleReference CreateRenderNodeGraph(const string_view rngPath)
    {
        IRenderNodeGraphManager& graphManager = renderContext_->GetRenderNodeGraphManager();

        auto loader = &graphManager.GetRenderNodeGraphLoader();
        auto const result = loader->Load(rngPath);
        if (!result.error.empty()) {
            return {};
        }
        return graphManager.Create(
            IRenderNodeGraphManager::RenderNodeGraphUsageType::RENDER_NODE_GRAPH_STATIC, result.desc
        );
    }

    void UpdateCamera()
    {
        if (updateCamera_ && cameraManager_) {
            updateCamera_ = false;
            activeCamera_ = cameraEntity_;
            auto cameraHandle = cameraManager_->Write(activeCamera_);
            if (cameraHandle) {
                cameraHandle->sceneFlags |= CameraComponent::SceneFlagBits::MAIN_CAMERA_BIT;
                cameraHandle->pipelineFlags |= CameraComponent::PipelineFlagBits::CLEAR_COLOR_BIT;
                cameraHandle->pipelineFlags |= CameraComponent::PipelineFlagBits::JITTER_BIT |
                                               CameraComponent::PipelineFlagBits::HISTORY_BIT |
                                               CameraComponent::PipelineFlagBits::VELOCITY_OUTPUT_BIT |
                                               CameraComponent::PipelineFlagBits::DEPTH_OUTPUT_BIT;
                cameraHandle->renderingPipeline = CameraComponent::RenderingPipeline::DEFERRED;
                
                // Log deferred rendering confirmation
                CORE_LOG_I("Using DEFERRED rendering pipeline");
            }
            const auto& sceneUtil = graphicsContext_->GetSceneUtil();
            sceneUtil.UpdateCameraViewport(*ecs_, activeCamera_, { windowWidth_, windowHeight_ }, false, 60.f, 1.f);
        }
    }

    void UpdateCameraTransform()
    {
        if (!transformManager_ || cameraEntity_ == Entity {}) {
            return;
        }
        
        // Calculate camera position from spherical coordinates
        float x = cameraDistance_ * Math::cos(cameraPitch_) * Math::sin(cameraYaw_);
        float y = cameraDistance_ * Math::sin(cameraPitch_);
        float z = cameraDistance_ * Math::cos(cameraPitch_) * Math::cos(cameraYaw_);
        
        Math::Vec3 cameraPos = cameraTarget_ + Math::Vec3(x, y, z);
        
        // Update transform
        auto handle = transformManager_->Write(cameraEntity_);
        if (handle) {
            handle->position = cameraPos;
            
            // Calculate rotation to look at target using quaternion
            Math::Vec3 forward = Math::Normalize(cameraTarget_ - cameraPos);
            Math::Vec3 right = Math::Normalize(Math::Cross(forward, Math::Vec3(0.0f, 1.0f, 0.0f)));
            Math::Vec3 up = Math::Cross(right, forward);
            
            // Create rotation from basis vectors
            // Using LookRotation-like approach
            float trace = right.x + up.y + forward.z;
            if (trace > 0.0f) {
                float s = Math::sqrt(trace + 1.0f) * 2.0f;
                handle->rotation.w = 0.25f * s;
                handle->rotation.x = (up.z - forward.y) / s;
                handle->rotation.y = (forward.x - right.z) / s;
                handle->rotation.z = (right.y - up.x) / s;
            } else if (right.x > up.y && right.x > forward.z) {
                float s = Math::sqrt(1.0f + right.x - up.y - forward.z) * 2.0f;
                handle->rotation.w = (up.z - forward.y) / s;
                handle->rotation.x = 0.25f * s;
                handle->rotation.y = (up.x + right.y) / s;
                handle->rotation.z = (forward.x + right.z) / s;
            } else if (up.y > forward.z) {
                float s = Math::sqrt(1.0f + up.y - right.x - forward.z) * 2.0f;
                handle->rotation.w = (forward.x - right.z) / s;
                handle->rotation.x = (up.x + right.y) / s;
                handle->rotation.y = 0.25f * s;
                handle->rotation.z = (forward.y + up.z) / s;
            } else {
                float s = Math::sqrt(1.0f + forward.z - right.x - up.y) * 2.0f;
                handle->rotation.w = (right.y - up.x) / s;
                handle->rotation.x = (forward.x + right.z) / s;
                handle->rotation.y = (forward.y + up.z) / s;
                handle->rotation.z = 0.25f * s;
            }
        }
    }

private:
    IEngine::Ptr engine_;
    IEcs::Ptr ecs_;
    IRenderContext::Ptr renderContext_;
    IGraphicsContext::Ptr graphicsContext_;
    RenderHandleReference renderNodeGraph_;
    uint32_t windowWidth_;
    uint32_t windowHeight_;
    bool autoAspect_;
    float originalFov_;
    float  orthoScale_;
    Entity activeCamera_;
    Entity rootNodeEntity_;
    Entity cameraEntity_;
    ITransformComponentManager* transformManager_;
    ICameraComponentManager* cameraManager_;
    vector<ResourceData> importedResources_;
    bool updateCamera_ = true;
    
    // Camera control
    float cameraDistance_ = 3.0f;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.0f;
    Math::Vec3 cameraTarget_ = Math::Vec3(0.0f, 0.0f, 0.0f);
    
    // Mouse state
    bool mouseLeftDown_ = false;
    bool mouseRightDown_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    
    // Sensitivity
    float rotateSensitivity_ = 0.005f;
    float zoomSensitivity_ = 0.1f;
    float panSensitivity_ = 0.01f;
};

IApplication* createApplication()
{
    return new MinimalDemo();
}

const char* applicationName = "Minimal Demo";