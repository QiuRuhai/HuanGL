# HuanGL

**HuanGL** is an OpenGL 4.6 renderer learning and showcase project. It started as a LearnOpenGL-derived codebase, but the active repository now contains a clean HuanGL foundation layer and will add rendering systems incrementally.

The original LearnOpenGL state is preserved at git tag `archive/learnogl-v1`.

## Current Status

Phases 1, 2, 2.5, and the minimum Phase 3 architecture reset are
complete. Phase 4 is in progress with Bloom implemented. The renderer
currently supports:

- Deferred PBR shading with metallic-roughness workflow (Cook-Torrance).
- Image-based lighting from a single HDR equirectangular environment (diffuse
  irradiance cubemap, prefiltered specular cubemap, BRDF LUT).
- Cascaded shadow maps (four cascades, `sampler2DArrayShadow`, 3x3 PCF).
- A post-processing pass with selectable tone mapping (ACES Filmic,
  Reinhard, linear) and sRGB-approximate gamma.
- Multi-mip HDR Bloom with soft-knee bright extraction, filtered
  downsampling, upsample combine, and pre-tone-map compositing.
- glTF, OBJ, and FBX loading through Assimp, including PBR factor extraction,
  normal-map sampling with TBN reconstructed in the fragment shader, and
  packed metallic-roughness textures.
- `.glb` embedded textures via Assimp `*N` indices.
- Multi-scene registration with runtime cycling.
- Eight runtime debug views: final composite, albedo, world-space normal,
  roughness, metallic, linear depth, shadow cascade overlay, and Bloom
  contribution.
- Window resize propagated through the entire pipeline.

For architecture, key design decisions, and the forward-looking roadmap (Phases 3 through 8), see [`docs/architecture.md`](docs/architecture.md).

## Repository Layout

```text
src/
  app/           Runtime state, scene registry, input command mapping
  core/          App lifecycle, Window, Input, Camera
  renderer/      OpenGL resource and state wrappers
  pipeline/      Render passes and concrete rendering techniques
  resource/      Resource management and mesh loading
  scene/         World, entities, and demo scene builders
  ui/            ImGui lifecycle and debug panels

external/
  glad/          Vendored GLAD2 loader
  glm/           Vendored GLM headers
  stb/           Vendored stb_image

shader/
  bloom/         Bloom extract/downsample/upsample shaders
  common/        Shared GLSL definitions

docs/
  superpowers/   Specs and implementation plans
```

Models, textures, phase-specific shaders, and showcase scenes are intentionally not carried from the old LearnOpenGL tree. They will be added when each renderer phase needs them.

## Build on Windows

Dependencies are provided through vcpkg. On Windows, `CMakeLists.txt` automatically uses `$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` when `CMAKE_TOOLCHAIN_FILE` is not already set. The expected local value is:

```powershell
$env:VCPKG_ROOT = "D:\Scoop\apps\vcpkg\current"
```

Default MSVC build:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Output:

```text
build\Debug\HuanGL.exe
```

LLVM/clang-cl build:

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

This repository does not use `CMakePresets.json`; build commands are kept explicit.

When new `.cpp` files are added, re-run the configure command before building because the project currently uses `GLOB_RECURSE`.

## Running

Models are not vendored in the repository. Place `.gltf`, `.glb`, or similar files under `resources/models/` (the directory is gitignored).
Two known-good test assets:

```powershell
# DamagedHelmet (small, fully embedded .glb)
curl -L -o resources/models/DamagedHelmet.glb `
  "https://github.com/KhronosGroup/glTF-Sample-Assets/raw/main/Models/DamagedHelmet/glTF-Binary/DamagedHelmet.glb"

# Sponza (larger, multi-texture scene; uses git sparse-checkout)
cd resources/models
git clone --depth 1 --filter=blob:none --sparse `
  https://github.com/KhronosGroup/glTF-Sample-Assets.git _assets
cd _assets
git sparse-checkout set Models/Sponza/glTF
cd ..
mv _assets/Models/Sponza Sponza
rm -r -force _assets
```

`App` registers any model it finds at startup and skips the rest with a log
message, so the binary runs even if neither model is present.

Runtime controls:

| Input | Action |
|-------|--------|
| `W` `A` `S` `D` | Camera translation |
| `Mouse` | Camera look |
| `N` | Cycle registered scenes |
| `T` | Cycle tone-map operator (ACES / Reinhard / linear) |
| `0` | Final composite |
| `1` | Albedo |
| `2` | World-space normal |
| `3` | Roughness |
| `4` | Metallic |
| `5` | Linear depth |
| `6` | Shadow cascade overlay |
| `7` | Bloom contribution |
| `Esc` | Quit |

## Technical Direction

- OpenGL 4.6 Core Profile
- GLAD2 loader API: `gladLoadGL((GLADloadfunc)glfwGetProcAddress)`
- Direct State Access API for OpenGL objects
- No RHI abstraction layer
- C++17
- GLFW and Assimp through vcpkg

## Planned Rendering Phases

| Phase | Status | Theme |
|-------|--------|-------|
| 1 | ✅ Complete | Foundation (GLAD2, RAII wrappers, App loop) |
| 2 | ✅ Complete | Deferred render pipeline (GBuffer, CSM, PBR+IBL) |
| 2.5 | ✅ Complete | Pipeline polish (PostProcess, glTF materials, debug views) |
| 3 | ✅ Minimum Complete | Application state, lightweight World, ImGui debug UI |
| 3.5 | ✅ Initial Complete | Concrete technique module boundary |
| 4 | In Progress | Bloom, TAA, improved tone mapping |
| 5 | Planned | RSM |
| 6 | Planned | SSGI |
| 7 | Planned | VXGI |
| 8 | Planned | DDGI |

For phase deliverables and ordering rationale see [`docs/architecture.md`](docs/architecture.md).
