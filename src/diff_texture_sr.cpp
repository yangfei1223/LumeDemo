#include "diff_texture_sr.h"

#include <render/device/intf_gpu_resource_manager.h>
#include <render/device/intf_shader_manager.h>
#include <render/device/gpu_resource_desc.h>
#include <render/nodecontext/intf_render_command_list.h>
#include <render/nodecontext/intf_pipeline_descriptor_set_binder.h>
#include <render/intf_renderer.h>
#include <render/intf_render_context.h>
#include <core/log.h>
#include <3d/ecs/components/camera_component.h>
#include <3d/ecs/components/transform_component.h>
#include <3d/ecs/systems/intf_node_system.h>
#include <3d/util/intf_scene_util.h>
#include <3d/intf_graphics_context.h>

#include <random>
#include <chrono>
#include <cstring>

namespace LumeDemo {

namespace {
    // Push constant structures matching shader layouts
    struct DownsamplePushConstant {
        BASE_NS::Math::Vec2 sourceSize;
        BASE_NS::Math::Vec2 destSize;
    };

    struct LossBackwardPushConstant {
        BASE_NS::Math::Vec2 lrTexSize;
        BASE_NS::Math::Vec2 screenSize;
        float downsampleRatio;
    };

    struct AdamOptimizerPushConstant {
        float learningRate;
        float beta1;
        float beta2;
        float epsilon;
        int iteration;
    };
}

void TexturePair::Initialize(IGpuResourceManager& gpuResMgr) {
    // Create low-resolution texture resources
    {
        GpuImageDesc desc;
        desc.imageType = CORE_IMAGE_TYPE_2D;
        desc.format = BASE_NS::Format::BASE_FORMAT_R8G8B8A8_UNORM;
        desc.width = lrWidth;
        desc.height = lrHeight;
        desc.depth = 1;
        desc.layerCount = 1;
        desc.mipCount = 1;
        desc.sampleCountFlags = CORE_SAMPLE_COUNT_1_BIT;
        desc.usageFlags = CORE_IMAGE_USAGE_SAMPLED_BIT |
                     CORE_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     CORE_IMAGE_USAGE_STORAGE_BIT;
        lrTexture = gpuResMgr.Create(desc);
    }
    
    // Create gradient image (RGBA32F)
    {
        GpuImageDesc desc;
        desc.imageType = CORE_IMAGE_TYPE_2D;
        desc.format = BASE_NS::Format::BASE_FORMAT_R32G32B32A32_SFLOAT;
        desc.width = lrWidth;
        desc.height = lrHeight;
        desc.usageFlags = CORE_IMAGE_USAGE_STORAGE_BIT;
        lrGradient = gpuResMgr.Create(desc);
    }
    
    // Create Adam momentum images
    {
        GpuImageDesc desc;
        desc.imageType = CORE_IMAGE_TYPE_2D;
        desc.format = BASE_NS::Format::BASE_FORMAT_R32G32B32A32_SFLOAT;
        desc.width = lrWidth;
        desc.height = lrHeight;
        desc.usageFlags = CORE_IMAGE_USAGE_STORAGE_BIT;
        lrMomentum1 = gpuResMgr.Create(desc);
        lrMomentum2 = gpuResMgr.Create(desc);
    }
}

void TexturePair::DownsampleFromGT(IRenderCommandList& cmdList,
                                   IShaderManager& shaderMgr,
                                   RenderHandleReference sampler) {
    // Note: Downsampling is performed via RenderNodeComputeGeneric
    // The render node graph handles pipeline creation, binding, and dispatch
}

void DiffTextureSRManager::Initialize(IRenderContext& renderContext,
                                      IEcs& ecs,
                                      IGraphicsContext& graphicsContext) {
    renderContext_ = &renderContext;
    gpuResMgr_ = &renderContext.GetDevice().GetGpuResourceManager();
    shaderMgr_ = &renderContext.GetDevice().GetShaderManager();
    ecs_ = &ecs;
    graphicsContext_ = &graphicsContext;
    
    // Create sampler for texture operations
    GpuSamplerDesc samplerDesc;
    samplerDesc.minFilter = CORE_FILTER_LINEAR;
    samplerDesc.magFilter = CORE_FILTER_LINEAR;
    samplerDesc.addressModeU = CORE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.addressModeV = CORE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ = gpuResMgr_->Create(samplerDesc);
    
    // Initialize texture pair
    texturePair_.Initialize(*gpuResMgr_);
    
    // Create render targets for GT and optimized views
    CreateRenderTargets();
    
    CORE_LOG_I("DiffTextureSRManager initialized");
}

void DiffTextureSRManager::CreateRenderTargets() {
    // GT render target (full resolution)
    {
        GpuImageDesc desc;
        desc.imageType = CORE_IMAGE_TYPE_2D;
        desc.format = BASE_NS::Format::BASE_FORMAT_R8G8B8A8_UNORM;
        desc.width = texturePair_.gtWidth;
        desc.height = texturePair_.gtHeight;
        desc.usageFlags = CORE_IMAGE_USAGE_SAMPLED_BIT |
                     CORE_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     CORE_IMAGE_USAGE_STORAGE_BIT;
        gtRenderTarget_ = gpuResMgr_->Create(desc);
    }
    
    // Optimized render target (full resolution)
    {
        GpuImageDesc desc;
        desc.imageType = CORE_IMAGE_TYPE_2D;
        desc.format = BASE_NS::Format::BASE_FORMAT_R8G8B8A8_UNORM;
        desc.width = texturePair_.gtWidth;
        desc.height = texturePair_.gtHeight;
        desc.usageFlags = CORE_IMAGE_USAGE_SAMPLED_BIT |
                     CORE_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     CORE_IMAGE_USAGE_STORAGE_BIT;
        optRenderTarget_ = gpuResMgr_->Create(desc);
    }
    
    // UV render target for differentiable rendering
    {
        GpuImageDesc desc;
        desc.imageType = CORE_IMAGE_TYPE_2D;
        desc.format = BASE_NS::Format::BASE_FORMAT_R32G32B32A32_SFLOAT;
        desc.width = texturePair_.gtWidth;
        desc.height = texturePair_.gtHeight;
        desc.usageFlags = CORE_IMAGE_USAGE_SAMPLED_BIT |
                     CORE_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     CORE_IMAGE_USAGE_STORAGE_BIT;
        uvRenderTarget_ = gpuResMgr_->Create(desc);
    }
}

void DiffTextureSRManager::RandomizeCamera(float distanceMin, float distanceMax) {
    static std::mt19937 rng(static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count()));
    
    std::uniform_real_distribution<float> yawDist(0.0f, 2.0f * BASE_NS::Math::PI);
    std::uniform_real_distribution<float> pitchDist(-BASE_NS::Math::PI / 3.0f, BASE_NS::Math::PI / 3.0f);
    std::uniform_real_distribution<float> distDist(distanceMin, distanceMax);
    
    cameraYaw_ = yawDist(rng);
    cameraPitch_ = pitchDist(rng);
    cameraDistance_ = distDist(rng);
}

void DiffTextureSRManager::UpdateCameraTransform(Entity cameraEntity) {
    if (!ecs_) return;
    
    auto* nodeSystem = GetSystem<CORE3D_NS::INodeSystem>(*ecs_);
    auto* transformMgr = GetManager<CORE3D_NS::ITransformComponentManager>(*ecs_);
    
    if (!transformMgr || cameraEntity == Entity {}) return;
    
    // Calculate camera position from spherical coordinates
    float x = cameraDistance_ * BASE_NS::Math::cos(cameraPitch_) * BASE_NS::Math::sin(cameraYaw_);
    float y = cameraDistance_ * BASE_NS::Math::sin(cameraPitch_);
    float z = cameraDistance_ * BASE_NS::Math::cos(cameraPitch_) * BASE_NS::Math::cos(cameraYaw_);
    
    BASE_NS::Math::Vec3 cameraPos = cameraTarget_ + BASE_NS::Math::Vec3(x, y, z);
    
    auto handle = transformMgr->Write(cameraEntity);
    if (handle) {
        handle->position = cameraPos;
        
        // Calculate rotation to look at target
        BASE_NS::Math::Vec3 forward = BASE_NS::Math::Normalize(cameraTarget_ - cameraPos);
        BASE_NS::Math::Vec3 right = BASE_NS::Math::Normalize(BASE_NS::Math::Cross(forward, BASE_NS::Math::Vec3(0.0f, 1.0f, 0.0f)));
        BASE_NS::Math::Vec3 up = BASE_NS::Math::Cross(right, forward);
        
        // Convert to quaternion
        float trace = right.x + up.y + forward.z;
        if (trace > 0.0f) {
            float s = BASE_NS::Math::sqrt(trace + 1.0f) * 2.0f;
            handle->rotation.w = 0.25f * s;
            handle->rotation.x = (up.z - forward.y) / s;
            handle->rotation.y = (forward.x - right.z) / s;
            handle->rotation.z = (right.y - up.x) / s;
        } else if (right.x > up.y && right.x > forward.z) {
            float s = BASE_NS::Math::sqrt(1.0f + right.x - up.y - forward.z) * 2.0f;
            handle->rotation.w = (up.z - forward.y) / s;
            handle->rotation.x = 0.25f * s;
            handle->rotation.y = (up.x + right.y) / s;
            handle->rotation.z = (forward.x + right.z) / s;
        } else if (up.y > forward.z) {
            float s = BASE_NS::Math::sqrt(1.0f + up.y - right.x - forward.z) * 2.0f;
            handle->rotation.w = (forward.x - right.z) / s;
            handle->rotation.x = (up.x + right.y) / s;
            handle->rotation.y = 0.25f * s;
            handle->rotation.z = (forward.y + up.z) / s;
        } else {
            float s = BASE_NS::Math::sqrt(1.0f + forward.z - right.x - up.y) * 2.0f;
            handle->rotation.w = (right.y - up.x) / s;
            handle->rotation.x = (forward.x + right.z) / s;
            handle->rotation.y = (forward.y + up.z) / s;
            handle->rotation.z = 0.25f * s;
        }
    }
}

void DiffTextureSRManager::OptimizeStep() {
    if (!isTraining_ || !renderContext_) return;
    
    // Step 1: Randomize camera for this training iteration
    RandomizeCamera();
    
    // Step 2: Update camera transform in ECS
    // Note: This requires camera entity to be passed or stored
    // The actual rendering will be done by the render graph
    
    // Step 3: Increment iteration counter
    texturePair_.iteration++;
    
    // Note: The actual compute shader dispatches (Loss + Adam) are handled
    // by the RenderNodeComputeGeneric nodes in the render node graph.
    // The render node graph is loaded and executed by the renderer.
    
    CORE_LOG_I("OptimizeStep: iteration %d", texturePair_.iteration);
}

void DiffTextureSRManager::DispatchLossBackward(IRenderCommandList& cmdList) {
    // Note: This is handled automatically by RenderNodeComputeGeneric
    // The render node graph configures and executes this dispatch
}

void DiffTextureSRManager::DispatchAdamOptimizer(IRenderCommandList& cmdList) {
    // Note: This is handled automatically by RenderNodeComputeGeneric
    // The render node graph configures and executes this dispatch
}

void DiffTextureSRManager::SetResolution(uint32_t gtW, uint32_t gtH, 
                                         uint32_t lrW, uint32_t lrH) {
    texturePair_.gtWidth = gtW;
    texturePair_.gtHeight = gtH;
    texturePair_.lrWidth = lrW;
    texturePair_.lrHeight = lrH;
    
    // Recreate render targets with new resolution
    if (gpuResMgr_) {
        CreateRenderTargets();
    }
}

} // namespace LumeDemo
