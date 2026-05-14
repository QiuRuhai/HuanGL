# HuanGL Phase 2: Render Pipeline 设计文档

**日期**：2026-05-14  
**项目**：HuanGL  
**目标**：实现延迟渲染管线（GBuffer + CSM/PCSS Shadow + PBR/IBI Lighting）和资源管理器

---

## 1. 概述

在 Phase 1 基础模块之上，构建完整的物理渲染管线。四个子模块按依赖关系从底向上实现：

```
ResourceManager → GBufferPass → ShadowPass → LightingPass
         ↓              ↓            ↓              ↓
      资源缓存      填充GBuffer    CSM深度图     PBR + IBL合成
```

Phase 2 期间通过 `TestScene` 硬编码测试几何体验证管线。完整 Scene 系统在 Phase 3 实现。

---

## 2. 文件结构

```text
src/
├── pipeline/
│   ├── RenderPipeline.h/cpp          # Pass 列表管理 + 执行顺序
│   └── passes/
│       ├── GBufferPass.h/cpp         # 延迟渲染 MRT (2 targets + depth)
│       ├── ShadowPass.h/cpp          # CSM 4 级联 + depth array
│       └── LightingPass.h/cpp        # PBR Cook-Torrance + IBL
├── resource/
│   └── ResourceManager.h/cpp         # 引用计数缓存
├── scene/
│   ├── Scene.h                       # 最小接口（Mesh/Material/Light 数据容器）
│   └── TestScene.h/cpp               # 硬编码测试几何体
shader/
├── gbuffer/
│   ├── gbuffer.vert
│   └── gbuffer.frag
├── shadow/
│   ├── csm.vert                      # 深度写入
│   └── csm.frag                      # 空实现
└── lighting/
    ├── fullscreen.vert
    ├── pbr_ibl.frag                  # PBR + CSM/PCSS + IBL
    ├── irradiance.frag               # 生成 irradiance cubemap
    ├── prefilter.frag                # 生成 prefiltered cubemap
    └── brdf_lut.frag                 # 生成 BRDF LUT
```

---

## 3. ResourceManager

### 3.1 接口

```cpp
class ResourceManager {
public:
    static void Init();
    static void Shutdown();

    template<typename T>
    static std::shared_ptr<T> Load(const std::string& path);
    static void GC();  // 清理零引用 entry

private:
    static std::unordered_map<std::string, std::weak_ptr<void>> cache_;
};
```

### 3.2 设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 缓存 key | typeid(T) + path | 同一路径可被解释为不同资源类型 |
| 持有方式 | weak_ptr 缓存 + shared_ptr 返回 | 外部不用时自动释放 |
| 加载方式 | 同步 | 设计文档明确排除异步加载 |
| GC 时机 | Shutdown 时 + 场景切换时手动调用 | Phase 2 无场景切换，仅 Shutdown 调用 |

### 3.3 托管类型

| 类型 | 加载方式 |
|------|---------|
| Texture | `Texture::Load2D()` / `Texture::LoadHDR()` |
| Mesh | Assimp 导入 → VAO + VBO + EBO + SubMesh 列表 |

---

## 4. 数据模型（glTF 2.0 PBR Metallic-Roughness）

### 4.1 Mesh / SubMesh / Material

```cpp
struct SubMesh {
    uint32_t indexOffset;    // 在 EBO 中的偏移
    uint32_t indexCount;     // 该 SubMesh 的索引数量
    uint32_t materialIndex;  // 指向 Scene 的 Material 列表
};

struct Mesh {
    std::shared_ptr<VertexArray> vao;
    std::shared_ptr<Buffer>      vbo;
    std::shared_ptr<Buffer>      ebo;
    std::vector<SubMesh>         subMeshes;
};

struct Material {
    std::shared_ptr<Texture> albedoMap;
    std::shared_ptr<Texture> normalMap;
    std::shared_ptr<Texture> roughnessMap;
    std::shared_ptr<Texture> metallicMap;
    glm::vec4 baseColorFactor  = {1, 1, 1, 1};
    float     roughnessFactor  = 1.0f;
    float     metallicFactor   = 0.0f;
};
```

### 4.2 DirectionalLight

```cpp
struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 color;
    float     intensity = 1.0f;
};
```

### 4.3 顶点布局

所有 Mesh 使用统一顶点格式：

```
location = 0: vec3 position
location = 1: vec3 normal
location = 2: vec2 texcoord
location = 3: vec3 tangent (Phase 2 预留，暂不读取)
```

### 4.4 Model 矩阵

Phase 2 通过 per-draw uniform 设置 `model` 矩阵。TestScene 硬编码每个几何体的变换。
Phase 3 从 Scene Graph 获取。

---

## 5. Scene 接口

Phase 2 提供最小接口，Phase 3 扩展 ImGui 场景切换、多场景管理。

```cpp
class Scene {
public:
    virtual ~Scene() = default;
    virtual void Init(ResourceManager& rm) = 0;
    virtual void Update(float dt) = 0;

    const std::vector<Mesh*>&    GetMeshes()    const;
    const std::vector<Material>& GetMaterials() const;
    const DirectionalLight&      GetSunLight()  const;
    const glm::vec3&             GetAmbient()   const;

protected:
    std::vector<std::shared_ptr<Mesh>> meshes_;
    std::vector<Material>              materials_;
    DirectionalLight                   sunLight_;
    glm::vec3                          ambient_ = {0.03f, 0.03f, 0.05f};
};
```

Phase 2 用 `TestScene` 硬编码 plane + 几个 sphere/cube 验证管线。

### 各 Pass 使用 Scene 数据

| Pass | 使用的数据 |
|------|----------|
| GBufferPass | `GetMeshes()` + `GetMaterials()` → 绑贴图填 GBuffer |
| ShadowPass | `GetMeshes()` + `GetSunLight()` → 从光源视角渲染深度 |
| LightingPass | `GetSunLight()` + `GetAmbient()` → 直接光 + IBL 环境光 |

---

## 6. GBufferPass

### 6.1 GBuffer 布局（方案 C：2 MRT + Depth）

| Attachment | 通道 | 内部格式 | 内容 |
|-----------|------|---------|------|
| RT0 | RGBA | GL_RGBA8 | Albedo RGB (sRGB) + Metallic A (UNORM) |
| RT1 | RGBA | GL_RGBA16F | Normal XY (world-space) + Roughness Z |
| Depth | — | GL_DEPTH_COMPONENT24 | 硬件深度 |

Normal 存世界空间，LightingPass 直接对 IBL cubemap 采样，无需矩阵变换。
Depth 存为纹理（非 Renderbuffer），LightingPass 采样反推世界坐标。
不存 Position —— 从 Depth + 屏幕 UV 反推。

### 6.2 接口

```cpp
class GBufferPass {
public:
    void Init(int width, int height);
    void Resize(int width, int height);
    void Render(const Scene& scene, const CameraData& camera);

    std::shared_ptr<Texture> GetAlbedoMetallic() const;
    std::shared_ptr<Texture> GetNormalRoughness() const;
    std::shared_ptr<Texture> GetDepth() const;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<Shader>      shader_;
};
```

### 6.3 Shader 流程

```
Vertex Shader:
  输入: position(0), normal(1), texcoord(2)
  输出: world-space normal, texcoord
  变换: model × viewProj → gl_Position

Fragment Shader:
  输入: world-space normal, texcoord
  采样 Material 贴图 → pack 到两个 MRT:
    layout(location=0): vec4(albedo, metallic)
    layout(location=1): vec3(worldNormal, roughness)
```

---

## 7. ShadowPass

### 7.1 CSM 原理

4 个级联覆盖相机视锥的不同距离段。每个级联独立从光源视角渲染一张 depth map。
近处级联覆盖范围小 → 同样分辨率下精度更高 → 近处阴影更清晰。

### 7.2 参数

| 参数 | 值 |
|------|-----|
| 级联数 | 4 |
| 每级联分辨率 | 2048 × 2048 |
| 存储 | GL_TEXTURE_2D_ARRAY, GL_DEPTH_COMPONENT24 |

### 7.3 接口

```cpp
struct CascadeData {
    glm::mat4 viewProj;
    float     farPlane;
};

class ShadowPass {
public:
    void Init(int resolution = 2048);
    void Render(const Scene& scene, const CameraData& camera,
                const DirectionalLight& sunLight);

    std::shared_ptr<Texture> GetShadowMapArray() const;
    const std::array<CascadeData, 4>& GetCascades() const;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<Shader>      shader_;
    std::array<CascadeData, 4>   cascades_;
    int                          resolution_;
};
```

### 7.4 级联选择

LightingPass 根据像素的世界空间深度选择对应级联：

```
if   (depth < cascades[0].farPlane) → sample layer 0
elif (depth < cascades[1].farPlane) → sample layer 1
elif (depth < cascades[2].farPlane) → sample layer 2
else                               → sample layer 3
```

### 7.5 PCSS 软阴影

不在 ShadowPass 中计算 —— 在 LightingPass fragment shader 采样 shadow map 时计算 PCSS。
根据遮挡物到接收面的距离自适应调节 PCF 采样半径。

---

## 8. LightingPass

### 8.1 渲染流程

全屏三角形 fragment shader，每个像素：

```
1. 从 GBuffer 读取 albedo, metallic, roughness, normal, depth
2. 从 depth 反推世界坐标
3. 直接光: Cook-Torrance GGX BRDF × 太阳光 × PCSS 阴影
4. 间接光: IBL diffuse (irradiance cubemap) + IBL specular (prefilter + BRDF LUT)
5. 输出 HDR color (RGBA16F)
```

### 8.2 接口

```cpp
class LightingPass {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Render(const GBufferPass& gbuffer, const ShadowPass& shadow,
                const Scene& scene, const CameraData& camera);

private:
    std::unique_ptr<Shader> shader_;

    // IBL 纹理（初始化时从 HDR 环境图生成）
    std::shared_ptr<Texture> irradianceMap_;   // 32² cubemap RGBA16F
    std::shared_ptr<Texture> prefilterMap_;    // 128² cubemap with mips RGBA16F
    std::shared_ptr<Texture> brdfLUT_;         // 512² 2D RG16F

    // IBL 生成用的 shader
    std::unique_ptr<Shader> irradianceShader_;
    std::unique_ptr<Shader> prefilterShader_;
    std::unique_ptr<Shader> brdfLUTShader_;
};
```

### 8.3 IBL 生成

| 纹理 | 大小 | 格式 | 生成方式 |
|------|------|------|---------|
| Irradiance Map | 32² × 6 | RGBA16F | 对 HDR 环境图做漫反射半球余弦加权采样 |
| Prefilter Map | 128² × 6 × 5 mip | RGBA16F | 每 mip 对应一个 roughness 级别，镜面反射重要性采样 |
| BRDF LUT | 512² | RG16F | 运行时计算 GGX 积分，R=scale, G=bias |

IBL 使用 `resources/texture/hdr/brown_photostudio_02_2k.hdr`（CC0，已下载）。

### 8.4 Shader 纹理绑定

```
binding 0/1/2 → UBO (Camera, Lights, Time) — App 层统一更新
sampler0     → GBuffer Albedo+Metallic (RT0)
sampler1     → GBuffer Normal+Roughness (RT1)
sampler2     → GBuffer Depth
sampler3     → CSM Shadow Map Array
sampler4     → IBL Irradiance Cubemap
sampler5     → IBL Prefilter Cubemap
sampler6     → BRDF LUT
```

---

## 9. RenderPipeline

```cpp
class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void Execute(const Scene& scene, const CameraData& camera);

private:
    ShadowPass   shadowPass_;
    GBufferPass  gbufferPass_;
    LightingPass lightingPass_;
};
```

Execute 内部顺序：

```
ShadowPass::Render()       → 4 级联 depth array
GBufferPass::Render()      → 2 MRT + depth 纹理
// GIPass::Render()        ← Phase 5-8 插入点
LightingPass::Render()     → HDR color 到 backbuffer
```

---

## 10. 不在 Phase 2 范围内

- GIPass 及任何 GI 算法（Phase 5-8）
- PostProcess（Bloom / TAA / ACES — Phase 4）
- ImGui 调试面板（Phase 3）
- SceneManager / SponzaScene / HelmetScene（Phase 3）
- 多光源支持（当前仅方向光 + IBL）

---

## 11. 实现顺序

1. ResourceManager + Schema（Mesh / SubMesh / Material）
2. Scene 最小接口 + TestScene
3. GBufferPass + 对应 shader
4. ShadowPass + 对应 shader
5. LightingPass（IBL 生成 + PBR shader + CSM/PCSS 采样）
6. RenderPipeline 组装
