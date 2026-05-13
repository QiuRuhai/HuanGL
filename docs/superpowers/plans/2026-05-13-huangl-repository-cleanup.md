# HuanGL Repository Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the active repository into a minimal, buildable HuanGL baseline with third-party code under `external/` and legacy LearnOpenGL material removed.

**Architecture:** Keep HuanGL-owned code under `src/`, keep the shared UBO shader file under `shader/common/`, and move only the vendored third-party pieces used by Phase 1 into `external/`. GLFW and Assimp remain vcpkg-managed CMake dependencies, while GLAD, GLM, and stb are vendored in a narrow layout.

**Tech Stack:** C++17, OpenGL 4.6, GLAD2, GLFW via vcpkg, Assimp via vcpkg, GLM header-only, stb_image, CMake, PowerShell on Windows.

---

## File Structure

**Created or moved into:**

- `external/glad/include/glad/glad.h` — GLAD public OpenGL loader header.
- `external/glad/include/KHR/khrplatform.h` — KHR platform header required by GLAD.
- `external/glad/src/glad.c` — GLAD implementation compiled explicitly by CMake.
- `external/glm/glm/` — GLM public header tree used by project source.
- `external/glm/copying.txt` — GLM license text.
- `external/glm/readme.md` — GLM upstream readme.
- `external/stb/stb_image.h` — stb_image public header.
- `external/stb/stb_image.cpp` — stb_image implementation compiled explicitly by CMake.

**Modified:**

- `CMakeLists.txt` — use `external/` include roots and explicitly compile GLAD/stb implementations.
- `.gitignore` — clean UTF-8 ignore rules for build and IDE output.
- `README.md` — describe HuanGL instead of LearnOGL.
- `AGENTS.md` — become the single tracked repository agent context file.

**Deleted:**

- `include/` — old mixed third-party include root after required files are moved.
- `lib/` — local GLFW static libraries.
- `demo/` — old screenshots and GIF.
- `resources/` — old LearnOpenGL assets.
- legacy shader directories under `shader/`, while preserving `shader/common/uniforms.glsl`.
- `CLAUDE.md` — duplicate agent context.
- `.claude/`, `.vs/`, `.idea/`, `.vscode/`, `build/` — local or generated directories if present.

## Task 1: Move Vendored Dependencies and Fix CMake

**Files:**
- Move: `include/glad/glad.h` -> `external/glad/include/glad/glad.h`
- Move: `include/KHR/khrplatform.h` -> `external/glad/include/KHR/khrplatform.h`
- Move: `src/glad.c` -> `external/glad/src/glad.c`
- Move: `include/glm/glm/` -> `external/glm/glm/`
- Move: `include/glm/copying.txt` -> `external/glm/copying.txt`
- Move: `include/glm/readme.md` -> `external/glm/readme.md`
- Move: `include/stb_image.h` -> `external/stb/stb_image.h`
- Move: `src/stb_image.cpp` -> `external/stb/stb_image.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Verify the source files that will be moved exist**

  Run:

  ```powershell
  $paths = @(
    "include/glad/glad.h",
    "include/KHR/khrplatform.h",
    "src/glad.c",
    "include/glm/glm",
    "include/glm/copying.txt",
    "include/glm/readme.md",
    "include/stb_image.h",
    "src/stb_image.cpp"
  )
  foreach ($p in $paths) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing required path before move: $p" }
  }
  "All dependency source paths exist."
  ```

  Expected: prints `All dependency source paths exist.`

- [ ] **Step 2: Create the `external/` dependency directories**

  Run:

  ```powershell
  $dirs = @(
    "external/glad/include/glad",
    "external/glad/include/KHR",
    "external/glad/src",
    "external/glm",
    "external/stb"
  )
  foreach ($d in $dirs) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
  }
  Get-ChildItem -Directory external | Select-Object Name
  ```

  Expected: output includes `glad`, `glm`, and `stb`.

- [ ] **Step 3: Move GLAD into `external/glad/`**

  Run:

  ```powershell
  Move-Item -LiteralPath "include/glad/glad.h" -Destination "external/glad/include/glad/glad.h"
  Move-Item -LiteralPath "include/KHR/khrplatform.h" -Destination "external/glad/include/KHR/khrplatform.h"
  Move-Item -LiteralPath "src/glad.c" -Destination "external/glad/src/glad.c"
  Test-Path "external/glad/include/glad/glad.h"
  Test-Path "external/glad/include/KHR/khrplatform.h"
  Test-Path "external/glad/src/glad.c"
  ```

  Expected: three `True` lines.

- [ ] **Step 4: Move the minimal GLM tree into `external/glm/`**

  Run:

  ```powershell
  Move-Item -LiteralPath "include/glm/glm" -Destination "external/glm/glm"
  Move-Item -LiteralPath "include/glm/copying.txt" -Destination "external/glm/copying.txt"
  Move-Item -LiteralPath "include/glm/readme.md" -Destination "external/glm/readme.md"
  Test-Path "external/glm/glm/glm.hpp"
  Test-Path "external/glm/glm/gtc/type_ptr.hpp"
  Test-Path "external/glm/copying.txt"
  Test-Path "external/glm/readme.md"
  ```

  Expected: four `True` lines.

- [ ] **Step 5: Move stb into `external/stb/`**

  Run:

  ```powershell
  Move-Item -LiteralPath "include/stb_image.h" -Destination "external/stb/stb_image.h"
  Move-Item -LiteralPath "src/stb_image.cpp" -Destination "external/stb/stb_image.cpp"
  Test-Path "external/stb/stb_image.h"
  Test-Path "external/stb/stb_image.cpp"
  ```

  Expected: two `True` lines.

- [ ] **Step 6: Replace `CMakeLists.txt` with the external-layout build config**

  Set `CMakeLists.txt` to this exact content:

  ```cmake
  cmake_minimum_required(VERSION 3.20)

  project(HuanGL LANGUAGES C CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  if(APPLE)
      set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
      add_compile_definitions(GL_SILENCE_DEPRECATION=1)
  endif()

  file(GLOB_RECURSE SRC
      src/*.cpp
      src/*.c
  )

  add_executable(${PROJECT_NAME} ${SRC})

  target_sources(${PROJECT_NAME} PRIVATE
      external/glad/src/glad.c
      external/stb/stb_image.cpp
  )

  target_include_directories(${PROJECT_NAME} PRIVATE
      ${CMAKE_SOURCE_DIR}/external/glad/include
      ${CMAKE_SOURCE_DIR}/external/glm
      ${CMAKE_SOURCE_DIR}/external/stb
  )

  find_package(OpenGL REQUIRED)
  if(TARGET OpenGL::GL)
      target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
  else()
      if(APPLE)
          find_library(OPENGL_FRAMEWORK OpenGL)
          target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_FRAMEWORK})
      endif()
  endif()

  find_package(glfw3 3.3 REQUIRED CONFIG)
  if(TARGET glfw)
      target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
  elseif(TARGET glfw::glfw)
      target_link_libraries(${PROJECT_NAME} PRIVATE glfw::glfw)
  else()
      message(FATAL_ERROR "Found glfw3 config but no target named glfw or glfw::glfw")
  endif()

  find_package(assimp REQUIRED CONFIG)
  if(TARGET assimp::assimp)
      target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
  else()
      target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
  endif()

  if(APPLE)
      set_target_properties(${PROJECT_NAME} PROPERTIES
          BUILD_RPATH "/opt/homebrew/lib"
          INSTALL_RPATH "/opt/homebrew/lib"
      )
  endif()

  if(MSVC)
      target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
  else()
      target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SRC})
  ```

- [ ] **Step 7: Reconfigure after moving third-party implementation files**

  Run:

  ```powershell
  $tc = "D:\Scoop\apps\vcpkg\2026.03.18\scripts\buildsystems\vcpkg.cmake"
  cmake -B build "-DCMAKE_TOOLCHAIN_FILE=$tc" -DCMAKE_BUILD_TYPE=Debug
  ```

  Expected: CMake configure completes and writes build files without errors.

- [ ] **Step 8: Build after the dependency move**

  Run:

  ```powershell
  cmake --build build --config Debug
  ```

  Expected: build completes and produces `build\Debug\HuanGL.exe`.

- [ ] **Step 9: Commit the dependency-layout change**

  Run:

  ```powershell
  git status --short
  git add -A CMakeLists.txt external include src
  git commit -m "build: move vendored dependencies to external"
  ```

  Expected: commit succeeds. The status before commit should show the moved third-party files and `CMakeLists.txt`.

## Task 2: Remove Legacy LearnOpenGL Assets and Local Output

**Files:**
- Delete: `include/`
- Delete: `lib/`
- Delete: `demo/`
- Delete: `resources/`
- Delete: legacy shader directories under `shader/`
- Delete: `CLAUDE.md`
- Delete: `.claude/`
- Delete locally if present: `.vs/`, `.idea/`, `.vscode/`, `build/`
- Modify: `.gitignore`

- [ ] **Step 1: Preserve `shader/common/uniforms.glsl` while clearing legacy shaders**

  Run:

  ```powershell
  if (-not (Test-Path -LiteralPath "shader/common/uniforms.glsl")) {
      throw "Missing shader/common/uniforms.glsl before shader cleanup"
  }
  $shaderDirs = Get-ChildItem -LiteralPath "shader" -Directory | Where-Object { $_.Name -ne "common" }
  $shaderDirs | Select-Object FullName
  ```

  Expected: output lists legacy shader directories such as `shader\pbr`, `shader\deferred`, and `shader\bloom`.

- [ ] **Step 2: Remove all legacy shader directories except `shader/common/`**

  Run:

  ```powershell
  $workspace = (Resolve-Path ".").Path
  $shaderRoot = (Resolve-Path "shader").Path
  $targets = Get-ChildItem -LiteralPath $shaderRoot -Directory | Where-Object { $_.Name -ne "common" }
  foreach ($target in $targets) {
      $resolved = (Resolve-Path -LiteralPath $target.FullName).Path
      if (-not $resolved.StartsWith($shaderRoot)) {
          throw "Refusing to remove outside shader root: $resolved"
      }
      Remove-Item -LiteralPath $resolved -Recurse -Force
  }
  Test-Path "shader/common/uniforms.glsl"
  Get-ChildItem -LiteralPath "shader" -Directory | Select-Object Name
  ```

  Expected: `shader/common/uniforms.glsl` still exists, and the only directory under `shader` is `common`.

- [ ] **Step 3: Remove tracked legacy roots and duplicate assistant context**

  Run:

  ```powershell
  $workspace = (Resolve-Path ".").Path
  $paths = @("include", "lib", "demo", "resources", "CLAUDE.md")
  foreach ($p in $paths) {
      if (Test-Path -LiteralPath $p) {
          $resolved = (Resolve-Path -LiteralPath $p).Path
          if (-not $resolved.StartsWith($workspace)) {
              throw "Refusing to remove outside workspace: $resolved"
          }
          Remove-Item -LiteralPath $resolved -Recurse -Force
      }
  }
  foreach ($p in $paths) {
      if (Test-Path -LiteralPath $p) { throw "Path still exists after removal: $p" }
  }
  "Tracked legacy roots removed."
  ```

  Expected: prints `Tracked legacy roots removed.`

- [ ] **Step 4: Remove local generated and IDE directories**

  Run:

  ```powershell
  $workspace = (Resolve-Path ".").Path
  $paths = @(".claude", ".vs", ".idea", ".vscode", "build")
  foreach ($p in $paths) {
      if (Test-Path -LiteralPath $p) {
          $resolved = (Resolve-Path -LiteralPath $p).Path
          if (-not $resolved.StartsWith($workspace)) {
              throw "Refusing to remove outside workspace: $resolved"
          }
          Remove-Item -LiteralPath $resolved -Recurse -Force
      }
  }
  "Local generated and IDE directories removed when present."
  ```

  Expected: prints `Local generated and IDE directories removed when present.`

- [ ] **Step 5: Replace `.gitignore` with clean UTF-8 rules**

  Set `.gitignore` to this exact content:

  ```gitignore
  # Build output
  build/
  cmake-build-*/
  out/

  # IDE and editor state
  .vs/
  .idea/
  .vscode/

  # Generated Visual Studio project files
  *.sln
  *.slnx
  *.vcxproj
  *.vcxproj.filters
  *.vcxproj.user
  ```

- [ ] **Step 6: Reconfigure after deleting `build/`**

  Run:

  ```powershell
  $tc = "D:\Scoop\apps\vcpkg\2026.03.18\scripts\buildsystems\vcpkg.cmake"
  cmake -B build "-DCMAKE_TOOLCHAIN_FILE=$tc" -DCMAKE_BUILD_TYPE=Debug
  ```

  Expected: CMake configure completes and recreates `build/`.

- [ ] **Step 7: Build after removing legacy roots**

  Run:

  ```powershell
  cmake --build build --config Debug
  ```

  Expected: build completes and produces `build\Debug\HuanGL.exe`.

- [ ] **Step 8: Commit the cleanup removal**

  Run:

  ```powershell
  git status --short
  git add -A .gitignore include lib demo resources shader CLAUDE.md
  git commit -m "chore: remove legacy LearnOpenGL assets"
  ```

  Expected: commit succeeds. `build/`, `.vs/`, `.idea/`, and `.vscode/` should not be included in the commit because `.gitignore` ignores them.

## Task 3: Update README and AGENTS Context

**Files:**
- Modify: `README.md`
- Add or modify: `AGENTS.md`

- [ ] **Step 1: Replace `README.md` with the HuanGL baseline description**

  Set `README.md` to this exact content:

  ```markdown
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
    pipeline/      Render pipeline code added by renderer phases
    resource/      Resource management code added by renderer phases
    scene/         Scene system code added by renderer phases
    ui/            Debug UI code added by renderer phases

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
  ```

- [ ] **Step 2: Replace `AGENTS.md` with the single repository context file**

  Set `AGENTS.md` to this exact content:

  ```markdown
  # HuanGL - Project Context

  ## Project Summary

  **HuanGL** is an OpenGL 4.6 renderer learning and showcase project. It was refactored from an original LearnOpenGL single-file project, now archived at git tag `archive/learnogl-v1`.

  The active goal is to demonstrate C++ rendering architecture and gradually implement global illumination techniques for a portfolio-quality technical project.

  ## Build Method on Windows

  ```powershell
  $tc = "D:\Scoop\apps\vcpkg\2026.03.18\scripts\buildsystems\vcpkg.cmake"
  cmake -B build "-DCMAKE_TOOLCHAIN_FILE=$tc" -DCMAKE_BUILD_TYPE=Debug
  cmake --build build --config Debug
  ```

  Output:

  ```text
  build\Debug\HuanGL.exe
  ```

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
  ```

- [ ] **Step 3: Verify the documentation no longer describes LearnOGL as the active project**

  Run:

  ```powershell
  rg -n "LearnOGL|LearnOpenGL -|OpenGL 3.3" README.md AGENTS.md
  ```

  Expected: no matches. The archival reference should use `archive/learnogl-v1`, not old project branding.

- [ ] **Step 4: Commit the documentation update**

  Run:

  ```powershell
  git status --short
  git add README.md AGENTS.md
  git commit -m "docs: update HuanGL repository context"
  ```

  Expected: commit succeeds and includes `README.md` plus tracked `AGENTS.md`.

## Task 4: Final Validation and Cleanup Review

**Files:**
- Verify: entire repository

- [ ] **Step 1: Confirm removed path references are gone from build and source files**

  Run:

  ```powershell
  rg -n "learnopengl|include/GLFW|include\\GLFW|include/glm|include\\glm|src/glad\.c|src/stb_image\.cpp" CMakeLists.txt src shader
  ```

  Expected: no matches in active build, source, or shader files.

- [ ] **Step 2: Confirm external dependency layout**

  Run:

  ```powershell
  $paths = @(
    "external/glad/include/glad/glad.h",
    "external/glad/include/KHR/khrplatform.h",
    "external/glad/src/glad.c",
    "external/glm/glm/glm.hpp",
    "external/glm/glm/gtc/type_ptr.hpp",
    "external/stb/stb_image.h",
    "external/stb/stb_image.cpp",
    "shader/common/uniforms.glsl"
  )
  foreach ($p in $paths) {
      if (-not (Test-Path -LiteralPath $p)) { throw "Missing expected path: $p" }
  }
  "External layout verified."
  ```

  Expected: prints `External layout verified.`

- [ ] **Step 3: Confirm legacy roots are removed**

  Run:

  ```powershell
  $paths = @("include", "lib", "demo", "resources", "CLAUDE.md", ".claude")
  foreach ($p in $paths) {
      if (Test-Path -LiteralPath $p) { throw "Legacy path still exists: $p" }
  }
  "Legacy roots removed."
  ```

  Expected: prints `Legacy roots removed.`

- [ ] **Step 4: Reconfigure from the cleaned tree**

  Run:

  ```powershell
  $tc = "D:\Scoop\apps\vcpkg\2026.03.18\scripts\buildsystems\vcpkg.cmake"
  cmake -B build "-DCMAKE_TOOLCHAIN_FILE=$tc" -DCMAKE_BUILD_TYPE=Debug
  ```

  Expected: configure completes without errors.

- [ ] **Step 5: Build from the cleaned tree**

  Run:

  ```powershell
  cmake --build build --config Debug
  ```

  Expected: build completes and produces `build\Debug\HuanGL.exe`.

- [ ] **Step 6: Inspect final Git status**

  Run:

  ```powershell
  git status --short
  ```

  Expected: no untracked `.claude/`, `.vs/`, `.idea/`, `.vscode/`, `build/`, `include/`, `lib/`, `demo/`, or `resources/` entries. Any remaining changes should be intentional source, docs, shader common, `external/`, or `.gitignore` changes from this plan.

- [ ] **Step 7: Create a final validation commit if Task 4 produced tracked corrections**

  If Step 6 shows tracked corrections from this task, run:

  ```powershell
  git add -A
  git commit -m "chore: finalize repository cleanup"
  ```

  Expected: commit succeeds only when Task 4 required tracked corrections. If there are no tracked corrections, skip this step and record that no final commit was needed.
