#ifndef DIFF_TEXTURE_SR_H
#define DIFF_TEXTURE_SR_H

#include <base/containers/vector.h>
#include <base/math/vector.h>
#include <render/resource_handle.h>
#include <render/device/intf_gpu_resource_manager.h>
#include <render/intf_render_context.h>
#include <core/ecs/intf_ecs.h>

namespace LumeDemo {

// 纹理对：高分辨率Ground Truth + 低分辨率优化目标
struct TexturePair {
    // Ground Truth (高分辨率，只读)
    RENDER_NS::RenderHandle gtTexture;
    uint32_t gtWidth = 1024;
    uint32_t gtHeight = 1024;
    
    // 优化目标 (低分辨率，可微)
    RENDER_NS::RenderHandle lrTexture;      // 当前值 (RGBA8)
    RENDER_NS::RenderHandle lrGradient;     // 梯度 (RGBA32F)
    RENDER_NS::RenderHandle lrMomentum1;    // Adam一阶矩 (RGBA32F)
    RENDER_NS::RenderHandle lrMomentum2;    // Adam二阶矩 (RGBA32F)
    uint32_t lrWidth = 512;
    uint32_t lrHeight = 512;
    
    // 迭代计数
    uint32_t iteration = 0;
    
    void Initialize(RENDER_NS::IGpuResourceManager& gpuResMgr);
    void DownsampleFromGT(RENDER_NS::IRenderCommandList& cmdList, 
                          RENDER_NS::IShaderManager& shaderMgr);
};

// 可微超分辨率管理器
class DiffTextureSRManager {
public:
    void Initialize(RENDER_NS::IRenderContext& renderContext,
                    CORE_NS::IEcs& ecs);
    
    // 设置纹理对
    void SetTexturePair(const TexturePair& pair) { texturePair_ = pair; }
    TexturePair& GetTexturePair() { return texturePair_; }
    
    // 设置参考图像（渲染目标）
    void SetReferenceRenderTarget(RENDER_NS::RenderHandle gtTarget,
                                   RENDER_NS::RenderHandle optTarget);
    
    // 随机相机生成
    void RandomizeCamera(float distanceMin = 2.0f, 
                         float distanceMax = 5.0f);
    
    // 执行优化步骤
    void OptimizeStep();
    
    // 获取当前Loss值
    float GetCurrentLoss() const { return currentLoss_; }
    
    // 是否正在训练
    bool IsTraining() const { return isTraining_; }
    void SetTraining(bool training) { isTraining_ = training; }
    
    // Adam参数
    struct AdamParams {
        float learningRate = 0.01f;
        float beta1 = 0.9f;
        float beta2 = 0.999f;
        float epsilon = 1e-8f;
    };
    void SetAdamParams(const AdamParams& params) { adamParams_ = params; }
    const AdamParams& GetAdamParams() const { return adamParams_; }
    
    // 分辨率设置
    void SetResolution(uint32_t gtW, uint32_t gtH, 
                       uint32_t lrW, uint32_t lrH);
    
private:
    RENDER_NS::IRenderContext* renderContext_ = nullptr;
    RENDER_NS::IGpuResourceManager* gpuResMgr_ = nullptr;
    CORE_NS::IEcs* ecs_ = nullptr;
    
    TexturePair texturePair_;
    RENDER_NS::RenderHandle gtRenderTarget_;   // Ground Truth渲染结果
    RENDER_NS::RenderHandle optRenderTarget_;  // 优化渲染结果
    RENDER_NS::RenderHandle uvRenderTarget_;   // UV输出
    
    AdamParams adamParams_;
    float currentLoss_ = 0.0f;
    bool isTraining_ = false;
    
    // 相机参数
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.0f;
    float cameraDistance_ = 3.0f;
    Math::Vec3 cameraTarget_ = Math::Vec3(0.0f, 0.0f, 0.0f);
};

} // namespace LumeDemo

#endif // DIFF_TEXTURE_SR_H
