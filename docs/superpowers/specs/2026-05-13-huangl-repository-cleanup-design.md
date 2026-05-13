# HuanGL Repository Cleanup Design

**Date:** 2026-05-13  
**Project:** HuanGL  
**Scope:** Convert the repository from a LearnOpenGL-derived archive shape into a minimal, buildable HuanGL baseline.

## Context

Phase 1 has produced the HuanGL foundation layer: `core`, `renderer`, GLAD2 loading, DSA-based GL wrappers, UBO definitions, and a minimal `App` loop. The repository still contains large amounts of historical LearnOpenGL material: old shaders, sample models, screenshots, local libraries, and a broad `include/` tree with third-party code mixed together.

The original LearnOpenGL state is already archived at git tag `archive/learnogl-v1`, so the active repository should no longer carry legacy demo material that is not part of the new HuanGL direction.

## Goals

- Make the active repository express HuanGL as a new OpenGL 4.6 renderer project.
- Keep the repository buildable after cleanup.
- Move vendored third-party code into `external/`.
- Stop vendoring GLFW and local GLFW libraries; use vcpkg/CMake targets for GLFW and Assimp.
- Remove legacy LearnOpenGL resources, shaders, screenshots, and helper wrappers.
- Keep only HuanGL source, docs, build files, shader common definitions, and the minimal third-party code currently needed by Phase 1.
- Use `AGENTS.md` as the single repository agent context file.

## Non-Goals

- Do not implement Phase 2 rendering passes in this cleanup.
- Do not add new models, textures, or shader programs.
- Do not introduce a resource manager, scene system, ImGui, or render pipeline implementation.
- Do not replace vcpkg dependency management.
- Do not preserve old LearnOpenGL demo assets in the active tree.

## Target Repository Shape

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
    include/glad/
    include/KHR/
    src/glad.c
  glm/
    glm/
    copying.txt
    readme.md
  stb/
    stb_image.h
    stb_image.cpp

shader/
  common/uniforms.glsl

docs/
  superpowers/specs/
  superpowers/plans/
```

The `src/` directories listed above are the intended architecture targets. Empty future directories do not need to be tracked by Git; they may remain locally or be recreated when their implementation starts. The active cleanup should preserve useful source structure but avoid keeping placeholder assets or old shader programs.

## Files and Directories to Remove

Tracked legacy content to remove:

- `include/learnopengl/`
- `include/GLFW/`
- old root `include/` location after `glad`, `KHR`, `glm`, and `stb_image.h` are migrated
- `lib/`
- `demo/`
- `resources/`
- all legacy shader directories except `shader/common/uniforms.glsl`
- `CLAUDE.md`

Local or generated content to remove from the working tree if present:

- `.claude/`
- `.vs/`
- `.idea/`
- `.vscode/`
- `build/`

Generated and IDE directories must also be covered by `.gitignore` so they do not reappear in normal status output.

## Third-Party Layout

### GLAD

Move:

- `include/glad/glad.h` to `external/glad/include/glad/glad.h`
- `include/KHR/khrplatform.h` to `external/glad/include/KHR/khrplatform.h`
- `src/glad.c` to `external/glad/src/glad.c`

Keep source includes as:

```cpp
#include <glad/glad.h>
```

The CMake include path will make this stable.

### GLM

Move only the usable GLM header tree:

- `include/glm/glm/` to `external/glm/glm/`
- `include/glm/copying.txt` to `external/glm/copying.txt`
- `include/glm/readme.md` to `external/glm/readme.md`

Drop GLM docs, tests, CMake packaging files, utility files, and CI metadata from the active repository.

Keep source includes as:

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
```

### stb

Move:

- `include/stb_image.h` to `external/stb/stb_image.h`
- `src/stb_image.cpp` to `external/stb/stb_image.cpp`

Keep source includes as:

```cpp
#include <stb_image.h>
```

### GLFW and Assimp

Do not keep GLFW headers or static libraries in the repository. GLFW and Assimp remain vcpkg-managed dependencies resolved by:

```cmake
find_package(glfw3 3.3 REQUIRED CONFIG)
find_package(assimp REQUIRED CONFIG)
```

Assimp is not currently used by Phase 1 source after removing `include/learnopengl/`, but keeping the CMake dependency is acceptable because Phase 2+ will need model loading.

## CMake Changes

`CMakeLists.txt` should stop using the old root `include/` directory:

```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/external/glad/include
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/stb
)
```

The project source glob should collect HuanGL-owned source only:

```cmake
file(GLOB_RECURSE SRC
    src/*.cpp
    src/*.c
)
```

Third-party implementation files should be added explicitly:

```cmake
target_sources(${PROJECT_NAME} PRIVATE
    external/glad/src/glad.c
    external/stb/stb_image.cpp
)
```

This keeps vendored dependencies out of the project source glob and avoids accidentally compiling third-party tests or examples.

## Include Reference Updates

Most HuanGL source includes can remain unchanged because CMake include roots preserve the public third-party include names:

- `<glad/glad.h>`
- `<glm/glm.hpp>`
- `<glm/gtc/type_ptr.hpp>`
- `<stb_image.h>`
- `<GLFW/glfw3.h>`

The implementation must verify with `rg "#include"` that no project source still depends on:

- `include/learnopengl`
- local `include/GLFW`
- relative paths into the old root `include/`
- old `src/glad.c` or `src/stb_image.cpp` paths in build config

## Shader and Resource Baseline

Keep only:

- `shader/common/uniforms.glsl`

Remove all old shader programs under directories such as:

- `shader/pbr/`
- `shader/deferred/`
- `shader/bloom/`
- `shader/ssao/`
- `shader/screen/`
- `shader/background/`
- `shader/cubemap/`
- `shader/irradiance/`
- `shader/prefilter/`
- `shader/Blinn-Phong/`
- `shader/blur/`
- `shader/brdf/`

New Phase 2 shader directories and programs should be introduced by the Phase 2 implementation plan, not preserved from the old project.

Remove all current `resources/` content. Models and textures will be added later according to the active HuanGL scene and resource manager designs.

## Documentation Changes

Update `README.md` so it describes:

- HuanGL, not LearnOGL.
- Current Phase 1 status.
- Windows vcpkg build commands.
- The `external/` dependency layout.
- The fact that models, textures, and phase-specific shaders will be reintroduced incrementally.

Update `AGENTS.md` so it remains the single source of repository context:

- Replace old `include/` references with `external/`.
- Note that `CLAUDE.md` and `.claude/` have been removed.
- Note that legacy assets and shaders were intentionally removed because `archive/learnogl-v1` preserves the old state.
- Keep the GLAD2, DSA, namespace, MSVC, and CMake reconfigure reminders.

Delete `CLAUDE.md` because it duplicates `AGENTS.md` and is specific to a different assistant ecosystem.

## Gitignore Changes

Rewrite `.gitignore` in clean UTF-8 text and cover at least:

```gitignore
build/
cmake-build-*/
out/
.vs/
.idea/
.vscode/
*.sln
*.vcxproj
*.vcxproj.user
*.vcxproj.filters
```

Do not ignore `external/`, `docs/`, `src/`, or `shader/common/`.

## Validation

After cleanup implementation:

1. Re-run CMake configure with the vcpkg toolchain because `glad.c` and `stb_image.cpp` moved.
2. Build Debug:

   ```powershell
   $tc = "D:\Scoop\apps\vcpkg\2026.03.18\scripts\buildsystems\vcpkg.cmake"
   cmake -B build "-DCMAKE_TOOLCHAIN_FILE=$tc" -DCMAKE_BUILD_TYPE=Debug
   cmake --build build --config Debug
   ```

3. Verify no source includes reference removed paths:

   ```powershell
   rg "learnopengl|include/GLFW|include\\\\GLFW|include/glm|include\\\\glm|src/glad.c|src/stb_image.cpp"
   ```

4. Verify tracked status matches the cleanup design:

   ```powershell
   git status --short
   ```

5. Confirm the output executable remains `build\Debug\HuanGL.exe`.

## Risks and Mitigations

- **Risk:** CMake may still try to compile old `src/glad.c` or `src/stb_image.cpp`.
  **Mitigation:** Move these files out of `src/` and add their new `external/` paths with `target_sources`.

- **Risk:** GLFW headers disappear from the repository before CMake uses vcpkg include directories.
  **Mitigation:** Keep `find_package(glfw3 3.3 REQUIRED CONFIG)` and link the imported GLFW target.

- **Risk:** The cleanup accidentally removes future HuanGL context.
  **Mitigation:** Keep `docs/superpowers/`, `AGENTS.md`, and Phase 1 source intact; rely on `archive/learnogl-v1` for historical assets.

- **Risk:** Empty future directories are not represented in Git.
  **Mitigation:** Recreate phase directories when implementation starts, or keep lightweight `.gitkeep` files only if a specific plan needs them.

## Acceptance Criteria

- The repository no longer contains old LearnOpenGL shaders, resources, demo screenshots, local GLFW libraries, or `include/learnopengl`.
- Third-party vendored code is under `external/`.
- `CMakeLists.txt` builds from the new external layout.
- `README.md` and `AGENTS.md` describe the cleaned HuanGL repository.
- `CLAUDE.md` and `.claude/` are gone.
- Debug build succeeds after reconfigure.
