# HuanGL 重构设计文档

**日期**：2026-05-13  
**项目**：LearnOGL → HuanGL  
**目标**：工程能力展示 + 渐进式 GI 学习路径 + 视觉效果强大

---

## 1. 项目概述

将现有单文件 `main.cpp`（680 行）重构为模块化 OpenGL 4.6 渲染器，命名为 **HuanGL**（幻·GL）。

核心目标：
- 展示 C++ 工程架构能力（分层设计、接口抽象）
- 展示现代 OpenGL 4.x 特性（Compute Shader、DSA、UBO/SSBO）
- 渐进实现四种 GI 算法（RSM → SSGI → VXGI → DDGI）
- 最终视觉效果达到现代游戏引擎水准

---

## 2. 文件结构

```
HuanGL/
├── src/
│   ├── main.cpp                      # 入口，只负责 App 生命周期
│   ├── core/
│   │   ├── App.h/cpp                 # 主循环、生命周期管理
│   │   ├── Window.h/cpp              # GLFW 封装
│   │   └── Input.h/cpp               # 输入状态查询
│   ├── renderer/
│   │   ├── Renderer.h/cpp            # OpenGL 状态机封装、资源工厂
│   │   ├── Shader.h/cpp              # 升级支持 Compute Shader
│   │   ├── Framebuffer.h/cpp         # FBO/RBO 封装
│   │   ├── Texture.h/cpp             # 2D/Cubemap/3D 纹理统一封装
│   │   └── Buffer.h/cpp              # VAO/VBO/UBO/SSBO 封装
│   ├── pipeline/
│   │   ├── RenderPipeline.h/cpp      # 基类，管理 pass 执行顺序
│   │   ├── passes/
│   │   │   ├── GBufferPass.h/cpp     # 延迟渲染 G-Buffer 填充
│   │   │   ├── ShadowPass.h/cpp      # CSM + PCSS 软阴影
│   │   │   ├── LightingPass.h/cpp    # PBR + IBL 光照合成
│   │   │   └── PostProcessPass.h/cpp # Bloom → TAA → ACES → Gamma
│   │   └── gi/
│   │       ├── GIPass.h              # 纯虚基类
│   │       ├── RSMPass.h/cpp         # 阶段 1：Reflective Shadow Maps
│   │       ├── SSGIPass.h/cpp        # 阶段 2：Screen Space GI
│   │       ├── VXGIPass.h/cpp        # 阶段 3：Voxel Cone Tracing
│   │       └── DDGIPass.h/cpp        # 阶段 4：Dynamic Diffuse GI
│   ├── scene/
│   │   ├── Scene.h/cpp               # 抽象基类
│   │   ├── SceneManager.h/cpp        # ImGui 场景切换、生命周期
│   │   ├── SponzaScene.h/cpp         # GI showcase 主场景
│   │   └── HelmetScene.h/cpp         # PBR 材质 showcase
│   ├── resource/
│   │   └── ResourceManager.h/cpp     # 引用计数缓存，防重复加载
│   └── ui/
│       └── ImGuiLayer.h/cpp          # ImGui 初始化 + 调试面板
├── shader/
│   ├── gbuffer/
│   ├── shadow/                       # CSM depth pass + PCSS filter
│   ├── gi/
│   │   ├── rsm/
│   │   ├── ssgi/
│   │   ├── vxgi/                     # 含 voxelization compute shader
│   │   └── ddgi/                     # 含 probe update compute shader
│   ├── lighting/
│   └── postprocess/
│       ├── bloom/
│       ├── taa/
│       └── tonemapping/
├── resources/
│   ├── objects/
│   │   ├── Sponza/
│   │   └── DamagedHelmet/
│   └── texture/hdr/
├── docs/
│   └── superpowers/specs/
└── CMakeLists.txt                    # OpenGL 4.6，vcpkg 支持
```

---

## 3. 核心架构

### 3.1 App 生命周期

```
main()
  └── App::Run()
        ├── Init: Window → Renderer → ResourceManager → ImGuiLayer → SceneManager
        ├── Loop:
        │     ├── Input::Poll()
        │     ├── ActiveScene→OnUpdate(dt)
        │     ├── ActiveScene→OnRender()
        │     │     └── RenderPipeline::Execute()
        │     │           ├── ShadowPass::Render()
        │     │           ├── GBufferPass::Render()
        │     │           ├── GIPass::Render()         ← 当前选中算法
        │     │           ├── LightingPass::Render()
        │     │           └── PostProcessPass::Render()
        │     └── ImGuiLayer::Render()
        └── Shutdown
```

### 3.2 GIPass 接口

```cpp
class GIPass {
public:
    virtual void Init(const RenderContext& ctx) = 0;
    virtual void Render(const GBuffer& gbuffer, const Scene& scene) = 0;
    virtual void RenderUI() = 0;   // 每个算法暴露自己的调参面板
    virtual const char* Name() const = 0;
    virtual ~GIPass() = default;
};
```

### 3.3 UBO 统一数据块

所有 pass 共享，每帧只需更新三个 UBO：

```glsl
layout(std140, binding = 0) uniform CameraUBO {
    mat4 view, proj, viewProj;
    vec3 camPos;
};
layout(std140, binding = 1) uniform LightsUBO {
    LightData lights[MAX_LIGHTS];
    int count;
};
layout(std140, binding = 2) uniform TimeUBO {
    float time, deltaTime;
};
```

### 3.4 ResourceManager

```cpp
// 同路径只加载一次，场景切换自动释放引用为零的资源
auto tex  = ResourceManager::Load<Texture>("sponza/floor_albedo.png");
auto mesh = ResourceManager::Load<Mesh>("sponza/sponza.gltf");
```

---

## 4. 渲染管线

### 4.1 Sponza 场景管线（完整 GI 管线）

```
ShadowPass     → 生成 CSM depth maps（4 级联）
GBufferPass    → 填充 albedo / normal / roughness-metallic / depth
GIPass         → 选中算法计算间接光 irradiance
LightingPass   → PBR + IBL + 直接光 + GI 合成
PostProcess    → Bloom → TAA → ACES Tone Mapping → Gamma
```

### 4.2 Helmet 场景管线（纯 PBR 管线）

```
GBufferPass    → 填充材质数据
LightingPass   → PBR + IBL（Irradiance + Prefilter + BRDF LUT）
PostProcess    → Bloom → ACES → Gamma
```

### 4.3 阴影：CSM + PCSS

- 4 级联 Shadow Map，覆盖 Sponza 全场景深度范围
- PCSS（Percentage Closer Soft Shadows）：阴影随遮挡物距离自然变软
- 这是 Sponza 场景视觉质量的关键基础

### 4.4 后处理 stack

```
Bloom          → 提取高亮 → 高斯模糊 → 叠加
TAA            → 抖动采样 + 历史帧混合，消除锯齿，同时稳定 GI
ACES           → 电影级色调映射（Naughty Dog / Epic 标准）
Gamma          → sRGB 输出
```

---

## 5. GI 渐进实现路径

### 阶段 1 — RSM（Reflective Shadow Maps）
- **原理**：阴影贴图每个 texel 视为面光源，采样间接漫反射
- **OpenGL**：多 Render Target，标准 FBO，4.x DSA 编写
- **展示**：Sponza 彩色布帘产生彩色间接光，GI 概念验证
- **参考**：Dachsbacher & Stamminger 2005

### 阶段 2 — SSGI（Screen Space Global Illumination）
- **原理**：GBuffer 空间沿法线半球 raymarch，采样间接光
- **OpenGL**：屏幕空间 fullscreen pass，依赖 GBuffer
- **展示**：与 SSAO 对比，演示 screen space 方法局限性
- **ImGui**：ray step 数、采样半径调节

### 阶段 3 — VXGI（Voxel Cone Tracing）
- **原理**：Compute Shader 体素化场景 → 3D Texture 存储 radiance → 锥形采样间接光
- **OpenGL**：需要 4.3+ Compute Shader、Image Load/Store、3D Texture
- **展示**：Sponza 里彩色间接光弹射效果最直观，技术含量最高
- **ImGui**：体素网格可视化、锥角、mip 级别调节
- **参考**：Crassin et al. 2011

### 阶段 4 — DDGI（Dynamic Diffuse GI）
- **原理**：场景规则分布 Irradiance Probe，Compute Shader 动态更新球谐 + 深度信息
- **OpenGL**：Compute Shader、SSBO（probe 数据）、Texture Array
- **展示**：Probe 可视化，动态场景下 GI 实时更新，可引用学术论文
- **ImGui**：Probe 间距、可视化开关、混合权重
- **参考**：McGuire et al. 2019「Dynamic Diffuse Global Illumination」

---

## 6. ImGui 面板规划

```
[Scene]          Sponza ▼  /  Damaged Helmet ▼
─────────────────────────────────────────────
[GI Method]      VXGI ▼   (RSM / SSGI / VXGI / DDGI)   ← Sponza 专属
[GI Intensity]   ████░  0.8
[Show Probes]    □                                        ← DDGI 专属
[Voxel Debug]    □                                        ← VXGI 专属
─────────────────────────────────────────────
[Shadow]         CSM + PCSS
[Shadow Softness] ███░░  0.6
─────────────────────────────────────────────
[Post Process]
  Bloom          ████░  0.7
  TAA            ✓
  Tone Mapping   ACES ▼
─────────────────────────────────────────────
[Stats]          Frame: 12.3ms | GI: 4.1ms
```

---

## 7. OpenGL 版本升级

| 特性 | 用途 |
|------|------|
| OpenGL 4.3 Compute Shader | VXGI 体素化、DDGI probe 更新 |
| OpenGL 4.5 DSA | 更简洁的资源操作 API，消除大量 bind/unbind |
| OpenGL 4.6 SPIR-V | 可选，shader 预编译 |
| SSBO | DDGI probe 数据存储 |
| Image Load/Store | VXGI 3D 纹理写入 |

CMakeLists.txt 中 `glfwWindowHint` 升级至 4.6 Core Profile。

---

## 8. 不在本项目范围内

- RHI 抽象层（项目目的是深入学习 OpenGL，非跨 API）
- Path Tracing（另立项目实现）
- 异步资源加载（ResourceManager 同步缓存已足够）
- 物理引擎集成
