# HuanGL

**HuanGL** is an OpenGL 4.6 renderer learning and showcase project. It started as a LearnOpenGL-derived codebase, but the active repository now contains a clean HuanGL foundation layer and will add rendering systems incrementally.

The original LearnOpenGL state is preserved at git tag `archive/learnogl-v1`.

## Current Status

Phase 1 is complete:

- GLFW window and input wrappers
- OpenGL 4.6 context creation through GLAD2
- Renderer state helpers and GL debug output
- RAII wrappers for shaders, buffers, textures, framebuffers, and uniform buffers
- Shared GLSL UBO definitions in `shader/common/uniforms.glsl`
- Minimal `App` loop that opens a window and clears the screen

Phase 2 will introduce the render pipeline, beginning with deferred rendering infrastructure.

## Repository Layout

```text
src/
  core/          App, Window, Input
  renderer/      OpenGL resource and state wrappers
  pipeline/      Render pipeline code added by later phases
  resource/      Resource management code added by later phases
  scene/         Scene system code added by later phases
  ui/            Debug UI code added by later phases

external/
  glad/          Vendored GLAD2 loader
  glm/           Vendored GLM headers
  stb/           Vendored stb_image

shader/
  common/        Shared GLSL definitions

docs/
  superpowers/   Specs and implementation plans
```

Models, textures, phase-specific shaders, and showcase scenes are intentionally not carried from the old LearnOpenGL tree. They will be added when each renderer phase needs them.

## Build on Windows

Dependencies are provided through vcpkg. The current local toolchain path is:

```powershell
$tc = "D:\Scoop\apps\vcpkg\2026.03.18\scripts\buildsystems\vcpkg.cmake"
cmake -B build "-DCMAKE_TOOLCHAIN_FILE=$tc" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Output:

```text
build\Debug\HuanGL.exe
```

When new `.cpp` files are added, re-run the configure command before building because the project currently uses `GLOB_RECURSE`.

## Technical Direction

- OpenGL 4.6 Core Profile
- GLAD2 loader API: `gladLoadGL((GLADloadfunc)glfwGetProcAddress)`
- Direct State Access API for OpenGL objects
- No RHI abstraction layer
- C++17
- GLFW and Assimp through vcpkg

## Planned Rendering Phases

- Phase 2: Render Pipeline
- Phase 3: Scene System
- Phase 4: Post-Processing
- Phase 5-8: RSM, SSGI, VXGI, and DDGI
