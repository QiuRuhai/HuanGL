# Phase 4.5: GI Foundations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the correctness and measurement gaps that every GI technique depends on — correct normals under non-uniform scale, sRGB-aware texture caching, and per-stage GPU timing in the debug UI.

**Architecture:** A new `GpuProfiler` (GL_TIMESTAMP ring buffer) brackets each stage in `RenderPipeline::Execute` with zero changes to the stages themselves; results flow `RenderPipeline → ApplicationState → DebugUI`. Two self-contained correctness fixes (normal matrix in `gbuffer.vert`, sRGB cache key in `ResourceManager` + routing `MeshLoader` through it). Docs and the teaching-comment convention are updated last.

**Tech Stack:** OpenGL 4.6 Core (DSA), C++17, GLAD2, Dear ImGui. No unit-test framework — verification is **compile clean + visual runtime check** (the established project practice).

**Spec:** `docs/superpowers/specs/2026-06-01-huangl-gi-foundations-design.md`

**Conventions to honor:**
- `namespace HuanGL`, trailing-underscore members, DSA calls (`glCreateQueries`, `glGetQueryObjectui64v`).
- This is a learning project: **teaching comments explaining WHY/algorithm are encouraged** (this plan adds that convention to AGENTS.md in Task 6).
- After adding a new `.cpp`, **rerun CMake configure** (project uses `GLOB_RECURSE`).
- Build: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build --config Debug`.
- Run: `.\build\Debug\HuanGL.exe`.

---

## File Structure

| File | Responsibility | Task |
|------|----------------|------|
| `src/renderer/GpuProfiler.h` (create) | Public `StageTiming` + `GpuProfiler` interface | 1 |
| `src/renderer/GpuProfiler.cpp` (create) | Ring-buffered GL_TIMESTAMP timing | 1 |
| `src/pipeline/RenderPipeline.h` (modify) | Own profiler, expose `GetStageTimings()` | 2 |
| `src/pipeline/RenderPipeline.cpp` (modify) | Bracket each stage with Begin/EndStage | 2 |
| `src/app/ApplicationState.h` (modify) | Carry `stageTimings` for the UI | 3 |
| `src/core/App.cpp` (modify) | Copy timings after `Execute` | 3 |
| `src/ui/DebugUI.cpp` (modify) | "GPU Timing" table | 3 |
| `shader/gbuffer/gbuffer.vert` (modify) | Inverse-transpose normal matrix | 4 |
| `src/resource/ResourceManager.h/cpp` (modify) | sRGB-aware `LoadTexture` cache | 5 |
| `src/resource/MeshLoader.cpp` (modify) | Route external textures through cache | 5 |
| `docs/architecture.md` (modify) | Roadmap 4.5/4.6, trim Known Limitations | 6 |
| `AGENTS.md` (modify) | Roadmap row, teaching-comment convention | 6 |

Tasks 1→2→3 are sequential (profiler must exist before integration before display). Tasks 4, 5, 6 are independent of each other and of 1-3.

---

### Task 1: Create GpuProfiler

**Files:**
- Create: `src/renderer/GpuProfiler.h`
- Create: `src/renderer/GpuProfiler.cpp`

- [ ] **Step 1: Create `src/renderer/GpuProfiler.h`**

```cpp
#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>

namespace HuanGL {

struct StageTiming {
    std::string name;
    double ms = 0.0;
};

// Per-stage GPU timing using GL_TIMESTAMP query pairs, ring-buffered across
// kFrameDepth frames. We read back a slot only when it is kFrameDepth frames
// old, so the results are already available and glGetQueryObject never stalls
// the GPU. Assumes a stable stage list frame-to-frame (true for the fixed
// RenderPipeline); the query objects are reused every frame.
class GpuProfiler {
public:
    GpuProfiler() = default;
    ~GpuProfiler();
    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    void BeginFrame();
    void BeginStage(const char* name);
    void EndStage();
    void EndFrame();

    // Timings for the most recently resolved frame. Empty until the ring has
    // filled (first kFrameDepth frames).
    const std::vector<StageTiming>& GetResults() const { return results_; }

private:
    static constexpr int kFrameDepth = 3;

    struct StageQuery {
        std::string name;
        GLuint start = 0;
        GLuint end   = 0;
    };
    struct FrameSlot {
        std::vector<StageQuery> stages;
        bool pending = false;
    };

    void Resolve(FrameSlot& slot);

    FrameSlot frames_[kFrameDepth];
    FrameSlot* current_ = nullptr;
    int frameCount_ = 0;
    int stageCursor_ = 0;
    std::vector<StageTiming> results_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/renderer/GpuProfiler.cpp`**

```cpp
#include "GpuProfiler.h"

namespace HuanGL {

GpuProfiler::~GpuProfiler() {
    for (auto& slot : frames_) {
        for (auto& s : slot.stages) {
            if (s.start) glDeleteQueries(1, &s.start);
            if (s.end)   glDeleteQueries(1, &s.end);
        }
    }
}

void GpuProfiler::BeginFrame() {
    current_ = &frames_[frameCount_ % kFrameDepth];
    // This slot, if pending, holds queries issued kFrameDepth frames ago.
    // Those timestamps are guaranteed ready, so this readback never stalls.
    if (current_->pending) {
        Resolve(*current_);
        current_->pending = false;
    }
    stageCursor_ = 0;
}

void GpuProfiler::BeginStage(const char* name) {
    if (!current_) return;
    if (stageCursor_ >= static_cast<int>(current_->stages.size())) {
        StageQuery q;
        glCreateQueries(GL_TIMESTAMP, 1, &q.start);
        glCreateQueries(GL_TIMESTAMP, 1, &q.end);
        current_->stages.push_back(q);
    }
    StageQuery& q = current_->stages[stageCursor_];
    q.name = name;
    glQueryCounter(q.start, GL_TIMESTAMP);
}

void GpuProfiler::EndStage() {
    if (!current_ || stageCursor_ >= static_cast<int>(current_->stages.size()))
        return;
    glQueryCounter(current_->stages[stageCursor_].end, GL_TIMESTAMP);
    ++stageCursor_;
}

void GpuProfiler::EndFrame() {
    if (!current_) return;
    current_->pending = (stageCursor_ > 0);
    current_ = nullptr;
    ++frameCount_;
}

void GpuProfiler::Resolve(FrameSlot& slot) {
    results_.clear();
    results_.reserve(slot.stages.size());
    for (auto& s : slot.stages) {
        GLuint64 start = 0, end = 0;
        glGetQueryObjectui64v(s.start, GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(s.end,   GL_QUERY_RESULT, &end);
        double ms = (end >= start) ? static_cast<double>(end - start) / 1.0e6
                                   : 0.0;
        results_.push_back({s.name, ms});
    }
}

} // namespace HuanGL
```

- [ ] **Step 3: Reconfigure CMake (new .cpp picked up by GLOB_RECURSE) and build**

Run:
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```
Expected: configures and compiles clean. `GpuProfiler.cpp` appears in the build output. The profiler is not yet referenced by anything, so behavior is unchanged.

- [ ] **Step 4: Commit**

```powershell
git add src/renderer/GpuProfiler.h src/renderer/GpuProfiler.cpp
git commit -m "feat: add GpuProfiler (GL_TIMESTAMP per-stage GPU timing)"
```

---

### Task 2: Integrate GpuProfiler into RenderPipeline

**Files:**
- Modify: `src/pipeline/RenderPipeline.h`
- Modify: `src/pipeline/RenderPipeline.cpp`

- [ ] **Step 1: Add the profiler member and accessor to `RenderPipeline.h`**

Add `#include "../renderer/GpuProfiler.h"` with the other renderer includes, then add the member and accessor. The full file after editing:

```cpp
#pragma once
#include "IPipelineStage.h"
#include "PipelineResources.h"
#include "../renderer/FrameContext.h"
#include "../renderer/RenderSceneView.h"
#include "../renderer/UniformBuffer.h"
#include "../renderer/GpuProfiler.h"
#include <memory>
#include <string>
#include <vector>

namespace HuanGL {

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void InvalidateHistory();
    void Execute(const RenderSceneView& scene, const FrameContext& frame);

    // Per-stage GPU timings from the most recently resolved frame.
    const std::vector<StageTiming>& GetStageTimings() const {
        return profiler_.GetResults();
    }

private:
    void BuildStages(const std::string& hdrPath);
    void UpdateUniformBuffers(const RenderSceneView& scene, const FrameContext& frame);

    std::vector<std::unique_ptr<IPipelineStage>> stages_;
    PipelineResources resources_;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;

    GpuProfiler profiler_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Bracket each stage in `RenderPipeline.cpp::Execute`**

Replace the existing `Execute` body. The full method after editing:

```cpp
void RenderPipeline::Execute(const RenderSceneView& scene,
                              const FrameContext& frame) {
    resources_.Clear();
    resources_.Set(scene);
    UpdateUniformBuffers(scene, frame);

    profiler_.BeginFrame();
    for (auto& stage : stages_) {
        Renderer::PushDebugGroup(stage->GetName());
        profiler_.BeginStage(stage->GetName());
        stage->Execute(resources_, frame);
        profiler_.EndStage();
        Renderer::PopDebugGroup();
    }
    profiler_.EndFrame();
}
```

- [ ] **Step 3: Build**

Run:
```powershell
cmake --build build --config Debug
```
Expected: compiles clean. Runtime behavior visually unchanged (timings collected but not yet displayed).

- [ ] **Step 4: Commit**

```powershell
git add src/pipeline/RenderPipeline.h src/pipeline/RenderPipeline.cpp
git commit -m "feat: time each pipeline stage via GpuProfiler"
```

---

### Task 3: Plumb timings to ApplicationState and display in DebugUI

**Files:**
- Modify: `src/app/ApplicationState.h`
- Modify: `src/core/App.cpp`
- Modify: `src/ui/DebugUI.cpp`

- [ ] **Step 1: Add `stageTimings` to `ApplicationState.h`**

Add the include and the member. The full file after editing:

```cpp
#pragma once
#include "SceneRegistry.h"
#include "../core/Camera.h"
#include "../renderer/FrameContext.h"
#include "../renderer/GpuProfiler.h"
#include <vector>

namespace HuanGL {

struct FrameStats {
    float deltaTime = 0.0f;
    float frameTimeMs = 0.0f;
    float fps = 0.0f;
};

struct ApplicationState {
    bool running = true;
    bool cameraActive = false;
    SceneRegistry sceneRegistry;
    Camera camera {60.0f, 0.1f, 100.0f};
    RenderSettings renderSettings;
    DebugSettings debugSettings;
    FrameStats frameStats;
    std::vector<StageTiming> stageTimings;
};

} // namespace HuanGL
```

- [ ] **Step 2: Copy timings after `Execute` in `App.cpp::Render`**

In `src/core/App.cpp`, the `Render` method currently ends with:

```cpp
    pipeline_->Execute(sceneView, frame);
    StorePreviousCameraState(frame.camera);
```

Change it to copy the timings into state (so the UI, which reads `ApplicationState`, can display them):

```cpp
    pipeline_->Execute(sceneView, frame);
    state_.stageTimings = pipeline_->GetStageTimings();
    StorePreviousCameraState(frame.camera);
```

- [ ] **Step 3: Add a "GPU Timing" section to `DebugUI.cpp`**

In `src/ui/DebugUI.cpp`, the `Stats` collapsing header currently reads:

```cpp
    if (ImGui::CollapsingHeader("Stats")) {
        ImGui::Text("FPS: %.1f", state.frameStats.fps);
        ImGui::Text("Frame: %.2f ms", state.frameStats.frameTimeMs);
    }
```

Replace it with the stats block plus a GPU timing table:

```cpp
    if (ImGui::CollapsingHeader("Stats")) {
        ImGui::Text("FPS: %.1f", state.frameStats.fps);
        ImGui::Text("Frame: %.2f ms", state.frameStats.frameTimeMs);
    }

    if (ImGui::CollapsingHeader("GPU Timing")) {
        if (state.stageTimings.empty()) {
            ImGui::TextDisabled("measuring...");
        } else {
            double total = 0.0;
            if (ImGui::BeginTable("gpu_timing", 2,
                                  ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Stage");
                ImGui::TableSetupColumn("ms");
                ImGui::TableHeadersRow();
                for (const auto& t : state.stageTimings) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(t.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", t.ms);
                    total += t.ms;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Total GPU");
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", total);
                ImGui::EndTable();
            }
        }
    }
```

- [ ] **Step 4: Build**

Run:
```powershell
cmake --build build --config Debug
```
Expected: compiles clean.

- [ ] **Step 5: Run and verify GPU timing displays**

Run:
```powershell
.\build\Debug\HuanGL.exe
```
Verify:
1. Open the "HuanGL Debug" window → expand "GPU Timing".
2. After a second or two it shows rows: `ShadowStage`, `GBufferStage`, `LightingStage`, `BloomStage`, `TAAStage`, `PostProcessStage`, each with a nonzero ms, plus a `Total GPU` row.
3. Toggle Bloom off (Techniques → Bloom) — `BloomStage` ms drops toward ~0 (the stage early-returns).
4. Switch scenes with `N` — totals change (Sponza > TestScene).
5. No crash, no GL errors in the console.

- [ ] **Step 6: Commit**

```powershell
git add src/app/ApplicationState.h src/core/App.cpp src/ui/DebugUI.cpp
git commit -m "feat: display per-stage GPU timing in DebugUI"
```

---

### Task 4: Fix normal matrix in gbuffer.vert

**Files:**
- Modify: `shader/gbuffer/gbuffer.vert`

- [ ] **Step 1: Replace `mat3(model)` with the inverse-transpose**

In `shader/gbuffer/gbuffer.vert`, replace this block:

```glsl
    // Note: mat3(model) is only correct for orthogonal transforms (rotation +
    // uniform scale). For non-uniform scale, transpose(inverse(mat3(model)))
    // would be needed. Pre-existing limitation across the codebase.
    mat3 normalMat = mat3(model);
```

with:

```glsl
    // Normals transform by the inverse-transpose of the model matrix, which
    // stays correct under non-uniform scale (plain mat3(model) skews normals
    // when scale is anisotropic). Computed per-vertex here for learning
    // clarity; production code precomputes a normalMatrix on the CPU and
    // uploads it once per draw instead of inverting per vertex.
    mat3 normalMat = transpose(inverse(mat3(model)));
```

- [ ] **Step 2: Build**

Run:
```powershell
cmake --build build --config Debug
```
Expected: compiles clean (shaders are loaded at runtime, so this just confirms nothing else broke).

- [ ] **Step 3: Run and verify normals are correct under non-uniform scale**

Run:
```powershell
.\build\Debug\HuanGL.exe
```
Verify:
1. Default scenes still render correctly (uniform scale → unchanged result).
2. Temporary check: in the DebugUI "Scene" header, expand an entity and set a non-uniform `Scale` (e.g. `2, 1, 1`). Switch Debug Mode to "Normal". The normals should remain perpendicular to surfaces (smooth hemisphere gradient on spheres), **not** skew/shift with the stretch. Reset the scale afterward.

- [ ] **Step 4: Commit**

```powershell
git add shader/gbuffer/gbuffer.vert
git commit -m "fix: use inverse-transpose normal matrix in gbuffer.vert"
```

---

### Task 5: sRGB-aware texture cache

**Files:**
- Modify: `src/resource/ResourceManager.h`
- Modify: `src/resource/ResourceManager.cpp`
- Modify: `src/resource/MeshLoader.cpp`

**Why MeshLoader changes too:** `ResourceManager::Load<Texture>` is currently dead code — nothing calls it. `MeshLoader` loads external textures with `Texture::Load2D` directly, bypassing the cache, which is exactly why linear textures shared between materials get loaded more than once. Fixing the cache key alone does nothing unless `MeshLoader` actually routes through the cache.

- [ ] **Step 1: Replace the texture entry in `ResourceManager.h`**

Replace the dead `Load<Texture>` path with an sRGB-aware `LoadTexture`. The full file after editing:

```cpp
#pragma once
#include <string>
#include <unordered_map>
#include <memory>

namespace HuanGL {

class Texture;

class ResourceManager {
public:
    static void Init();
    static void Shutdown();

    // Mesh cache (keyed by path).
    template<typename T>
    static std::shared_ptr<T> Load(const std::string& path);

    // Texture cache keyed by (path, sRGB) so the same file can be cached both
    // as color (sRGB) and as linear data (normal/roughness/metallic) without
    // collisions, and shared linear textures are loaded only once.
    static std::shared_ptr<Texture> LoadTexture(const std::string& path, bool sRGB);

    static void GC();

private:
    template<typename T>
    static std::string MakeKey(const std::string& path) {
        return std::string(typeid(T).name()) + "|" + path;
    }
    struct Entry { std::weak_ptr<void> ptr; };
    static std::unordered_map<std::string, Entry> cache_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Replace the texture load in `ResourceManager.cpp`**

Replace the `Load<Texture>` specialization with `LoadTexture`. Keep `Load<Mesh>` exactly as-is. The full file after editing:

```cpp
#include "ResourceManager.h"
#include "MeshLoader.h"
#include "../renderer/Texture.h"

namespace HuanGL {

std::unordered_map<std::string, ResourceManager::Entry> ResourceManager::cache_;

void ResourceManager::Init() {}
void ResourceManager::Shutdown() { GC(); }

void ResourceManager::GC() {
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (it->second.ptr.expired()) it = cache_.erase(it);
        else ++it;
    }
}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& path,
                                                      bool sRGB) {
    std::string key = "Texture|" + path + (sRGB ? "|srgb" : "|linear");
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (auto p = it->second.ptr.lock())
            return std::static_pointer_cast<Texture>(p);
    }
    auto tex = Texture::Load2D(path, sRGB);
    cache_[key] = {tex};
    return tex;
}

template<>
std::shared_ptr<Mesh> ResourceManager::Load<Mesh>(const std::string& path) {
    std::string key = MakeKey<Mesh>(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (auto p = it->second.ptr.lock())
            return std::static_pointer_cast<Mesh>(p);
    }
    auto result = MeshLoader::Load(path);
    cache_[key] = {result.mesh};
    return result.mesh;
}

} // namespace HuanGL
```

- [ ] **Step 3: Route MeshLoader external textures through the cache**

In `src/resource/MeshLoader.cpp`, add the include near the top (after the existing `#include "../renderer/Texture.h"`):

```cpp
#include "ResourceManager.h"
```

Then, in `LoadMaterialTexture`, replace the external-file load:

```cpp
    // External file on disk.
    std::string resolved = ResolvePath(modelDir, path);
    return Texture::Load2D(resolved, sRGB);
```

with the cached path:

```cpp
    // External file on disk — go through the cache so a texture shared by
    // several materials (common in Sponza) is loaded only once per sRGB mode.
    std::string resolved = ResolvePath(modelDir, path);
    return ResourceManager::LoadTexture(resolved, sRGB);
```

Leave the embedded-texture path (`Texture::Load2DFromMemory`) unchanged — `*N` indices have no stable cache key.

- [ ] **Step 4: Build**

Run:
```powershell
cmake --build build --config Debug
```
Expected: compiles clean.

- [ ] **Step 5: Run and verify**

Run:
```powershell
.\build\Debug\HuanGL.exe
```
Verify:
1. Switch to the Sponza scene (`N`) — it loads and renders the same as before (textures still correct: albedo in color, normals/roughness linear).
2. No crash, no GL errors.
3. (Optional sanity) Sponza shares textures across many materials; with caching, repeated loads of the same `(path, sRGB)` now return the cached texture. Re-entering Sponza (cycle `N` around) should not re-decode already-cached textures.

- [ ] **Step 6: Commit**

```powershell
git add src/resource/ResourceManager.h src/resource/ResourceManager.cpp src/resource/MeshLoader.cpp
git commit -m "fix: sRGB-aware texture cache, route MeshLoader through it"
```

---

### Task 6: Update docs and the teaching-comment convention

**Files:**
- Modify: `docs/architecture.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Insert Phase 4.5 / 4.6 into the `docs/architecture.md` roadmap table**

In `docs/architecture.md`, the roadmap table row for Phase 4 currently reads:

```
| 4 | In Progress | Bloom, TAA, improved tone mapping |
| 5 | Planned | RSM |
```

Replace with:

```
| 4 | ✅ Complete | Bloom, TAA, improved tone mapping |
| 4.5 | In Progress | GI foundations: correctness fixes + per-stage GPU profiling |
| 4.6 | Planned | Scalability + showcase: frustum culling, draw batching, showcase scene |
| 5 | Planned | RSM |
```

- [ ] **Step 2: Add Phase 4.5 / 4.6 detail sections in `docs/architecture.md`**

In `docs/architecture.md`, immediately before the `### Phase 5 — Reflective Shadow Maps (RSM)` heading, insert:

```markdown
### Phase 4.5 — GI Foundations

**Goal.** Close the correctness and measurement gaps every GI technique
depends on, before adding indirect lighting.

**Deliverables.**
- `GpuProfiler` (`src/renderer/GpuProfiler.h/cpp`): per-stage GPU timing via
  ring-buffered `GL_TIMESTAMP` queries, surfaced in the DebugUI "GPU Timing"
  table. Enables the A/B technique comparison the project is built around.
- Correct normal matrix (`transpose(inverse(mat3(model)))`) in the GBuffer
  vertex shader, fixing lighting under non-uniform scale.
- sRGB-aware texture cache: `ResourceManager::LoadTexture(path, sRGB)` keyed
  by `(path, sRGB)`, with `MeshLoader` routed through it so shared linear
  textures load once.

**Depends on.** Phase 3.6 (stage pipeline — the profiler brackets stages).

**Out of scope.** Motion vectors / velocity buffer: all scenes are static, so
TAA's existing depth-reprojection is already correct; a velocity buffer is
deferred until animated content exists.

**Risk.** Low. Main pitfall is a profiler ring-buffer off-by-one stalling the
GPU; mitigated by only reading slots that are `kFrameDepth` frames old.

### Phase 4.6 — Scalability and Showcase

**Goal.** Make the renderer scale to large scenes and ship a portfolio-quality
demonstration scene before the heavier GI techniques land.

**Deliverables.**
- Frustum culling (AABB accumulation in `Mesh`, per-pass cull).
- Draw batching to reduce per-sub-mesh `glDrawElements` overhead.
- A committed showcase scene and a clear asset-loading story.

**Depends on.** Phase 4.5.

**Risk.** Culling correctness across shadow cascades (each cascade has its own
frustum); validate per-cascade before trusting the cull.
```

- [ ] **Step 3: Trim the fixed entries from Known Limitations in `docs/architecture.md`**

In the `## Known Limitations` section, delete entry #1 (`mat3(model)` normal matrix) and entry #5 (`ResourceManager` always loads textures as sRGB). Renumber the remaining entries so the list stays 1..N contiguous. In the entry that mentions no frustum culling / no mesh batching, append the sentence:

```
*Scheduled for Phase 4.6.*
```

to each of those two entries.

- [ ] **Step 4: Update the `AGENTS.md` roadmap and Phase tables**

In `AGENTS.md`, the "Planned Phases" table currently has:

```
| 3.6 | ✅ Complete | Modular pipeline architecture (IPipelineStage, PipelineResources) |
| 4 | In Progress | Bloom, TAA, improved tone mapping |
```

Replace with:

```
| 3.6 | ✅ Complete | Modular pipeline architecture (IPipelineStage, PipelineResources) |
| 4 | ✅ Complete | Bloom, TAA, improved tone mapping |
| 4.5 | In Progress | GI foundations: correctness fixes + per-stage GPU profiling |
| 4.6 | Planned | Scalability + showcase: culling, batching, showcase scene |
```

- [ ] **Step 5: Add the teaching-comment convention to `AGENTS.md`**

In `AGENTS.md`, in the "Key Technical Decisions" bullet list, add a new bullet after the modular-pipeline bullet (the one that mentions `IPipelineStage` / `src/pipeline/stages/`):

```
- This is a learning project: comments that explain WHY a technique works or the reasoning behind an algorithm are an asset. Prefer clear teaching comments over terse minimalism. (This supersedes any earlier implicit "minimal comments" assumption.)
```

- [ ] **Step 6: Add the design and plan to the AGENTS.md "Design Documents" list**

In `AGENTS.md`, in the "Design Documents" section, add:

```
- GI foundations design: `docs/superpowers/specs/2026-06-01-huangl-gi-foundations-design.md`
- GI foundations plan: `docs/superpowers/plans/2026-06-01-huangl-gi-foundations.md`
```

- [ ] **Step 7: Commit**

```powershell
git add docs/architecture.md AGENTS.md
git commit -m "docs: add Phase 4.5/4.6 roadmap, teaching-comment convention"
```

---

## Self-Review

**Spec coverage:**
- Goal 1 (normal matrix) → Task 4. ✓
- Goal 2 (sRGB cache) → Task 5. ✓
- Goal 3 (per-stage GPU timing) → Tasks 1-3. ✓
- Goal 4 (roadmap + convention + Known Limitations trim) → Task 6. ✓
- Spec component "GpuProfiler interface" matches Task 1 header verbatim (`BeginFrame/BeginStage/EndStage/EndFrame/GetResults`, `StageTiming{name,ms}`, `kFrameDepth=3`). ✓
- Spec "RenderPipeline integration" wrap order (PushDebugGroup → BeginStage → Execute → EndStage → PopDebugGroup) matches Task 2 Step 2. ✓
- Spec "DebugUI display" (table name→ms + total) matches Task 3 Step 3. ✓
- Spec data-flow (`RenderPipeline::GetStageTimings → App → ApplicationState → DebugUI`) matches Tasks 2-3. ✓
- Spec non-goal (no motion vectors) honored — no task adds a velocity buffer. ✓

**Note vs spec file list:** Task 5 additionally edits `MeshLoader.cpp`. The spec listed only `ResourceManager.h/cpp`, but the cache fix is inert without routing MeshLoader through it (the only texture-loading call site). This realizes the spec's stated *intent* ("shared linear textures load once").

**Type consistency:** `StageTiming` / `GpuProfiler` method names are identical across Tasks 1, 2, 3 and ApplicationState. `GetStageTimings()` (RenderPipeline) vs `GetResults()` (GpuProfiler) are distinct-by-design (pipeline delegates to profiler). `LoadTexture(path, sRGB)` signature identical in ResourceManager.h (Task 5.1), .cpp (5.2), and the MeshLoader call site (5.3). ✓

**Placeholder scan:** No TBD/TODO; every code step shows complete code. ✓
