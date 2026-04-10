# 可微纹理超分辨率训练系统 - 开发进度归档

**归档日期**: 2025-04-10
**状态**: 核心功能实现完成，Critical Gaps 已修复

---

## 1. 项目概述

### 1.1 目标

实现一个基于可微渲染的纹理超分辨率训练系统：
- **训练目标**: 优化材质纹理 (base_color)，使其渲染结果逼近 GT 图像
- **训练方式**: Render Node Graph 内置训练循环，每帧执行 Forward → Loss → Backward → Adam
- **架构原则**: "RDG as the Loop" - 训练完全在渲染管线内完成

### 1.2 技术方案

```
Deferred Rendering Pipeline:
├── G-Buffer 生成 (depth, normal, material, uv, base_color)
├── PBR Shading → GT (Ground Truth)
├── LR Texture 优化 (待优化纹理)
└── 训练循环:
    ├── Forward: Simplified PBR(predicted = sample(LR, UV))
    ├── Loss: MSE(predicted, GT)
    ├── Backward: d_loss/d_baseColor (手动推导)
    └── Adam Optimizer: 更新 LR texture
```

---

## 2. 架构设计

### 2.1 Render Node Graph

```
renderNodeGraph_sr_simplified.json

┌─────────────────────────────────────────────────────────────────────┐
│  Node: CORE3D_RN_CAM_CTRL                                           │
│  功能: 创建 G-Buffer                                                 │
│  输出: depth, color (GT), velocity_normal, base_color, material, uv │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Node: CORE3D_RN_CAM_DM_SO_DF                                       │
│  功能: 写入 G-Buffer (Subpass 0)                                     │
│  renderSlot: CORE3D_RS_DM_DF_OPAQUE_UV                              │
│  输出: depth, color, velocity_normal, base_color, material, uv      │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Node: CORE3D_RN_CAM_DM_DS_DF                                       │
│  功能: PBR Deferred Shading (Subpass 1)                              │
│  输入: depth, velocity_normal, base_color, material (input attachments)│
│  输出: color (GT - 最终渲染结果)                                      │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Node: SR_RESOURCES                                                 │
│  功能: 创建训练资源                                                   │
│  输出:                                                               │
│    - lr_texture (r8g8b8a8_srgb, 512x512) - 待优化纹理               │
│    - lr_gradient (rgba32f, 512x512) - 梯度缓冲                      │
│    - lr_momentum1 (rgba32f, 512x512) - Adam 一阶矩                  │
│    - lr_momentum2 (rgba32f, 512x512) - Adam 二阶矩                  │
│    - loss_output (r32f, 1024x1024) - Loss 输出                      │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Node: SR_TRAINING (RenderNodeSRTraining)                           │
│  功能: 训练循环                                                      │
│                                                                      │
│  Pass 0: Downsample Init (仅第一帧)                                  │
│    - Source: G-Buffer base_color (1024x1024)                        │
│    - Dest: lr_texture (512x512)                                     │
│    - 目的: 初始化 LR 纹理为下采样的 GT                               │
│                                                                      │
│  Pass 1: Clear Gradient                                             │
│    - 内联到 Adam optimizer 末尾                                     │
│                                                                      │
│  Pass 2: Differentiable Render (Mega Kernel)                        │
│    - Forward: Simplified PBR(predicted)                             │
│    - Loss: MSE(predicted, GT)                                       │
│    - Backward: d_loss/d_baseColor (手动推导)                        │
│    - 输出: lr_gradient, loss_output                                 │
│                                                                      │
│  Pass 3: Adam Optimizer                                             │
│    - 更新: lr_texture -= lr * m1_hat / (sqrt(m2_hat) + epsilon)     │
│    - 清零: gradient buffer                                          │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Node: LR_DISPLAY                                                   │
│  功能: 显示优化结果                                                   │
│  Shader: fullscreen_copy.shader                                     │
│  输入: lr_texture                                                   │
│  输出: CORE_DEFAULT_BACKBUFFER                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 RenderNodeSRTraining Pass 结构

```cpp
void ExecuteFrame(IRenderCommandList& cmdList) {
    // Pass 0: 初始化 LR 纹理 (仅第一帧)
    if (!config_.initialized) {
        DispatchDownsampleInit(cmdList);  // GT base_color → LR
        config_.initialized = true;
    }
    
    cmdList.AddCustomBarrierPoint();
    
    // Pass 1: 清除梯度 (内联到 Adam)
    // DispatchClearGradient(cmdList);
    
    // Pass 2: 可微渲染 (Forward + Loss + Backward)
    DispatchDifferentiableRender(cmdList);
    
    cmdList.AddCustomBarrierPoint();
    
    // Pass 3: Adam 优化器
    DispatchAdamOptimizer(cmdList);
}
```

---

## 3. 已完成实现

### 3.1 RenderNode 实现

**文件**: `LumeRender/src/postprocesses/render_node_sr_training.h/cpp`

**功能**:
- 解析 JSON 资源配置
- 创建 PSO (Pipeline State Object)
- 执行 4-pass 训练循环
- 支持配置参数 (learning rate, betas, 分辨率等)

**资源绑定**:

| Binding | 资源 | 类型 | 说明 |
|---------|------|------|------|
| 0 | depthBuffer | texture2D | G-Buffer 深度 |
| 1 | normalBuffer | texture2D | G-Buffer 法线 |
| 2 | materialBuffer | texture2D | 材质参数 |
| 3 | uvBuffer | texture2D | UV 坐标 |
| 4 | baseColorBuffer | texture2D | G-Buffer 基础色 (初始化源) |
| 5 | lrTexture | texture2D | 待优化纹理 |
| 6 | sampler | sampler | 采样器 |
| 7 | lrGradient | image2D (storage) | 梯度缓冲 |
| 8 | lossOutput | image2D (storage) | Loss 输出 |
| 9 | gtImage | texture2D | GT 图像 |
| 10 | lrMomentum1 | image2D (storage) | Adam 一阶矩 |
| 11 | lrMomentum2 | image2D (storage) | Adam 二阶矩 |

**Push Constants**:
```cpp
struct PushConstantData {
    // 分辨率
    uint32_t gtWidth, gtHeight;
    uint32_t lrWidth, lrHeight;
    uint32_t useUVBuffer;
    
    // 光照参数
    vec3 lightDir;
    vec3 lightColor;
    
    // 相机参数
    vec3 cameraPos;
    mat4 viewProjInv;  // 世界坐标计算
};
```

### 3.2 Shader 实现

#### 3.2.1 texture_downsample.comp

**功能**: 纹理下采样 (初始化 LR 纹理)

```glsl
// 输入: uSourceTex (GT base_color, 1024x1024)
// 输出: uDestImage (LR texture, 512x512)

vec4 color = textureLod(uSourceTex, uv, 0.0);
imageStore(uDestImage, destCoord, color);
```

#### 3.2.2 sr_differentiable_render.comp

**功能**: 可微渲染 Mega Kernel

```glsl
// Forward: Simplified PBR
vec3 pbrForwardSimplified(vec3 baseColor, vec3 N, float metallic, 
                          float roughness, vec3 V, vec3 L) {
    // BRDF 参数
    vec3 f0 = mix(vec3(0.04), baseColor, metallic);
    vec3 diffuseColor = baseColor * (1.0 - metallic);
    
    // Diffuse (Lambert)
    vec3 diffuse = diffuseColor / PI;
    
    // Specular (GGX)
    float D = ...;  // Normal Distribution
    float G = ...;  // Geometry
    vec3 F = ...;   // Fresnel
    
    return (diffuse + specular) * lightColor * NoL;
}

// Backward: 手动推导梯度
vec3 pbrBackwardSimplified(vec3 dL_dColor, ...) {
    // d_diffuse/d_baseColor = (1 - metallic) / PI
    vec3 d_diffuse = vec3((1.0 - metallic) / PI);
    
    // d_specular/d_baseColor
    // F = f0 + (1 - f0) * (1 - VoH)^5
    // d_F/d_baseColor = metallic * (1 - (1 - VoH)^5)
    float d_F_factor = metallic * (1.0 - pow(1.0 - VoH, 5.0));
    vec3 d_specular = vec3(D * G * d_F_factor * 0.25);
    
    // Chain rule
    return dL_dColor * (d_diffuse + d_specular) * lightColor * NoL;
}

// World Position (正确实现)
vec3 getWorldPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = viewProjInv * ndc;
    return world.xyz / world.w;
}
```

#### 3.2.3 sr_adam_optimizer.comp

**功能**: Adam 优化器 + 梯度清零

```glsl
// Adam 更新
m1 = beta1 * m1 + (1.0 - beta1) * grad;
m2 = beta2 * m2 + (1.0 - beta2) * grad * grad;

// Bias correction
vec4 m1_hat = m1 / (1.0 - pow(beta1, iteration));
vec4 m2_hat = m2 / (1.0 - pow(beta2, iteration));

// Update
value -= lr * m1_hat / (sqrt(m2_hat) + epsilon);
value = clamp(value, 0.0, 1.0);

// Clear gradient for next iteration
imageStore(uGradient, coord, vec4(0.0));
```

### 3.3 JSON 配置

**文件**: `LumeDemo/assets/app/renderNodeGraph_sr_simplified.json`

**节点列表**:
1. CORE3D_RN_CAM_CTRL - G-Buffer 创建
2. CORE3D_RN_CAM_DM_SO_DF - G-Buffer 写入 (UV 输出)
3. CORE3D_RN_CAM_DM_DS_DF - PBR Shading
4. SR_RESOURCES - 训练资源创建
5. SR_TRAINING - 训练循环
6. LR_DISPLAY - 结果显示

---

## 4. 文件清单

### 4.1 新增文件

```
LumeRender/src/postprocesses/
├── render_node_sr_training.h         # RenderNode 头文件
└── render_node_sr_training.cpp       # RenderNode 实现

LumeRender/assets/render/shaders/computeshader/
├── texture_downsample.comp           # 下采样 shader
├── texture_downsample.shader         # Shader JSON
├── sr_differentiable_render.comp     # 可微渲染 Mega Kernel
├── sr_differentiable_render.shader   # Shader JSON
├── sr_adam_optimizer.comp            # Adam 优化器
└── sr_adam_optimizer.shader          # Shader JSON

LumeDemo/assets/app/
└── renderNodeGraph_sr_simplified.json # Render Node Graph 配置
```

### 4.2 修改文件

```
LumeRender/src/node/
└── core_render_node_factory.cpp      # 注册 RenderNodeSRTraining

LumeDemo/src/
└── application.cpp                   # 移除 DiffTextureSRManager，使用 RNG
```

### 4.3 依赖文件 (已存在)

```
Lume3D/assets/3d/shaders/shader/
└── core3d_dm_df_uv.frag              # UV 输出 shader (Subpass 0)

Lume3D/src/render/node/
└── render_node_default_material_deferred_shading.cpp  # Deferred shading 参考
```

---

## 5. 已解决的问题

### 5.1 Critical Gaps (已修复)

| 问题 | 解决方案 | 提交 |
|------|----------|------|
| LR 纹理无初始值 | 添加 Pass 0: Downsample Init (第一帧执行) | ✅ |
| 梯度缓冲未清除 | Adam optimizer 末尾清零 | ✅ |
| World Position 错误 | 正确实现 getWorldPos()，添加 viewProjInv 矩阵 | ✅ |
| 缺少相机矩阵 | 添加 viewProjInv 到 push constant | ✅ |

### 5.2 API 问题 (已修复)

| 问题 | 解决方案 |
|------|----------|
| InputResources 误用为数组 | 改用 `.images`, `.samplers` 字段访问 |
| DispatchLossBackward 未声明 | 头文件添加声明 |
| Binder API 调用 | `BindImage(0, handle)` 不使用初始化列表 |

---

## 6. 已知限制和后续工作

### 6.1 已知限制

| 限制 | 说明 | 影响 |
|------|------|------|
| 单光源假设 | 只考虑一个方向光 | 多光源场景效果差 |
| 无间接光照 | 缺少环境光/IBL | 暗部效果不准确 |
| 梯度累加 race condition | 多线程写入同一 texel | 梯度可能不准确 |
| Normal/Material 格式假设 | 假设特定编码格式 | 需验证实际格式 |
| 光照/相机硬编码 | 未从 RenderDataStore 获取 | 需手动配置 |

### 6.2 后续工作

#### 优先级 Medium

- [ ] 从 RenderDataStore 获取光照数据
- [ ] 从 RenderDataStore 获取相机数据 (viewProjInv)
- [ ] 验证 Normal 解码格式
- [ ] 验证 Material 编码格式

#### 优先级 Low

- [ ] 多光源支持
- [ ] IBL 间接光照
- [ ] 梯度原子累加 (GL_ARB_shader_atomic_float)
- [ ] 多材质纹理优化 (normal, roughness, metallic)

### 6.3 潜在优化

- 使用 atomic add 累加梯度 (避免 race condition)
- 分离 Clear Gradient Pass (更清晰的管线)
- 训练控制 UI (按键切换训练状态)
- Loss 可视化 (调试用)

---

## 7. 编译和运行

### 7.1 编译命令

```powershell
# 配置
cmake -S . -B build -G "Visual Studio 17 2022"

# 编译
cmake --build build --config Release
```

### 7.2 运行

```powershell
Push-Location build\Release
.\LumeDemo.exe
Pop-Location
```

### 7.3 验证清单

- [ ] G-Buffer 正确生成 (depth, normal, material, uv)
- [ ] UV 输出 shader 正确执行
- [ ] Deferred shading 输出 GT 图像
- [ ] LR 纹理正确初始化 (下采样 GT)
- [ ] 训练 Pass 正确执行
- [ ] Adam 优化器更新 LR 纹理
- [ ] 显示优化结果

---

## 8. 技术要点

### 8.1 可微采样 Backward 推导

**双线性插值 Forward**:
```
color = w00 * LR[tx0,ty0] + w10 * LR[tx1,ty0] 
      + w01 * LR[tx0,ty1] + w11 * LR[tx1,ty1]
```

**Backward**:
```
dL/d_LR[tx,ty] = w(tx,ty) * dL/d_color
```

### 8.2 Simplified PBR Backward 推导

**Forward**:
```
diffuse = baseColor * (1 - metallic) / π
specular = D * G * F * 0.25
F = f0 + (1 - f0) * (1 - VoH)^5
f0 = mix(0.04, baseColor, metallic)
```

**Backward**:
```
d_diffuse/d_baseColor = (1 - metallic) / π
d_F/d_baseColor = metallic * (1 - (1 - VoH)^5)
d_specular/d_baseColor = D * G * 0.25 * d_F/d_baseColor
d_color/d_baseColor = (d_diffuse + d_specular) * lightColor * NoL
```

### 8.3 World Position 计算

```glsl
vec3 getWorldPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = viewProjInv * ndc;
    return world.xyz / world.w;
}
```

---

## 9. 参考资料

### 9.1 内部参考

- `Lume3D/assets/3d/shaders/shader/core3d_dm_fullscreen_deferred_shading.frag` - PBR 参考
- `Lume3D/api/3d/shaders/common/3d_dm_lighting_common.h` - 光照计算
- `LumeRender/src/node/render_node_compute_generic.cpp` - API 参考

### 9.2 外部参考

- [glTF PBR Specification](https://github.com/KhronosGroup/glTF/tree/master/specification/2.0)
- [GGX BRDF](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
- [Adam Optimizer](https://arxiv.org/abs/1412.6980)

---

## 10. 变更历史

| 日期 | 版本 | 变更内容 |
|------|------|----------|
| 2025-04-10 | v1.0 | 初始归档 - 核心功能实现完成 |

---

**归档人**: Claude (Sisyphus Agent)
**审核**: 待审核