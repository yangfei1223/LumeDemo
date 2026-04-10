# 可微纹理超分辨率实施计划

## 项目目标
通过可微渲染优化低分辨率纹理，使其渲染效果接近高分辨率纹理。

## 核心架构
```
高分辨率纹理(GT) --下采样--> 低分辨率纹理(优化目标)
       |                              |
       v                              v
Ground Truth渲染              优化渲染
       |                              |
       +------------Loss计算-----------+
                     |
                     v
              反向传播梯度
                     |
                     v
              Adam优化器更新
                     |
                     v
              更新低分辨率纹理
```

## 实施阶段

### Phase 1: Shader开发 (已完成)
- [x] `texture_downsample.comp` - 纹理下采样
- [x] `sr_loss_backward.comp` - MSE Loss + 反向传播
- [ ] `sr_adam_optimizer.comp` - Adam优化器 (待创建)

### Phase 2: GBuffer修改 (待实施)
- 修改 `Lume3D/assets/3d/shaders/shader/core3d_dm_df.frag`
- 在GBuffer中添加UV输出（使用velocity_normal的BA通道）

### Phase 3: C++组件 (待实施)
1. 创建 `include/diff_texture_sr.h` - 可微纹理SR管理器
2. 创建 `src/diff_texture_sr.cpp` - 实现
3. 修改 `src/application.cpp` - 集成训练循环

### Phase 4: 渲染图配置 (待实施)
- 创建 `assets/app/renderNodeGraph_texture_sr.json`

### Phase 5: 构建测试 (待实施)
- 编译所有shader
- 构建项目
- 运行测试

## 关键设计决策

### 1. UV获取方案
在GBuffer的`velocity_normal`输出中打包UV坐标（使用BA通道）

### 2. 分辨率配置
- GT: 1024x1024 (原始纹理)
- LR: 512x512 (优化目标)
- 可调: 通过UI或配置文件

### 3. 训练数据生成
每帧随机化相机位置（球坐标）：
- Distance: 2.0 - 5.0
- Yaw: 0 - 2π
- Pitch: -π/3 - π/3

### 4. Loss函数
- 主Loss: L2 (MSE)
- 可选: L1, Huber (未来扩展)

### 5. 显示模式
左右分屏对比:
- 左侧: Ground Truth渲染
- 右侧: 优化结果渲染
- 下方: Loss曲线

## 文件清单

### Shader文件
```
LumeRender/assets/render/shaders/computeshader/
├── texture_downsample.comp      [已创建]
├── sr_loss_backward.comp        [已创建]
└── sr_adam_optimizer.comp       [待创建]
```

### C++文件
```
include/
└── diff_texture_sr.h            [待创建]

src/
├── diff_texture_sr.cpp          [待创建]
└── application.cpp              [待修改]
```

### 配置文件
```
assets/app/
└── renderNodeGraph_texture_sr.json  [待创建]
```

## 下一步行动

1. 创建Adam优化器Shader
2. 修改Lume3D GBuffer添加UV输出
3. 实现DiffTextureSRManager类
4. 在application.cpp中集成训练循环

## 注意事项

1. **子模块修改**: 修改Lume3D后需要提交到子模块仓库
2. **Shader编译**: 新增shader需要运行cmake重新编译
3. **内存占用**: 梯度图像使用RGBA32F，内存占用较大
4. **收敛性**: 建议先在小分辨率(256x256)上测试收敛性
