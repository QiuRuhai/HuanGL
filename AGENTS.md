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

## Current Directory Structure

```text
src/
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

### Phase 2: Render Pipeline

- `GBufferPass` for deferred rendering
- `ShadowPass` for cascaded shadow maps and soft shadow filtering
- `LightingPass` for PBR and IBL
- `ResourceManager` for texture and mesh caching

### Phase 3: Scene System

- SceneManager and debug UI
- Sponza scene for GI showcase
- Damaged Helmet scene for PBR showcase

### Phase 4: Post-Processing

- Bloom
- TAA
- ACES tone mapping

### Phase 5-8: GI Algorithms

- RSM
- SSGI
- VXGI
- DDGI

## Design Documents

- Overall refactor design: `docs/superpowers/specs/2026-05-13-huangl-refactor-design.md`
- Phase 1 plan: `docs/superpowers/plans/2026-05-13-huangl-phase1-foundation.md`
- Repository cleanup design: `docs/superpowers/specs/2026-05-13-huangl-repository-cleanup-design.md`
- Repository cleanup plan: `docs/superpowers/plans/2026-05-13-huangl-repository-cleanup.md`
