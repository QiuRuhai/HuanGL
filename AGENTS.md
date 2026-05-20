# HuanGL - Project Context

## Project Summary

**HuanGL** is an OpenGL 4.6 renderer learning and showcase project. It was refactored from an original LearnOpenGL single-file project, now archived at git tag `archive/learnogl-v1`.

The active goal is to demonstrate C++ rendering architecture and gradually implement global illumination techniques for a portfolio-quality technical project.

## Build Method on Windows

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Output:

```text
build\Debug\HuanGL.exe
```

LLVM/clang-cl verification build:

```powershell
cmake -S . -B build-clang -G Ninja `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_CXX_COMPILER=clang-cl `
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-clang
```

Output:

```text
build-clang\HuanGL.exe
```

The repository intentionally does not use `CMakePresets.json`; build commands stay explicit in docs.
On Windows, `CMakeLists.txt` automatically uses `$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` when `CMAKE_TOOLCHAIN_FILE` is not already set.

Important: after adding new `.cpp` files, rerun CMake configure because the project uses `GLOB_RECURSE`.

## Repository Baseline

The repository has been cleaned to a minimal HuanGL baseline:

- Old LearnOpenGL resources, screenshots, local GLFW libraries, and legacy shader programs are removed.
- Third-party vendored code lives under `external/`.
- GLFW and Assimp are resolved through vcpkg/CMake targets.
- `AGENTS.md` is the single repository agent context file.
- `CLAUDE.md` and `.claude/` are intentionally removed.

## Key Technical Decisions

- Use **GLAD2**, not GLAD1. Load OpenGL with `gladLoadGL((GLADloadfunc)glfwGetProcAddress)`.
- Use OpenGL Direct State Access APIs throughout, such as `glCreateTextures`, `glCreateBuffers`, `glCreateFramebuffers`, `glNamedBuffer*`, and `glBindTextureUnit`.
- Do not add an RHI layer. The point of the project is to learn and show OpenGL directly.
- Put project code in `namespace HuanGL`.
- GL debug callbacks must be file-local functions using `GLAPIENTRY`; avoid class static callback methods on MSVC.
- MSVC does not accept single-line ternary statements such as `{ e ? glEnable(...) : glDisable(...); }`; use `if`/`else`.
- Reconstruct world position from depth and inverse view-proj in the lighting pass; do not write world position to a GBuffer attachment.
- Use `sampler2DArrayShadow` for CSM and let the hardware perform the depth comparison; cascade selection uses view-space Z.
- Tone mapping lives in `PostProcessPass`, not in the lighting shader. `LightingPass` writes raw HDR radiance to an RGBA16F target so future passes (Bloom, TAA) can read it.
- Re-orthogonalize the TBN basis in the fragment shader, not the vertex shader. Interpolation destroys vertex-side orthogonality.
- Treat Assimp texture paths starting with `*` as indices into `aiScene::mTextures` and decode the embedded bytes via `Texture::Load2DFromMemory`. This is the only correct way to load textures from `.glb`.
- `Material::packedMetallicRoughness` flag signals the glTF convention (G = roughness, B = metallic) so the shader samples the right channels.
- `App` registers scenes with soft failure: if a model file is missing or fails to load, the app logs and skips, continuing with the remaining registered scenes.
- Debug UI edits `ApplicationState` and `World`; render passes read `FrameContext` and `RenderSceneView`. Do not let ImGui mutate pass internals directly.
- `packed`, `near`, and `far` are reserved or potentially reserved names in GLSL across drivers. Avoid them as GLSL identifiers.

## Current Progress

### Phase 1: Foundation Complete

| File | Responsibility |
|------|----------------|
| `src/core/Window.h/cpp` | GLFW wrapper, OpenGL 4.6 context, resize callback |
| `src/core/Input.h/cpp` | Keyboard, mouse, scroll input state |
| `src/core/App.h/cpp` | Main loop, owns Window, ESC exits |
| `src/renderer/Renderer.h/cpp` | GL state helpers, debug callback, debug groups |
| `src/renderer/Shader.h/cpp` | Vertex, fragment, geometry, compute shader wrapper with DSA uniforms |
| `src/renderer/Buffer.h/cpp` | VertexArray and Buffer wrappers for VBO, EBO, UBO, SSBO |
| `src/renderer/Texture.h/cpp` | 2D, HDR, cubemap, 3D texture wrapper and image binding |
| `src/renderer/Framebuffer.h/cpp` | FBO wrapper, MRT, depth renderbuffer, completeness checks |
| `src/renderer/UniformBuffer.h` | Header-only CameraUBO, LightsUBO, TimeUBO helpers |
| `shader/common/uniforms.glsl` | Shared GLSL UBO definitions |
| `src/main.cpp` | Minimal entry calling `App::Run()` |

### Phase 2: Render Pipeline — Complete

| File | Responsibility |
|------|----------------|
| `src/pipeline/RenderPipeline.h/cpp` | Pass orchestrator, owns Shadow/GBuffer/Lighting/PostProcess passes |
| `src/pipeline/passes/ShadowPass.h/cpp` | Four-cascade CSM with `sampler2DArrayShadow` |
| `src/pipeline/passes/GBufferPass.h/cpp` | Deferred MRT fill (RGBA8 albedo+metallic, RGBA16F normal+roughness, D24 depth) |
| `src/pipeline/passes/LightingPass.h/cpp` | Cook-Torrance PBR + IBL (irradiance + prefilter + BRDF LUT) |
| `src/resource/ResourceManager.h/cpp` | `weak_ptr` texture and mesh cache with GC |
| `src/resource/MeshLoader.h/cpp` | Assimp wrapper, returns `LoadResult { mesh, materials }` |
| `src/scene/Scene.h` | Scene interface with mesh/material/light access |
| `src/scene/TestScene.h/cpp` | Procedural test scene (floor + spheres + PBR factor materials) |
| `src/renderer/Schema.h` | `Mesh`, `SubMesh`, `Material`, `DirectionalLight`, `Vertex` schemas |
| `src/core/Camera.h/cpp` | Free-fly camera, WASD plus mouse look |
| `shader/gbuffer/*.{vert,frag}` | GBuffer fill |
| `shader/shadow/csm.vert` | Cascade depth render |
| `shader/lighting/*.{vert,frag}` | PBR+IBL, IBL precompute (equirect→cubemap, irradiance, prefilter, BRDF LUT), fullscreen helper |

### Phase 2.5: Pipeline Polish — Complete

| File | Responsibility |
|------|----------------|
| `src/pipeline/passes/PostProcessPass.h/cpp` | Tone mapping, gamma, debug visualization |
| `src/scene/ModelScene.h/cpp` | Loads a model via `MeshLoader`, adds optional floor |
| `shader/postprocess/postprocess.frag` | ACES / Reinhard / linear tone map plus seven debug modes |
| `src/renderer/Texture.h/cpp` | Adds `Load2DFromMemory` for `.glb` embedded textures |
| `src/resource/MeshLoader.h/cpp` | Extends to extract PBR textures and handle `*N` embedded paths |
| `src/core/Input.h/cpp` | Adds `IsKeyJustPressed` via GLFW key callback |
| `src/core/App.h/cpp` | Multi-scene registration with `N`-key cycling, debug-key handling, resize propagation |

### Phase 3: Architecture Reset and Debug UI — Minimum Complete

| File | Responsibility |
|------|----------------|
| `src/app/ApplicationState.h` | Runtime state container for scene registry, camera, render settings, debug settings, frame stats |
| `src/app/SceneRegistry.h/cpp` | Demo scene registration, active scene selection, soft-failure loading |
| `src/app/InputController.h/cpp` | Keyboard command mapping into `ApplicationState` |
| `src/scene/World.h/cpp` | Lightweight entity/world model used by renderer and DebugUI |
| `src/renderer/FrameContext.h` | Per-frame viewport, time, camera data, render settings, debug settings |
| `src/renderer/RenderSceneView.h` | Read-only renderable view consumed by passes |
| `src/pipeline/CascadeData.h` | Shared CSM cascade metadata |
| `src/pipeline/PipelineOutputs.h` | Explicit pass output structs for future passes |
| `src/ui/ImGuiLayer.h/cpp` | Dear ImGui context and GLFW/OpenGL3 backend wrapper |
| `src/ui/DebugUI.h/cpp` | HuanGL Debug panel that edits state/world data |

## Current Directory Structure

```text
src/
  app/
  core/
  renderer/
  pipeline/
  resource/
  scene/
  ui/

external/
  glad/
  glm/
  stb/

shader/
  common/

docs/
  superpowers/specs/
  superpowers/plans/
```

Empty future directories may be absent from Git until their implementation starts.

## Planned Phases

| Phase | Status | Theme |
|-------|--------|-------|
| 1 | ✅ Complete | Foundation (GLAD2, RAII wrappers, App loop) |
| 2 | ✅ Complete | Deferred render pipeline (GBuffer, CSM, PBR+IBL) |
| 2.5 | ✅ Complete | Pipeline polish (PostProcess, glTF materials, debug views) |
| 3 | ✅ Minimum Complete | Application state, lightweight World, ImGui debug UI |
| 4 | Planned | Bloom, TAA, improved tone mapping |
| 5 | Planned | RSM |
| 6 | Planned | SSGI |
| 7 | Planned | VXGI |
| 8 | Planned | DDGI |

For phase deliverables, dependencies, and ordering rationale see
[`docs/architecture.md`](docs/architecture.md).

## Design Documents

- Refactor design: `docs/superpowers/specs/2026-05-13-huangl-refactor-design.md`
- Repository cleanup design: `docs/superpowers/specs/2026-05-13-huangl-repository-cleanup-design.md`
- Phase 2 pipeline design: `docs/superpowers/specs/2026-05-14-huangl-phase2-pipeline-design.md`
- Phase 2.5 polish design: `docs/superpowers/specs/2026-05-16-huangl-phase2.5-polish-design.md`
- Docs overhaul design: `docs/superpowers/specs/2026-05-19-huangl-docs-overhaul-design.md`
- Architecture reset design: `docs/superpowers/specs/2026-05-20-huangl-architecture-reset-design.md`
- Phase 1 plan: `docs/superpowers/plans/2026-05-13-huangl-phase1-foundation.md`
- Repository cleanup plan: `docs/superpowers/plans/2026-05-13-huangl-repository-cleanup.md`
- Phase 2 pipeline plan: `docs/superpowers/plans/2026-05-14-huangl-phase2-pipeline.md`
- Phase 2.5 polish plan: `docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md`
- Docs overhaul plan: `docs/superpowers/plans/2026-05-19-huangl-docs-overhaul.md`
- Architecture reset plan: `docs/superpowers/plans/2026-05-20-huangl-architecture-reset.md`
- Architecture and roadmap: `docs/architecture.md`
