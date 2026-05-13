# HuanGL — 项目上下文

## 项目简介

**HuanGL**（幻·GL）是一个 OpenGL 4.6 渲染器学习/展示项目，从原始 LearnOpenGL 单文件项目重构而来。目标：展示 C++ 工程架构能力 + 渐进实现四种 GI 算法，用于简历/技术展示。

原始代码归档于 git tag `archive/learnogl-v1`。

## 构建方法（Windows）

```powershell
# 依赖：vcpkg 安装 glfw3、assimp（已通过 Scoop 安装 vcpkg）
$tc = "D:\Scoop\apps\vcpkg\2026.03.18\scripts\buildsystems\vcpkg.cmake"
cmake -B build "-DCMAKE_TOOLCHAIN_FILE=$tc" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
# 产出：build\Debug\HuanGL.exe
```

**重要**：每次新增 `.cpp` 文件后必须重新运行 `cmake -B build ...`，因为使用了 `GLOB_RECURSE`，CMake 不会自动感知新文件。

## 关键技术决策

- **GLAD2**（非 GLAD1）：加载函数是 `gladLoadGL((GLADloadfunc)glfwGetProcAddress)`，不是旧的 `gladLoadGLLoader`
- **DSA API 全程**：用 `glCreateTextures`、`glCreateBuffers`、`glCreateFramebuffers`、`glNamedBuffer*`、`glBindTextureUnit` 等
- **无 RHI 层**：项目目的是深入学习 OpenGL，不做跨 API 抽象
- **HuanGL namespace**：所有代码在 `namespace HuanGL {}` 内
- **GLAPIENTRY**：GL debug callback 必须用文件局部函数（非类静态方法），MSVC 的 APIENTRY 作为类成员有问题
- MSVC 不接受 `{ e ? glEnable(...) : glDisable(...); }` 形式的单行函数体，要用 if-else

## 当前进度

### ✅ Phase 1: Foundation（已完成）

所有核心基础类已实现并通过构建：

| 文件 | 职责 |
|------|------|
| `src/core/Window.h/cpp` | GLFW 封装，OpenGL 4.6 context，resize callback |
| `src/core/Input.h/cpp` | 键盘/鼠标/滚轮输入，每帧 delta |
| `src/core/App.h/cpp` | 主循环，持有 Window，ESC 退出 |
| `src/renderer/Renderer.h/cpp` | GL 状态封装，GL_DEBUG_OUTPUT callback，PushDebugGroup |
| `src/renderer/Shader.h/cpp` | Vertex/Fragment/Geometry/Compute shader，DSA uniform |
| `src/renderer/Buffer.h/cpp` | VertexArray + Buffer (VBO/EBO/UBO/SSBO)，DSA |
| `src/renderer/Texture.h/cpp` | 2D/HDR/Cubemap/3D 纹理，DSA，BindImage |
| `src/renderer/Framebuffer.h/cpp` | FBO，MRT，深度 RBO，IsComplete |
| `src/renderer/UniformBuffer.h` | CameraUBO/LightsUBO/TimeUBO 模板封装（header-only） |
| `shader/common/uniforms.glsl` | 共享 UBO 定义（binding 0/1/2） |
| `src/main.cpp` | 13 行，仅调用 App::Run() |

### 🔜 Phase 2: Render Pipeline（下一步）

计划文档：`docs/superpowers/plans/`（尚未创建）

内容：
- GBufferPass（Deferred rendering 的 G-Buffer 填充）
- ShadowPass（Cascaded Shadow Maps + PCSS 软阴影）
- LightingPass（PBR + IBL）
- ResourceManager（引用计数纹理/Mesh 缓存）

### 🔜 Phase 3: Scene System
- SceneManager + ImGui
- SponzaScene（GI showcase）
- HelmetScene（PBR showcase）

### 🔜 Phase 4: Post-Processing
- Bloom → TAA → ACES Tone Mapping

### 🔜 Phase 5–8: GI 算法（渐进实现）
- RSM → SSGI → VXGI → DDGI

## 展示场景

| 场景 | 目的 |
|------|------|
| Sponza | GI showcase（彩色间接光弹射） |
| Damaged Helmet | PBR 材质 showcase |

## 设计文档位置

- 完整设计：`docs/superpowers/specs/2026-05-13-huangl-refactor-design.md`
- Phase 1 实现计划：`docs/superpowers/plans/2026-05-13-huangl-phase1-foundation.md`

## 目录结构

```
src/
  core/          # App, Window, Input
  renderer/      # Shader, Buffer, Texture, Framebuffer, Renderer, UniformBuffer
  pipeline/      # RenderPipeline, passes/, gi/  （Phase 2+）
  scene/         # Scene, SceneManager           （Phase 3+）
  resource/      # ResourceManager               （Phase 2+）
  ui/            # ImGuiLayer                    （Phase 3+）
shader/
  common/        # uniforms.glsl
  gbuffer/       # Phase 2
  shadow/        # Phase 2
  gi/rsm|ssgi|vxgi|ddgi/  # Phase 5-8
  lighting/      # Phase 2
  postprocess/   # Phase 4
```
