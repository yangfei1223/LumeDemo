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
#include <3d/namespace.h>

// Forward declarations
namespace RENDER_NS {
    class IRenderCommandList;
    class IShaderManager;
}

namespace Core3D {
    class IGraphicsContext;
}

namespace LumeDemo {

// Texture pair: High-res Ground Truth + Low-res optimization target
struct TexturePair {
    // Ground Truth (high-res, read-only)
    RENDER_NS::RenderHandleReference gtTexture;
    uint32_t gtWidth = 1024;
    uint32_t gtHeight = 1024;
    
    // Optimization target (low-res, differentiable)
    RENDER_NS::RenderHandleReference lrTexture;      // Current value (RGBA8)
    RENDER_NS::RenderHandleReference lrGradient;     // Gradient (RGBA32F)
    RENDER_NS::RenderHandleReference lrMomentum1;    // Adam 1st moment (RGBA32F)
    RENDER_NS::RenderHandleReference lrMomentum2;    // Adam 2nd moment (RGBA32F)
    uint32_t lrWidth = 512;
    uint32_t lrHeight = 512;
    
    // Iteration counter
    uint32_t iteration = 0;
    
    void Initialize(RENDER_NS::IGpuResourceManager& gpuResMgr);
    void DownsampleFromGT(RENDER_NS::IRenderCommandList& cmdList, 
                          RENDER_NS::IShaderManager& shaderMgr,
                          RENDER_NS::RenderHandleReference sampler);
};

// Differentiable Texture Super-Resolution Manager
class DiffTextureSRManager {
public:
    void Initialize(RENDER_NS::IRenderContext& renderContext,
                    CORE_NS::IEcs& ecs,
                    Core3D::IGraphicsContext& graphicsContext);
    
    // Set texture pair
    void SetTexturePair(const TexturePair& pair) { texturePair_ = pair; }
    TexturePair& GetTexturePair() { return texturePair_; }
    
    // Set reference render targets
    void SetReferenceRenderTarget(RENDER_NS::RenderHandleReference gtTarget,
                                   RENDER_NS::RenderHandleReference optTarget);
    
    // Random camera generation
    void RandomizeCamera(float distanceMin = 2.0f, 
                          float distanceMax = 5.0f);
    
    // Update camera transform
    void UpdateCameraTransform(CORE_NS::Entity cameraEntity);
    
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
    RENDER_NS::RenderHandleReference GetGTRenderTarget() const { return gtRenderTarget_; }
    RENDER_NS::RenderHandleReference GetOptimizedRenderTarget() const { return optRenderTarget_; }
    RENDER_NS::RenderHandleReference GetUVRenderTarget() const { return uvRenderTarget_; }
    
    // Get low-res texture resources
    RENDER_NS::RenderHandleReference GetLRTexture() const { return texturePair_.lrTexture; }
    RENDER_NS::RenderHandleReference GetLRGradient() const { return texturePair_.lrGradient; }
    RENDER_NS::RenderHandleReference GetLRMomentum1() const { return texturePair_.lrMomentum1; }
    RENDER_NS::RenderHandleReference GetLRMomentum2() const { return texturePair_.lrMomentum2; }
    RENDER_NS::RenderHandleReference GetSampler() const { return sampler_; }
    
    // Set GT texture from external source
    void SetGTTexture(RENDER_NS::RenderHandleReference gtTexture) {
        texturePair_.gtTexture = gtTexture;
    }
    
    // Get iteration count
    uint32_t GetIteration() const { return texturePair_.iteration; }
    
    // Run self-test and write results to file
    void RunSelfTest(const char* outputPath = "sr_test_results.txt");
    
    // Phase 4 test: Create test texture (checkerboard pattern)
    void CreateTestTexture();
    
    // Phase 4 test: Run compute shader test
    void RunComputeShaderTest(const char* outputPath = "sr_compute_test_results.txt");
    
    // Set GT texture from render node graph output
    void SetGTTextureFromRenderOutput(RENDER_NS::RenderHandleReference gtTexture);
    
    // Initialize LR texture by downsampling GT
    void InitializeLRFromGT();
    
    // Check if initialized with GT texture
    bool HasGTTexture() const { return texturePair_.gtTexture && texturePair_.gtTexture.GetHandle().id != 0; }
    
    // Execute one training iteration (called after render)
    // This performs: downsample GT -> loss + backward -> adam update
    void ExecuteTrainingIteration(RENDER_NS::IRenderCommandList& cmdList);
    
    // Get camera parameters
    float GetCameraYaw() const { return cameraYaw_; }
    float GetCameraPitch() const { return cameraPitch_; }
    float GetCameraDistance() const { return cameraDistance_; }
    BASE_NS::Math::Vec3 GetCameraTarget() const { return cameraTarget_; }
    
    // Compute shader dispatch (for render node calls)
    void DispatchLossBackward(RENDER_NS::IRenderCommandList& cmdList);
    void DispatchAdamOptimizer(RENDER_NS::IRenderCommandList& cmdList);
    
private:
    void CreateRenderTargets();
    
    RENDER_NS::IRenderContext* renderContext_ = nullptr;
    RENDER_NS::IGpuResourceManager* gpuResMgr_ = nullptr;
    RENDER_NS::IShaderManager* shaderMgr_ = nullptr;
    CORE_NS::IEcs* ecs_ = nullptr;
    Core3D::IGraphicsContext* graphicsContext_ = nullptr;
    
    TexturePair texturePair_;
    RENDER_NS::RenderHandleReference gtRenderTarget_;
    RENDER_NS::RenderHandleReference optRenderTarget_;
    RENDER_NS::RenderHandleReference uvRenderTarget_;
    RENDER_NS::RenderHandleReference sampler_;
    
    AdamParams adamParams_;
    float currentLoss_ = 0.0f;
    bool isTraining_ = false;
    
    // Camera parameters
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.0f;
    float cameraDistance_ = 3.0f;
    BASE_NS::Math::Vec3 cameraTarget_ = BASE_NS::Math::Vec3(0.0f, 0.0f, 0.0f);
};

} // namespace LumeDemo

#endif // DIFF_TEXTURE_SR_H