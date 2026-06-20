# GI Evaluation Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an in-engine reference path tracer, error-metric module, and comparison harness so the renderer becomes a GI testbed measured against path-traced ground truth.

**Architecture:** Three new units attach as a tail bypass to the existing six-stage pipeline (unchanged): `PathTracerScene` (CPU geometry → median-split BVH → SSBOs), `PathTracerStage` (compute path tracer with progressive accumulation, writes `ReferenceOutputs`), and `ComparisonStage` (per-pixel error compute + RMSE/MAPE readback + view modes to backbuffer). A primitive Cornell scene is the diffuse-GI validation target. The current renderer (direct sun + IBL ambient, no interreflection) is the first contestant against ground truth; no GI technique ships in this plan.

**Tech Stack:** C++17, OpenGL 4.6 Core + Direct State Access, GLSL compute shaders (SSBO + image load/store), GLM, vcpkg (MSVC/clang-cl), CMake + Ninja.

## Global Constraints

- OpenGL 4.6 Core Profile; Direct State Access (`glCreate*`/`glNamed*`/`glBindTextureUnit`); no RHI abstraction layer.
- C++17. Header guards via `#pragma once`. Namespace `HuanGL`.
- No new third-party dependencies; only what vcpkg already provides.
- Pipeline stages implement `IPipelineStage` (Init/Resize/InvalidateHistory/Execute/GetName) and live under `src/pipeline/stages/`; outputs are published into the typed `PipelineResources` registry; UI mutates `ApplicationState`, stages read `FrameContext`/`RenderSceneView` (render produces data, UI reads state).
- This is a learning project: teaching comments that explain WHY a rendering technique works are an asset; prefer them over terse minimalism.
- No unit-test framework exists. Verification is **compile + run + observe** (visual or numeric), except pure-CPU math (BVH, ray intersection) which is checked with assertion-based self-tests runnable from a `--selftest` CLI path.
- GLSL std430: avoid `vec3` members in SSBO structs (16-byte alignment traps). Use `vec4`/`ivec4`/`uvec4` and pad explicitly; the CPU mirror uses `glm::vec4`/`glm::ivec4` so `sizeof` matches byte-for-byte.
- After adding any new `.cpp`, re-run the CMake configure step (the build uses `GLOB_RECURSE`).
- New shaders live under `shader/`; load them through `Shader` with paths relative to the shader base path (see how existing stages construct `Shader`).

**Build/run commands (used throughout):**

```powershell
# Configure (after adding new .cpp files)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
# Build
cmake --build build --config Debug
# Run
build\Debug\HuanGL.exe
```

---

## File Structure

**Create:**
- `src/pipeline/PathTracerScene.h` / `.cpp` — world-triangle gather, median-split BVH, SSBO packing/upload.
- `src/pipeline/Bvh.h` / `.cpp` — pure-CPU BVH builder + ray/AABB/triangle math + self-tests.
- `src/pipeline/stages/PathTracerStage.h` / `.cpp` — compute path tracer, accumulation, `ReferenceOutputs`.
- `src/pipeline/stages/ComparisonStage.h` / `.cpp` — error compute, metric readback, view modes.
- `src/scene/CornellScene.h` / `.cpp` — primitive Cornell box validation scene.
- `shader/pathtracer/pathtrace.comp` — the path tracer.
- `shader/comparison/error.comp` — per-pixel error image.
- `shader/comparison/composite.frag` (+ reuse a fullscreen vertex path) — view-mode compositor to backbuffer.

**Modify:**
- `src/renderer/Schema.h` — add `CpuGeometry` and a `std::shared_ptr<CpuGeometry>` member on `Mesh`.
- `src/scene/TestScene.cpp` — fill CPU geometry in its `BuildMesh` helper (so the shared helper pattern stays correct).
- `src/renderer/FrameContext.h` — add path-tracer/comparison settings to `RenderSettings`/`DebugSettings`.
- `src/app/ApplicationState.h` — add comparison metric/sample-count readout fields.
- `src/pipeline/RenderPipeline.h` / `.cpp` — register the two new stages; expose comparison readouts; pass `hdrPath` to `PathTracerStage`.
- `src/core/App.cpp` — register `CornellScene`; copy comparison readouts into `ApplicationState`; reset accumulation on camera move / scene switch.
- `src/ui/DebugUI.cpp` — add the "GI Comparison" panel.
- `docs/architecture.md` — roadmap note.

---

## Task 1: CPU geometry retained on `Mesh`

**Files:**
- Modify: `src/renderer/Schema.h`
- Modify: `src/scene/TestScene.cpp:36-60` (the `BuildMesh` helper)
- Test: assertion self-check added in Task 3's `--selftest` (geometry presence is observed at runtime here)

**Interfaces:**
- Produces: `struct CpuGeometry { std::vector<Vertex> vertices; std::vector<uint32_t> indices; };` and `Mesh::cpuGeometry` (`std::shared_ptr<CpuGeometry>`, may be null when a mesh was built without retention).

- [ ] **Step 1: Add `CpuGeometry` and the member**

In `src/renderer/Schema.h`, after the `SubMesh` struct and before `Mesh`, add:

```cpp
// CPU-side copy of mesh geometry, retained for systems that need triangles on
// the host (e.g. BVH construction for the reference path tracer). The realtime
// renderer never reads this; it draws from the GL buffers below. Null when a
// mesh was uploaded without retaining a host copy.
struct CpuGeometry {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
};
```

Then add to `struct Mesh` (after `subMeshes`):

```cpp
    std::shared_ptr<CpuGeometry> cpuGeometry; // optional host-side triangles
```

`Vertex` is already declared later in the file; move the `Vertex` struct definition above `CpuGeometry`, or forward the need by relocating `CpuGeometry` below `Vertex`. Place `CpuGeometry` **after** the `Vertex` definition (end of the structs, just before the closing namespace is fine) and keep the `Mesh::cpuGeometry` member referring to it via the shared_ptr (incomplete type is OK for `shared_ptr` members as long as the full type is visible in this header — so define `CpuGeometry` before `Mesh`). Concretely: move `struct Vertex { ... };` to the top of the struct list (right after `#include`s), then define `CpuGeometry`, then `SubMesh`, then `Mesh`.

- [ ] **Step 2: Fill CPU geometry in `TestScene::BuildMesh`**

In `src/scene/TestScene.cpp`, inside `BuildMesh` just before `return m;`:

```cpp
    m->cpuGeometry = std::make_shared<CpuGeometry>();
    m->cpuGeometry->vertices = verts;
    m->cpuGeometry->indices  = idx;
```

- [ ] **Step 3: Build**

Run: `cmake --build build --config Debug`
Expected: compiles clean; no behavior change at runtime (the new field is unused so far).

- [ ] **Step 4: Commit**

```bash
git add src/renderer/Schema.h src/scene/TestScene.cpp
git commit -m "feat: retain optional CPU geometry on Mesh for BVH construction"
```

---

## Task 2: Cornell validation scene

**Files:**
- Create: `src/scene/CornellScene.h`, `src/scene/CornellScene.cpp`
- Modify: `src/core/App.cpp` (register the scene)
- Test: runtime visual — the box renders under the realtime pipeline.

**Interfaces:**
- Consumes: `CpuGeometry`/`Mesh` from Task 1; `World`, `Entity`, `Material`, `Scene` (existing).
- Produces: `class CornellScene : public Scene { void Init(ResourceManager&) override; };`

- [ ] **Step 1: Header**

`src/scene/CornellScene.h`:

```cpp
#pragma once
#include "Scene.h"

namespace HuanGL {

// A primitive Cornell box: a unit-ish room with colored side walls and two
// inner boxes, all constant-color (factor-only) diffuse materials. This is the
// canonical diffuse-GI validation scene — color bleeding from the red/green
// walls onto the boxes is the headline result the reference path tracer must
// reproduce and the realtime renderer (no interreflection) must miss.
class CornellScene : public Scene {
public:
    void Init(ResourceManager& rm) override;
};

} // namespace HuanGL
```

- [ ] **Step 2: Implementation**

`src/scene/CornellScene.cpp`. Reuse the `BuildMesh` pattern from `TestScene.cpp:36-60` (copy the helper in as a local `static`, including the Task 1 CPU-geometry fill). Build the room from axis-aligned quads and the inner boxes from cubes. Provide a `static std::vector<Vertex> Quad(p0,p1,p2,p3,normal)` and a `static void AppendBox(...)` helper, or assemble explicit quads. Materials (factor-only, metallic 0):

```cpp
// 0 white, 1 red, 2 green
auto& mats = world_.GetMaterials();
mats.push_back({{},{},{},{}, {0.73f,0.73f,0.73f,1}, 1.0f, 0.0f}); // white
mats.push_back({{},{},{},{}, {0.65f,0.05f,0.05f,1}, 1.0f, 0.0f}); // red
mats.push_back({{},{},{},{}, {0.12f,0.45f,0.15f,1}, 1.0f, 0.0f}); // green
```

Geometry (a 1×1×1 box centered so the camera looks in through the open -Z face): floor/ceiling/back wall = white (mat 0), left wall = red (mat 1), right wall = green (mat 2), plus two white inner boxes. Each wall is one quad entity; give each entity a `MeshRenderer` from `BuildMesh(quadVerts, {0,1,2, 0,2,3}, matIdx)`. Set the sun to a soft downward light or disable it (set `intensity` low) so indirect dominates:

```cpp
auto& sun = world_.GetSunLight();
sun.direction = glm::normalize(glm::vec3(0.0f, -1.0f, -0.05f));
sun.color     = {1.0f, 1.0f, 1.0f};
sun.intensity = 3.0f;
world_.GetAmbient() = {0.0f, 0.0f, 0.0f}; // no ambient hack: let GI be the difference
```

Ensure every wall normal points **inward** (toward the room interior) so lighting and path-tracer hemisphere sampling agree.

- [ ] **Step 3: Register the scene in `App`**

In `src/core/App.cpp`, where `TestScene` is registered (search for `RegisterRequired`/`TestScene`), add:

```cpp
state_.sceneRegistry.RegisterRequired(
    std::make_unique<CornellScene>(), "Cornell", resourceManager_);
```

Add `#include "../scene/CornellScene.h"` near the other scene includes. Match the exact `RegisterRequired`/`RegisterOptional` signature already used (see `SceneRegistry.h`).

- [ ] **Step 4: Configure + build**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build --config Debug`
Expected: compiles; new `.cpp` picked up by GLOB_RECURSE.

- [ ] **Step 5: Run and observe**

Run: `build\Debug\HuanGL.exe`, press `N` to cycle to the "Cornell" scene.
Expected: a colored box room renders. Corners look flat/dark (no bounce light yet) — that missing bounce is exactly what the reference will later reveal.

- [ ] **Step 6: Commit**

```bash
git add src/scene/CornellScene.h src/scene/CornellScene.cpp src/core/App.cpp
git commit -m "feat: add Cornell box validation scene"
```

---

## Task 3: CPU BVH builder + ray math (self-tested)

**Files:**
- Create: `src/pipeline/Bvh.h`, `src/pipeline/Bvh.cpp`
- Modify: `src/core/App.cpp` (wire a `--selftest` early-exit path)
- Test: assertion-based self-tests in `Bvh.cpp`, invoked from `--selftest`.

**Interfaces:**
- Produces:
```cpp
struct BvhTri { glm::vec3 p0, p1, p2; glm::vec3 n0, n1, n2; uint32_t materialIndex; };
struct BvhNode { glm::vec3 aabbMin; glm::vec3 aabbMax; int leftFirst; int triCount; }; // triCount>0 => leaf, leftFirst=first tri index; triCount==0 => internal, leftFirst=left child (right=left+1)
class Bvh {
public:
    void Build(std::vector<BvhTri> tris);            // median-split; reorders tris
    const std::vector<BvhNode>& Nodes() const;
    const std::vector<BvhTri>&  Tris()  const;
    bool Empty() const;
};
namespace BvhSelfTest { bool RunAll(); }             // returns true if all asserts pass
```

- [ ] **Step 1: Write the failing self-test first**

In `src/pipeline/Bvh.cpp`, add a `BvhSelfTest::RunAll()` that builds a known tri set and asserts. Start with the test only (no `Build` body yet) so it fails to link/compile:

```cpp
namespace BvhSelfTest {
bool RunAll() {
    using glm::vec3;
    // Two triangles forming a quad on the XZ plane at y=0.
    std::vector<BvhTri> tris = {
        {{-1,0,-1},{1,0,-1},{1,0,1}, {0,1,0},{0,1,0},{0,1,0}, 0},
        {{-1,0,-1},{1,0,1},{-1,0,1}, {0,1,0},{0,1,0},{0,1,0}, 0},
    };
    Bvh bvh; bvh.Build(tris);
    assert(!bvh.Empty());
    assert(!bvh.Nodes().empty());
    // Root AABB must enclose all geometry.
    const BvhNode& root = bvh.Nodes()[0];
    assert(root.aabbMin.x <= -1.0f && root.aabbMax.x >= 1.0f);
    assert(root.aabbMin.z <= -1.0f && root.aabbMax.z >= 1.0f);
    // A ray straight down through the center must hit.
    float t; int hit = BvhSelfTest_ClosestHit(bvh, vec3(0,1,0), vec3(0,-1,0), t);
    assert(hit >= 0 && t > 0.9f && t < 1.1f);
    // A ray pointing away must miss.
    hit = BvhSelfTest_ClosestHit(bvh, vec3(0,1,0), vec3(0,1,0), t);
    assert(hit < 0);
    return true;
}
} // namespace BvhSelfTest
```

`BvhSelfTest_ClosestHit` is a CPU mirror of the traversal you will port to GLSL; declare it in `Bvh.h` and implement it in `Bvh.cpp` (Step 3).

- [ ] **Step 2: Wire `--selftest` and run to see it fail**

In `src/core/App.cpp` `main`/entry (or wherever argv is available — if `main` is elsewhere, search for `int main`), add at the very top before window creation:

```cpp
for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--selftest") {
        bool ok = BvhSelfTest::RunAll();
        std::printf("selftest: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }
}
```

Add `#include "../pipeline/Bvh.h"` and `#include <cstdio>`.

Run: `cmake --build build --config Debug`
Expected: FAILS to link — `Bvh::Build` / `BvhSelfTest_ClosestHit` undefined.

- [ ] **Step 3: Implement the BVH + ray math**

`src/pipeline/Bvh.h`:

```cpp
#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace HuanGL {

struct BvhTri  { glm::vec3 p0,p1,p2; glm::vec3 n0,n1,n2; uint32_t materialIndex; };
struct BvhNode { glm::vec3 aabbMin; glm::vec3 aabbMax; int leftFirst; int triCount; };

class Bvh {
public:
    void Build(std::vector<BvhTri> tris);
    const std::vector<BvhNode>& Nodes() const { return nodes_; }
    const std::vector<BvhTri>&  Tris()  const { return tris_; }
    bool Empty() const { return tris_.empty(); }
private:
    std::vector<BvhNode> nodes_;
    std::vector<BvhTri>  tris_;
};

// CPU reference traversal (mirrors the GLSL one). Returns triangle index or -1.
int BvhSelfTest_ClosestHit(const Bvh& bvh, glm::vec3 ro, glm::vec3 rd, float& tOut);

namespace BvhSelfTest { bool RunAll(); }

} // namespace HuanGL
```

`src/pipeline/Bvh.cpp`: implement median-split. Algorithm (teaching comment it):
1. Compute each triangle's centroid and AABB.
2. Recursively: compute node AABB over its tri range; if `count <= 4` make a leaf; else pick the largest-extent axis, sort the tri range by centroid on that axis, split at the median, recurse. Store nodes in a flat array; internal node `leftFirst` = index of left child, right child = left+1, `triCount=0`.
Include `<algorithm>`, `<cassert>`, `<cfloat>`. Implement ray-AABB slab test and Möller–Trumbore ray-triangle for `BvhSelfTest_ClosestHit` (stack-based traversal, nearest hit). These two functions are the spec you will translate verbatim into GLSL in Task 6 — keep them simple and branch-light.

- [ ] **Step 4: Build and run the self-test**

Run: `cmake --build build --config Debug` then `build\Debug\HuanGL.exe --selftest`
Expected: prints `selftest: PASS` and exits 0.

- [ ] **Step 5: Commit**

```bash
git add src/pipeline/Bvh.h src/pipeline/Bvh.cpp src/core/App.cpp
git commit -m "feat: CPU median-split BVH + ray math with self-tests"
```

---

## Task 4: `PathTracerScene` — world triangles → BVH → SSBOs

**Files:**
- Create: `src/pipeline/PathTracerScene.h`, `src/pipeline/PathTracerScene.cpp`
- Test: runtime log of node/triangle/material counts on the Cornell scene.

**Interfaces:**
- Consumes: `RenderSceneView` (renderables with `mesh`, `materials`, `modelMatrix`), `Mesh::cpuGeometry` (Task 1), `Bvh` (Task 3).
- Produces:
```cpp
class PathTracerScene {
public:
    void Build(const RenderSceneView& scene); // gather → BVH → pack → upload
    bool Ready() const;                        // false if no CPU geometry found
    void BindSSBOs() const;                    // binds nodes/tris/materials to fixed points
    uint32_t TriangleCount() const;
    // SSBO binding points (shared with pathtrace.comp):
    static constexpr GLuint kNodeBinding = 3;  // 0,1,2 are taken by Camera/Lights/Time UBOs
    static constexpr GLuint kTriBinding  = 4;
    static constexpr GLuint kMatBinding  = 5;
};
```

- [ ] **Step 1: Define std430-safe GPU structs**

In `PathTracerScene.h`, define the packed mirrors (vec4 only — see Global Constraints):

```cpp
// std430 layout; mirrored exactly in pathtrace.comp. vec4 everywhere to dodge
// the vec3 16-byte alignment trap.
struct GpuTri {
    glm::vec4 p0, p1, p2;   // .xyz position, .w unused
    glm::vec4 n0, n1, n2;   // .xyz normal,   .w unused
    glm::uvec4 mat;         // .x = material index, .yzw pad
};                          // 7*16 = 112 bytes
struct GpuNode {
    glm::vec4 aabbMin;      // .xyz min, .w unused
    glm::vec4 aabbMax;      // .xyz max, .w unused
    glm::ivec4 meta;        // .x = leftFirst, .y = triCount, .zw pad
};                          // 3*16 = 48 bytes
struct GpuMaterial {
    glm::vec4 baseColor;    // .rgb albedo, .a unused
    glm::vec4 mr;           // .x metallic, .y roughness, .zw pad
};                          // 2*16 = 32 bytes
```

- [ ] **Step 2: Implement `Build`**

`PathTracerScene.cpp`:
1. For each `Renderable` with `renderable.mesh->cpuGeometry`, read its `vertices`/`indices`, and for each sub-mesh walk index triples. Transform position by `modelMatrix`, transform normal by `mat3(transpose(inverse(modelMatrix)))` (teaching comment: inverse-transpose for normals under non-uniform scale — same reason as the GBuffer normal matrix). Emit a `BvhTri` with `materialIndex` = the sub-mesh's `materialIndex` **plus a per-renderable material base offset** (concatenate all renderables' materials into one global material array; track the running offset).
2. If no triangles were gathered, set a `ready_ = false` flag and return (log a warning: `"PathTracerScene: active scene has no CPU geometry; reference unavailable"`).
3. `bvh_.Build(std::move(tris))`.
4. Pack `bvh_.Nodes()` → `GpuNode[]`, `bvh_.Tris()` → `GpuTri[]`, the concatenated materials → `GpuMaterial[]`.
5. Upload each into a `Buffer(GL_SHADER_STORAGE_BUFFER)`; store the three buffers as members. `BindSSBOs()` calls `buf.BindBase(kNodeBinding)` etc.

Use `std::make_unique<Buffer>(GL_SHADER_STORAGE_BUFFER)` and `Upload(data, bytes)`.

- [ ] **Step 3: Temporary count log (verification hook)**

At the end of `Build`, add a one-time `std::printf("PathTracerScene: %zu nodes, %u tris, %zu materials\n", nodes.size(), TriangleCount(), materials.size());`. (Removed in Task 9 cleanup.)

- [ ] **Step 4: Build**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build --config Debug`
Expected: compiles. Not yet invoked (no stage calls it); no runtime change.

- [ ] **Step 5: Commit**

```bash
git add src/pipeline/PathTracerScene.h src/pipeline/PathTracerScene.cpp
git commit -m "feat: PathTracerScene builds BVH and uploads geometry SSBOs"
```

---

## Task 5: `PathTracerStage` plumbing — accumulation, reset, ReferenceOutputs

This task ships the stage with a **placeholder compute shader** that only writes camera-ray direction / environment color (no BVH yet), to validate dispatch, the accumulation buffer, reset wiring, and resource publication. Task 6 replaces the shader body with the real path tracer.

**Files:**
- Create: `src/pipeline/stages/PathTracerStage.h`, `.cpp`, `shader/pathtracer/pathtrace.comp`
- Modify: `src/renderer/FrameContext.h` (settings), `src/pipeline/RenderPipeline.h/.cpp` (register + hdrPath)
- Test: runtime — a temporary debug view shows the accumulation buffer; spp climbs when frozen, resets on camera move.

**Interfaces:**
- Consumes: `RenderSceneView` (from `PipelineResources`), `FrameContext.camera`, `PathTracerScene` (Task 4).
- Produces:
```cpp
struct ReferenceOutputs { std::shared_ptr<Texture> hdr; uint32_t sampleCount = 0; };
class PathTracerStage : public IPipelineStage {
public:
    explicit PathTracerStage(std::string hdrPath);
    const char* GetName() const override { return "PathTracerStage"; }
    void Init(int,int) override; void Resize(int,int) override;
    void InvalidateHistory() override;   // clears accumulation, sampleCount=0
    void Execute(PipelineResources&, const FrameContext&) override;
};
```

- [ ] **Step 1: Add settings to `FrameContext.h`**

In `struct RenderSettings` add:

```cpp
    bool pathTracerEnabled = false; // reference path tracer (off by default; expensive)
    int  pathTracerSpp     = 1;     // samples added per frame
    int  pathTracerMaxBounces = 4;
```

In `struct DebugSettings` add:

```cpp
    enum class CompareView { Realtime = 0, Reference = 1, Split = 2, ErrorHeatmap = 3 };
    CompareView compareView = CompareView::Realtime;
    float errorScale = 1.0f;        // heatmap sensitivity
```

- [ ] **Step 2: Placeholder compute shader**

`shader/pathtracer/pathtrace.comp` (env-only, validates plumbing):

```glsl
#version 460 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba32f, binding = 0) uniform image2D uAccum; // accumulation target

uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform int  uSampleIndex;   // 0 on reset
uniform vec2 uResolution;

vec3 rayDir(vec2 uv) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    return normalize(world.xyz / world.w - uCamPos);
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    if (px.x >= int(uResolution.x) || px.y >= int(uResolution.y)) return;
    vec2 uv = (vec2(px) + 0.5) / uResolution;
    vec3 d = rayDir(uv);
    vec3 sample_ = 0.5 + 0.5 * d; // placeholder: visualize ray direction
    vec3 prev = (uSampleIndex == 0) ? vec3(0.0) : imageLoad(uAccum, px).rgb;
    imageStore(uAccum, px, vec4(prev + sample_, 1.0));
}
```

- [ ] **Step 3: Stage implementation**

Model `Init`/`Resize` on `BloomStage`/`TAAStage` (read those files). Details:
- `Init(w,h)`: create `accum_ = Texture::Create2D(w,h, GL_RGBA32F, GL_RGBA, GL_FLOAT)`; create `shader_ = std::make_unique<Shader>("pathtracer/pathtrace.comp")`; load the env once: `env_ = Texture::LoadHDR(hdrPath_)` (used in Task 6); `sampleCount_ = 0`.
- `Resize`: recreate `accum_` and call `InvalidateHistory()`.
- `InvalidateHistory()`: `sampleCount_ = 0` (accumulation is overwritten when `uSampleIndex==0`).
- `Execute(resources, frame)`:
  - If `!frame.renderSettings.pathTracerEnabled`, return immediately (zero cost).
  - On first enable / scene change, (re)build `scene_.Build(resources.Get<RenderSceneView>())`. Detect scene change by comparing a cached pointer/hash; simplest: rebuild when `sampleCount_ == 0` and a `dirty_` flag is set by `InvalidateHistory`. For Task 5, rebuild every time `sampleCount_==0`.
  - If `!scene_.Ready()`, return.
  - Bind `accum_` as image unit 0: `accum_->BindImage(0, GL_READ_WRITE, GL_RGBA32F)`.
  - Bind SSBOs: `scene_.BindSSBOs()`.
  - Set uniforms: `uInvViewProj`, `uCamPos`, `uResolution`, `uSampleIndex = sampleCount_`, plus Task 6 light/env uniforms.
  - `shader_->Dispatch((w+7)/8, (h+7)/8)`.
  - `++sampleCount_`.
  - Publish: `ReferenceOutputs out; out.hdr = accum_; out.sampleCount = sampleCount_; resources.Set(out);` — consumers divide `hdr` by `sampleCount` to get the mean.

- [ ] **Step 4: Register the stage + pass hdrPath**

In `RenderPipeline::BuildStages`, after `PostProcessStage` push:

```cpp
stages_.push_back(std::make_unique<PathTracerStage>(hdrPath));
```

Add `#include "stages/PathTracerStage.h"`. (Placing it after PostProcess keeps it a tail bypass; the profiler will time it like any stage.)

- [ ] **Step 5: Temporary view of the accumulation buffer**

To observe before the comparison UI exists, temporarily make `PostProcessStage` or a quick path display the reference. Simplest: in Task 7 the ComparisonStage shows it; for now verify via RenderDoc or a one-line temporary blit. **Acceptable interim check:** add a temporary `std::printf` of `sampleCount_` each frame and confirm it climbs while frozen and resets on camera move (wire reset in Step 6). Remove the printf in Task 9.

- [ ] **Step 6: Reset on camera move / scene switch**

In `src/core/App.cpp`, where the frame is assembled (search for where `pipeline_.Execute` / `InvalidateHistory` is called for TAA on scene switch), also call `pipeline_.InvalidateHistory()` when the camera moved this frame and the path tracer is enabled. If a camera-moved signal does not exist, compare the current `camera` view matrix to the previous frame's and invalidate on change. (TAA already needs similar logic; reuse its trigger if present.)

- [ ] **Step 7: Configure, build, run**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` && `cmake --build build --config Debug` && `build\Debug\HuanGL.exe`
Expected: with `pathTracerEnabled` temporarily forced `true` (set the default to `true` for this step only, or toggle once the UI lands), the printf shows `sampleCount` increasing while the camera is still and resetting to 1 right after a `WASD`/mouse move. Revert the temporary default to `false`.

- [ ] **Step 8: Commit**

```bash
git add src/pipeline/stages/PathTracerStage.h src/pipeline/stages/PathTracerStage.cpp shader/pathtracer/pathtrace.comp src/renderer/FrameContext.h src/pipeline/RenderPipeline.h src/pipeline/RenderPipeline.cpp src/core/App.cpp
git commit -m "feat: PathTracerStage plumbing with progressive accumulation"
```

---

## Task 6: Real path tracer in `pathtrace.comp`

**Files:**
- Modify: `shader/pathtracer/pathtrace.comp`, `src/pipeline/stages/PathTracerStage.cpp` (add light/env uniforms + bind env texture)
- Test: runtime — white-furnace and Cornell-convergence checks.

**Interfaces:**
- Consumes: SSBO bindings 3/4/5 (`PathTracerScene::kNodeBinding/kTriBinding/kMatBinding`), `uAccum` image, camera/light/env uniforms.

- [ ] **Step 1: Replace the shader body**

Rewrite `shader/pathtracer/pathtrace.comp` as a unidirectional diffuse path tracer. Mirror the std430 structs from Task 4 and the CPU traversal from Task 3:

```glsl
#version 460 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba32f, binding = 0) uniform image2D uAccum;

struct GpuTri  { vec4 p0,p1,p2; vec4 n0,n1,n2; uvec4 mat; };
struct GpuNode { vec4 aabbMin; vec4 aabbMax; ivec4 meta; }; // meta.x=leftFirst meta.y=triCount
struct GpuMat  { vec4 baseColor; vec4 mr; };

layout(std430, binding = 3) readonly buffer Nodes { GpuNode nodes[]; };
layout(std430, binding = 4) readonly buffer Tris  { GpuTri  tris[];  };
layout(std430, binding = 5) readonly buffer Mats  { GpuMat  mats[];  };

uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec2 uResolution;
uniform int  uSampleIndex;
uniform int  uMaxBounces;
uniform int  uSppPerFrame;

uniform vec3  uSunDir;       // normalized direction the light travels
uniform vec3  uSunColor;
uniform float uSunIntensity;
uniform sampler2D uEnvMap;   // HDR equirectangular

const float PI = 3.14159265359;
const float INF = 1e30;

// ---- RNG (PCG hash) ----
uint pcg(inout uint s){ s = s*747796405u+2891336453u; uint w=((s>>((s>>28u)+4u))^s)*277803737u; return (w>>22u)^w; }
float rnd(inout uint s){ return float(pcg(s)) * (1.0/4294967296.0); }

vec2 dirToEquirect(vec3 d){ return vec2(atan(d.z,d.x)/(2.0*PI)+0.5, acos(clamp(d.y,-1.0,1.0))/PI); }
vec3 sampleEnv(vec3 d){ return texture(uEnvMap, dirToEquirect(d)).rgb; }

bool slabAABB(vec3 ro, vec3 inv, vec3 mn, vec3 mx, float tmax){
    vec3 t0=(mn-ro)*inv, t1=(mx-ro)*inv;
    vec3 a=min(t0,t1), b=max(t0,t1);
    float tn=max(max(a.x,a.y),a.z), tf=min(min(b.x,b.y),b.z);
    return tf>=max(tn,0.0) && tn<tmax;
}

// Möller–Trumbore. Returns t>0 on hit.
bool hitTri(vec3 ro, vec3 rd, GpuTri tr, out float t, out vec3 bary){
    vec3 e1=tr.p1.xyz-tr.p0.xyz, e2=tr.p2.xyz-tr.p0.xyz;
    vec3 pv=cross(rd,e2); float det=dot(e1,pv);
    if(abs(det)<1e-8) return false;
    float inv=1.0/det; vec3 tv=ro-tr.p0.xyz;
    float u=dot(tv,pv)*inv; if(u<0.0||u>1.0) return false;
    vec3 qv=cross(tv,e1); float v=dot(rd,qv)*inv; if(v<0.0||u+v>1.0) return false;
    t=dot(e2,qv)*inv; if(t<=1e-4) return false;
    bary=vec3(1.0-u-v,u,v); return true;
}

int closestHit(vec3 ro, vec3 rd, out float tBest, out vec3 nrm, out uint matIdx){
    vec3 inv=1.0/rd; tBest=INF; int best=-1;
    int stack[64]; int sp=0; stack[sp++]=0;
    while(sp>0){
        GpuNode nd=nodes[stack[--sp]];
        if(!slabAABB(ro,inv,nd.aabbMin.xyz,nd.aabbMax.xyz,tBest)) continue;
        if(nd.meta.y>0){ // leaf
            for(int i=0;i<nd.meta.y;i++){
                int ti=nd.meta.x+i; float t; vec3 b;
                if(hitTri(ro,rd,tris[ti],t,b) && t<tBest){
                    tBest=t; best=ti;
                    nrm=normalize(b.x*tris[ti].n0.xyz + b.y*tris[ti].n1.xyz + b.z*tris[ti].n2.xyz);
                    matIdx=tris[ti].mat.x;
                }
            }
        } else { stack[sp++]=nd.meta.x; stack[sp++]=nd.meta.x+1; }
    }
    return best;
}

bool occluded(vec3 ro, vec3 rd, float maxT){
    vec3 inv=1.0/rd; int stack[64]; int sp=0; stack[sp++]=0;
    while(sp>0){
        GpuNode nd=nodes[stack[--sp]];
        if(!slabAABB(ro,inv,nd.aabbMin.xyz,nd.aabbMax.xyz,maxT)) continue;
        if(nd.meta.y>0){
            for(int i=0;i<nd.meta.y;i++){ int ti=nd.meta.x+i; float t; vec3 b;
                if(hitTri(ro,rd,tris[ti],t,b) && t<maxT) return true; }
        } else { stack[sp++]=nd.meta.x; stack[sp++]=nd.meta.x+1; }
    }
    return false;
}

vec3 cosineHemisphere(vec3 n, inout uint s){
    float u1=rnd(s), u2=rnd(s);
    float r=sqrt(u1), phi=2.0*PI*u2;
    vec3 t = normalize(abs(n.x)>0.9 ? vec3(0,1,0) : vec3(1,0,0));
    vec3 b = normalize(cross(n,t)); t=cross(b,n);
    return normalize(t*r*cos(phi) + b*r*sin(phi) + n*sqrt(max(0.0,1.0-u1)));
}

vec3 rayDir(vec2 uv){ vec4 w=uInvViewProj*vec4(uv*2.0-1.0,1.0,1.0); return normalize(w.xyz/w.w - uCamPos); }

vec3 trace(vec3 ro, vec3 rd, inout uint s){
    vec3 L=vec3(0.0), thr=vec3(1.0);
    for(int bounce=0; bounce<uMaxBounces; ++bounce){
        float t; vec3 n; uint mi;
        int hit=closestHit(ro,rd,t,n,mi);
        if(hit<0){ L += thr * sampleEnv(rd); break; }
        vec3 albedo = mats[mi].baseColor.rgb;
        vec3 p = ro + rd*t;
        if(dot(n,rd)>0.0) n=-n;               // face forward
        // NEE: direct sun (directional light travels along uSunDir)
        vec3 toSun = normalize(-uSunDir);
        float ndl = max(dot(n,toSun),0.0);
        if(ndl>0.0 && !occluded(p+n*1e-3, toSun, INF)){
            L += thr * (albedo/PI) * uSunColor * uSunIntensity * ndl;
        }
        // Indirect: cosine-weighted diffuse bounce. For cosine pdf the
        // albedo/PI * cos / pdf simplifies to albedo.
        thr *= albedo;
        rd = cosineHemisphere(n,s);
        ro = p + n*1e-3;
        // Russian roulette after 3 bounces
        if(bounce>=3){ float q=max(thr.r,max(thr.g,thr.b)); if(rnd(s)>q) break; thr/=max(q,1e-4); }
    }
    return L;
}

void main(){
    ivec2 px=ivec2(gl_GlobalInvocationID.xy);
    if(px.x>=int(uResolution.x)||px.y>=int(uResolution.y)) return;
    uint seed = uint(px.x)*1973u + uint(px.y)*9277u + uint(uSampleIndex)*26699u + 1u;
    vec3 acc=vec3(0.0);
    for(int i=0;i<uSppPerFrame;i++){
        vec2 jitter=vec2(rnd(seed),rnd(seed));
        vec2 uv=(vec2(px)+jitter)/uResolution;
        acc += trace(uCamPos, rayDir(uv), seed);
    }
    acc /= float(uSppPerFrame);
    vec3 prev=(uSampleIndex==0)?vec3(0.0):imageLoad(uAccum,px).rgb;
    imageStore(uAccum, px, vec4(prev+acc, 1.0));
}
```

- [ ] **Step 2: Set the new uniforms in the stage**

In `PathTracerStage::Execute`, after binding, set: `uMaxBounces = frame.renderSettings.pathTracerMaxBounces`, `uSppPerFrame = frame.renderSettings.pathTracerSpp`, sun from `resources.Get<RenderSceneView>().sunLight` (`uSunDir`/`uSunColor`/`uSunIntensity`), and bind `env_` to a texture unit with `env_->Bind(0); shader_->SetInt("uEnvMap", 0);`. Note: `uAccum` uses image binding 0 and `uEnvMap` uses texture unit 0 — these are separate GL namespaces, no conflict.

- [ ] **Step 3: Build**

Run: `cmake --build build --config Debug`
Expected: shader compiles (watch the console for GLSL compile logs from `Shader`).

- [ ] **Step 4: White-furnace verification**

Temporarily set the Cornell sun `intensity` to 0 and point the camera at a wall; force `pathTracerEnabled=true`. Temporarily use a constant-white environment (either an all-white HDR or hardcode `sampleEnv` to return `vec3(1.0)`).
Expected: every diffuse surface converges toward its own albedo (e.g. the white wall → ~0.73, red wall → ~(0.65,0.05,0.05)) with no runaway brightening across bounces. Revert the temporary env hack.

- [ ] **Step 5: Cornell-convergence verification**

Restore the sun; freeze the camera looking into the box.
Expected: as `sampleCount` climbs the reference (viewable once Task 7 lands, or via RenderDoc on `uAccum`) becomes noise-free and shows **color bleeding** — red/green tint on the white boxes near the side walls — which the realtime image lacks.

- [ ] **Step 6: Commit**

```bash
git add shader/pathtracer/pathtrace.comp src/pipeline/stages/PathTracerStage.cpp
git commit -m "feat: diffuse multi-bounce reference path tracer (BVH + NEE + env)"
```

---

## Task 7: `ComparisonStage` — error metrics + view modes

**Files:**
- Create: `src/pipeline/stages/ComparisonStage.h`, `.cpp`, `shader/comparison/error.comp`, `shader/comparison/composite.frag`
- Modify: `src/pipeline/RenderPipeline.h/.cpp` (register + expose readouts)
- Test: runtime — split view, error heatmap, RMSE/MAPE numbers behave sensibly.

**Interfaces:**
- Consumes: `ReferenceOutputs` (Task 5), `TAAOutputs`/`LightingOutputs` (existing), `FrameContext.debugSettings.compareView/errorScale`.
- Produces:
```cpp
struct ComparisonReadout { double rmse = 0.0; double mape = 0.0; uint32_t sampleCount = 0; bool valid = false; };
class ComparisonStage : public IPipelineStage {
public:
    const char* GetName() const override { return "ComparisonStage"; }
    void Init(int,int) override; void Resize(int,int) override;
    void Execute(PipelineResources&, const FrameContext&) override;
    const ComparisonReadout& Readout() const;
};
```

- [ ] **Step 1: Error compute shader**

`shader/comparison/error.comp` — reads realtime + reference (mean), writes a per-pixel error texture (rgb = abs diff for heatmap, a = squared luminance error for reduction):

```glsl
#version 460 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(binding = 0) uniform sampler2D uRealtime;   // resolved HDR (pre-tonemap)
layout(binding = 1) uniform sampler2D uReference;   // accumulation buffer (sum)
layout(rgba32f, binding = 2) uniform image2D uError;
uniform vec2 uResolution;
uniform float uInvSampleCount; // 1/sampleCount to turn the reference sum into a mean

float luma(vec3 c){ return dot(c, vec3(0.2126,0.7152,0.0722)); }

void main(){
    ivec2 px=ivec2(gl_GlobalInvocationID.xy);
    if(px.x>=int(uResolution.x)||px.y>=int(uResolution.y)) return;
    vec3 a = texelFetch(uRealtime, px, 0).rgb;
    vec3 b = texelFetch(uReference, px, 0).rgb * uInvSampleCount;
    vec3 d = abs(a-b);
    float se = (luma(a)-luma(b)); se*=se;
    imageStore(uError, px, vec4(d, se));
}
```

- [ ] **Step 2: Composite fragment shader (view modes)**

`shader/comparison/composite.frag` — fullscreen; chooses what to draw. Reuse the existing fullscreen-triangle vertex setup the other post stages use (a `dummyVAO_` with `gl_VertexID`); copy that pattern from `PostProcessStage`/`TAAStage`.

```glsl
#version 460 core
out vec4 FragColor;
uniform sampler2D uRealtime;   // resolved HDR
uniform sampler2D uReference;  // accumulation sum
uniform sampler2D uError;      // abs diff in rgb
uniform vec2  uResolution;
uniform float uInvSampleCount;
uniform int   uView;           // 0 realtime,1 reference,2 split,3 heatmap
uniform float uErrorScale;

vec3 tonemap(vec3 c){ c*=1.0; return c/(c+vec3(1.0)); } // simple Reinhard for display
vec3 heat(float x){ // blue→green→red ramp
    x=clamp(x,0.0,1.0);
    return clamp(vec3(1.5-abs(4.0*x-3.0), 1.5-abs(4.0*x-2.0), 1.5-abs(4.0*x-1.0)),0.0,1.0);
}
void main(){
    vec2 uv=gl_FragCoord.xy/uResolution;
    ivec2 px=ivec2(gl_FragCoord.xy);
    vec3 rt=texelFetch(uRealtime,px,0).rgb;
    vec3 rf=texelFetch(uReference,px,0).rgb*uInvSampleCount;
    vec3 outc;
    if(uView==0) outc=tonemap(rt);
    else if(uView==1) outc=tonemap(rf);
    else if(uView==2) outc=(uv.x<0.5)?tonemap(rt):tonemap(rf);
    else { float e=length(texelFetch(uError,px,0).rgb)*uErrorScale; outc=heat(e); }
    FragColor=vec4(outc,1.0);
}
```

- [ ] **Step 3: Stage implementation**

- `Init`: create `errorShader_ = Shader("comparison/error.comp")`, `compositeShader_ = Shader(fullscreenVert, "comparison/composite.frag")` (use the same fullscreen vertex shader the other post stages use — find its path in `PostProcessStage`), `errorTex_ = Texture::Create2D(w,h, GL_RGBA32F, GL_RGBA, GL_FLOAT)`, a `dummyVAO_`.
- `Execute`:
  1. If `!resources.Has<ReferenceOutputs>()` (PT disabled) → bind default framebuffer and just blit/draw the realtime image (view 0), then return — keeps the screen correct when PT is off. (The realtime image is already on the backbuffer from PostProcessStage; simplest is to `return` and let PostProcess's output stand. Choose: when PT is off, **return immediately**.)
  2. Get `const auto& ref = resources.Get<ReferenceOutputs>();` and the realtime resolved HDR: `resources.Has<TAAOutputs>() ? Get<TAAOutputs>().resolvedHdr : Get<LightingOutputs>().hdrColor`.
  3. Run `error.comp`: bind realtime→unit 0, reference→unit 1, `errorTex_->BindImage(2, GL_WRITE_ONLY, GL_RGBA32F)`, set `uInvSampleCount = 1.0/max(ref.sampleCount,1)`; dispatch `(w+7)/8,(h+7)/8`.
  4. Throttled readback for metrics: when `ref.sampleCount` is 1 or a power of two (cheap heuristic to avoid a per-frame stall), `glGetTextureImage` the `errorTex_` into a CPU `std::vector<float>`, sum `.a` (squared error) and accumulate relative error for MAPE, compute `rmse_ = sqrt(sumSE/pixelCount)`, store into `readout_`. Set `readout_.valid=true`, `readout_.sampleCount=ref.sampleCount`.
  5. Composite to backbuffer: `Framebuffer::BindDefault()` (match how PostProcess binds the default FBO), use `compositeShader_`, bind realtime/reference/error textures + uniforms (`uView=(int)frame.debugSettings.compareView`, `uErrorScale`, `uInvSampleCount`), draw the fullscreen triangle.
- `Readout()` returns `readout_`.

For MAPE use `mean( |a-b| / (|b| + eps) )` on luminance, computed in the same CPU readback loop.

- [ ] **Step 4: Register + expose readout**

In `RenderPipeline::BuildStages`, push `ComparisonStage` **after** `PathTracerStage`. Keep a raw pointer to it (or query the vector) so `RenderPipeline` can expose:

```cpp
const ComparisonReadout& GetComparisonReadout() const;
```

Add the include. (Comparison must run after PathTracer so `ReferenceOutputs` exists that frame.)

- [ ] **Step 5: Configure, build, run**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` && `cmake --build build --config Debug` && `build\Debug\HuanGL.exe`
With `pathTracerEnabled` temporarily `true` on the Cornell scene and `compareView` temporarily set in code to `Split`:
Expected: left half realtime (flat corners), right half reference (color bleeding); switching `compareView` to `ErrorHeatmap` shows warm colors where indirect light is missing. Revert temporary defaults.

- [ ] **Step 6: Commit**

```bash
git add src/pipeline/stages/ComparisonStage.h src/pipeline/stages/ComparisonStage.cpp shader/comparison/error.comp shader/comparison/composite.frag src/pipeline/RenderPipeline.h src/pipeline/RenderPipeline.cpp
git commit -m "feat: ComparisonStage with error metrics and view modes"
```

---

## Task 8: DebugUI "GI Comparison" panel

**Files:**
- Modify: `src/ui/DebugUI.cpp`, `src/app/ApplicationState.h`, `src/core/App.cpp`
- Test: runtime — toggles and readouts drive the stages.

**Interfaces:**
- Consumes: `RenderSettings.pathTracer*`, `DebugSettings.compareView/errorScale` (Task 5), `ComparisonReadout` (Task 7).
- Produces: `ApplicationState::comparisonReadout` field populated by `App` each frame.

- [ ] **Step 1: Add readout field to `ApplicationState`**

In `src/app/ApplicationState.h` add `#include` for the comparison readout type (or forward-declare a small mirror) and:

```cpp
    ComparisonReadout comparisonReadout; // populated from RenderPipeline each frame
```

If including the stage header is heavy, define `ComparisonReadout` in a small shared header (e.g. move it to `src/renderer/FrameContext.h` or a new `src/pipeline/ComparisonReadout.h`) and include that from both the stage and `ApplicationState.h`. Prefer the small dedicated header.

- [ ] **Step 2: Copy readout in `App`**

In `src/core/App.cpp` after `pipeline_.Execute(...)`:

```cpp
state_.comparisonReadout = pipeline_.GetComparisonReadout();
```

(Mirrors the existing `state_.stageTimings = pipeline_.GetStageTimings();` line — find and place beside it.)

- [ ] **Step 3: Panel in DebugUI**

In `src/ui/DebugUI.cpp` `Draw`, add a collapsing header (match the existing ImGui style in the file):

```cpp
if (ImGui::CollapsingHeader("GI Comparison")) {
    auto& rs = state.renderSettings;
    auto& ds = state.debugSettings;
    ImGui::Checkbox("Path tracer (reference)", &rs.pathTracerEnabled);
    ImGui::SliderInt("SPP / frame", &rs.pathTracerSpp, 1, 8);
    ImGui::SliderInt("Max bounces", &rs.pathTracerMaxBounces, 1, 8);
    const char* views[] = {"Realtime","Reference","Split","Error heatmap"};
    int v = (int)ds.compareView;
    if (ImGui::Combo("View", &v, views, 4)) ds.compareView = (DebugSettings::CompareView)v;
    ImGui::SliderFloat("Error scale", &ds.errorScale, 0.01f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    const auto& r = state.comparisonReadout;
    if (r.valid) {
        ImGui::Text("Samples: %u", r.sampleCount);
        ImGui::Text("RMSE: %.5f", r.rmse);
        ImGui::Text("MAPE: %.2f%%", r.mape * 100.0);
    } else {
        ImGui::TextDisabled("enable path tracer to measure");
    }
}
```

- [ ] **Step 4: Configure, build, run**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` && `cmake --build build --config Debug` && `build\Debug\HuanGL.exe`
Expected: the panel toggles the path tracer live; on Cornell, enabling it + freezing the camera grows Samples and stabilizes RMSE; Split/Heatmap views switch correctly; "Error scale" changes heatmap intensity.

- [ ] **Step 5: Commit**

```bash
git add src/ui/DebugUI.cpp src/app/ApplicationState.h src/core/App.cpp src/pipeline/ComparisonReadout.h
git commit -m "feat: GI Comparison debug panel wiring path tracer + metrics"
```

---

## Task 9: End-to-end verification, cleanup, and docs

**Files:**
- Modify: temporary printf/log removals across Tasks 4–5; `docs/architecture.md`
- Test: the full verification matrix from the spec.

- [ ] **Step 1: Remove temporary instrumentation**

Remove the `std::printf` count logs added in Task 4 Step 3 and Task 5 Step 5, and any temporary default-`true`/forced-view edits. Confirm `pathTracerEnabled` defaults to `false` and `compareView` defaults to `Realtime`.

- [ ] **Step 2: Run the spec verification matrix**

Run `build\Debug\HuanGL.exe` and confirm:
1. **Zero-cost when off:** with the panel's path tracer off, the image and the GPU Timing panel are unchanged from before this work (PathTracer/Comparison rows ~0).
2. **Convergence:** Cornell + freeze camera → Samples climb, reference (Reference view) becomes noise-free.
3. **White-furnace:** (temporarily) sun off + uniform env → surfaces sit at their albedo; revert.
4. **Error reveals GI:** Realtime vs Reference split shows missing bounce; Error heatmap is warm in the corners.
5. **Reset:** moving the camera resets Samples to 1; freezing re-converges.
6. **No regressions:** TestScene + model scenes, all debug views (`0`–`7`), tone-map cycling (`T`), Bloom, TAA, and window resize behave as before; resize mid-accumulation does not crash and resets the reference.

- [ ] **Step 3: Update the roadmap**

In `docs/architecture.md`, add a short "GI Evaluation Framework" entry to the roadmap table between Phase 4.6 and Phase 5, and a brief paragraph (goal / deliverables / depends-on / risk) in the same style as the existing phase sections, noting that Phases 5/6/8 now validate against this framework's reference rather than only against each other. Link the spec: `docs/superpowers/specs/2026-06-20-huangl-gi-evaluation-framework-design.md`.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "docs: record GI evaluation framework in roadmap; remove temp instrumentation"
```

---

## Self-Review Notes

- **Spec coverage:** PT (Tasks 5–6), error metrics (Task 7), comparison harness/UI (Tasks 7–8), Cornell scene (Task 2), CPU geometry retention (Task 1), BVH (Tasks 3–4), white-furnace + Cornell + energy + reset verification (Tasks 6, 9). Non-goals (textured albedo, glossy indirect, FLIP, SAH) are deliberately absent.
- **Deviation from spec wording:** CPU geometry lives on `Mesh` (not only `MeshLoader::LoadResult`) because `PathTracerScene` consumes `Renderable->mesh`; this covers both primitive and Assimp build paths uniformly.
- **Type consistency:** `ReferenceOutputs{hdr,sampleCount}`, `ComparisonReadout{rmse,mape,sampleCount,valid}`, SSBO bindings 3/4/5, and the `GpuTri/GpuNode/GpuMaterial` ↔ GLSL struct mirrors are used identically across Tasks 4–8.
- **Known soft spot:** v1 reference is diffuse-only; glossy/metallic surfaces in non-Cornell scenes won't match — acceptable and documented (validation scene is Cornell).
