#ifndef DIFF_TEXTURE_SR_H
#define DIFF_TEXTURE_SR_H

#include <base/containers/vector.h>
#include <base/math/vector.h>
#include <base/namespace.h>
#include <core/namespace.h>
#include <render/namespace.h>
#include <render/resource_handle.h>
#include <render/device/intf_gpu_resource_manager.h>
#include <render/intf_render_context.h>
#include <core/ecs/intf_ecs.h>

// Forward declarations
namespace RENDER_NS {
    class IRenderCommandList;
    class IShaderManager;
}

namespace CORE3D_NS {
    class IGraphicsContext;
}

namespace LumeDemo {

using namespace BASE_NS;
using namespace CORE_NS;
using namespace RENDER_NS;
using namespace CORE3D_NS;

// Texture pair: High-res Ground Truth + Low-res optimization target
struct TexturePair {
    // Ground Truth (high-res, read-only)
    RenderHandleReference gtTexture;
    uint32_t gtWidth = 1024;
    uint32_t gtHeight = 1024;
    
    // Optimization target (low-res, differentiable)
    RenderHandleReference lrTexture;      // Current value (RGBA8)
    RenderHandleReference lrGradient;     // Gradient (RGBA32F)
    RenderHandleReference lrMomentum1;    // Adam 1st moment (RGBA32F)
    RenderHandleReference lrMomentum2;    // Adam 2nd moment (RGBA32F)
    uint32_t lrWidth = 512;
    uint32_t lrHeight = 512;
    
    // Iteration counter
    uint32_t iteration = 0;
    
    void Initialize(IGpuResourceManager& gpuResMgr);
    void DownsampleFromGT(IRenderCommandList& cmdList, 
                          IShaderManager& shaderMgr,
                          RenderHandleReference sampler);
};

// Differentiable Texture Super-Resolution Manager
class DiffTextureSRManager {
public:
    void Initialize(IRenderContext& renderContext,
                    IEcs& ecs,
                    IGraphicsContext& graphicsContext);
    
    // Set texture pair
    void SetTexturePair(const TexturePair& pair) { texturePair_ = pair; }
    TexturePair& GetTexturePair() { return texturePair_; }
    
    // Set reference render targets
    void SetReferenceRenderTarget(RenderHandleReference gtTarget,
                                   RenderHandleReference optTarget);
    
    // Random camera generation
    void RandomizeCamera(float distanceMin = 2.0f, 
                          float distanceMax = 5.0f);
    
    // Update camera transform
    void UpdateCameraTransform(Entity cameraEntity);
    
    // Execute optimization step
    void OptimizeStep();
    
    // Get current loss value
    float GetCurrentLoss() const { return currentLoss_; }
    
    // Training state
    bool IsTraining() const { return isTraining_; }
    void SetTraining(bool training) { isTraining_ = training; }
    
    // Adam parameters
    struct AdamParams {
        float learningRate = 0.01f;
        float beta1 = 0.9f;
        float beta2 = 0.999f;
        float epsilon = 1e-8f;
    };
    void SetAdamParams(const AdamParams& params) { adamParams_ = params; }
    const AdamParams& GetAdamParams() const { return adamParams_; }
    
    // Resolution settings
    void SetResolution(uint32_t gtW, uint32_t gtH, 
                        uint32_t lrW, uint32_t lrH);
    
    // Get render targets
    RenderHandleReference GetGTRenderTarget() const { return gtRenderTarget_; }
    RenderHandleReference GetOptimizedRenderTarget() const { return optRenderTarget_; }
    RenderHandleReference GetUVRenderTarget() const { return uvRenderTarget_; }
    
    // Get camera parameters
    float GetCameraYaw() const { return cameraYaw_; }
    float GetCameraPitch() const { return cameraPitch_; }
    float GetCameraDistance() const { return cameraDistance_; }
    Math::Vec3 GetCameraTarget() const { return cameraTarget_; }
    
    // Compute shader dispatch (for render node calls)
    void DispatchLossBackward(IRenderCommandList& cmdList);
    void DispatchAdamOptimizer(IRenderCommandList& cmdList);
    
private:
    void CreateRenderTargets();
    
    IRenderContext* renderContext_ = nullptr;
    IGpuResourceManager* gpuResMgr_ = nullptr;
    IShaderManager* shaderMgr_ = nullptr;
    IEcs* ecs_ = nullptr;
    IGraphicsContext* graphicsContext_ = nullptr;
    
    TexturePair texturePair_;
    RenderHandleReference gtRenderTarget_;   // Ground Truth render result
    RenderHandleReference optRenderTarget_;  // Optimized render result
    RenderHandleReference uvRenderTarget_;   // UV output
    RenderHandleReference sampler_;          // Shared sampler
    
    AdamParams adamParams_;
    float currentLoss_ = 0.0f;
    bool isTraining_ = false;
    
    // Camera parameters
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.0f;
    float cameraDistance_ = 3.0f;
    Math::Vec3 cameraTarget_ = Math::Vec3(0.0f, 0.0f, 0.0f);
};

} // namespace LumeDemo

#endif // DIFF_TEXTURE_SR_H