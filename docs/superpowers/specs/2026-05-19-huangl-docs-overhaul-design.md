# Docs Overhaul: Align Existing Docs with Phase 2.5 and Establish a Durable Roadmap

## Context

HuanGL has progressed through Phase 1, Phase 2, and Phase 2.5, but the
project's documentation has not kept up.

- `README.md` still states "Phase 1 is complete" and "Phase 2 will introduce
  the render pipeline." Both claims are now historical.
- `AGENTS.md` lists only Phase 1 source files in its progress table and does
  not register the modules added by Phase 2 (`GBufferPass`, `ShadowPass`,
  `LightingPass`, `ResourceManager`, `Scene`/`TestScene`) or Phase 2.5
  (`PostProcessPass`, `ModelScene`, `Input::IsKeyJustPressed`, `Material`
  texture extraction, embedded-texture decoding).
- The Phase 2.5 implementation plan was written during execution to
  `C:\Users\<...>\.claude\plans\calm-fluttering-clarke.md` and never copied
  into `docs/superpowers/plans/` alongside the Phase 1 and Phase 2 plans.
  The version-controlled record of how Phase 2.5 was executed is therefore
  missing.
- The project has **no forward-looking planning document**. The only
  forward-looking statements live in two short bullet lists at the bottom of
  `README.md` and `AGENTS.md` ("Phase 3 ... Phase 8 ..."). There is no
  written articulation of what HuanGL's "done" state is, why phases are
  ordered the way they are, what each phase delivers, or which phases can
  proceed in parallel.

This spec produces four documents that together close those gaps:

1. **Phase 2.5 plan archival** — bring the existing plan into the
   repository under the conventional location.
2. **README.md** — refresh "Current Status" and "Planned Rendering Phases";
   add a "Running" section covering models on disk and key bindings.
3. **AGENTS.md** — extend "Current Progress" with Phase 2 and Phase 2.5
   tables; mark completed phases; expand "Key Technical Decisions" with new
   patterns from Phases 2 and 2.5; complete the "Design Documents" list.
4. **`docs/architecture.md`** — a new durable reference combining current
   architecture and the forward-looking roadmap. This is the document
   future readers (the author, AI agents, portfolio reviewers) should
   consult to answer "how is HuanGL put together" and "where is it going."

The four documents have distinct audiences and lifecycles, which is why
they remain separate rather than collapsing into one:

| Document | Audience | Cadence |
|----------|----------|---------|
| `README.md` | GitHub visitors, reviewers | End of each phase |
| `AGENTS.md` | AI agents, contributors | When module set changes |
| `docs/architecture.md` | Future self, deep readers | When architecture or roadmap shifts |
| Phase 2.5 plan | Historical archive | Never (frozen artifact) |

All documents are written in English to remain consistent with the existing
`README.md`, `AGENTS.md`, and the four design specs in
`docs/superpowers/specs/`.

---

## 1. Phase 2.5 Plan Archival

### File
`docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md` (new).

### Source
Existing plan at `C:\Users\Episode\.claude\plans\calm-fluttering-clarke.md`.

### Editing rules
- Copy the content verbatim except for the items below.
- Rename the top-level heading to match the convention of the existing
  plans (e.g. `# HuanGL Phase 2.5 Polish — Implementation Plan`).
- Strip the implementation-time references to the temporary plan filename
  (`calm-fluttering-clarke.md`) and to the worktree path.
- Replace the absolute "design spec is at ..." path with a relative
  repository path: `docs/superpowers/specs/2026-05-16-huangl-phase2.5-polish-design.md`.
- Keep step ordering and content unchanged. This is an archive, not a
  rewrite.

### What it intentionally does NOT do
- Does not add post-hoc commentary on what changed during execution.
- Does not add a "results" section. Plans in this project are not living
  documents; they freeze at hand-off to implementation. The actual outcome
  lives in `git log` and the architecture doc.

---

## 2. README.md Update

### Sections kept unchanged
- The intro paragraph (project description and `archive/learnogl-v1` tag).
- The "Repository Layout" tree.
- The "Build on Windows" section (MSVC and clang-cl commands).
- The "Technical Direction" section.

### Sections rewritten

#### `Current Status`
Replace the Phase 1 bullet list with a flat capability statement reflecting
Phase 2.5:

- Deferred PBR shading with metallic-roughness workflow (Cook-Torrance).
- Image-based lighting from a single HDR equirectangular environment
  (irradiance map, prefiltered radiance, BRDF LUT).
- Cascaded Shadow Maps (four cascades, sampler2DArrayShadow, 3×3 PCF).
- Post-processing pass with selectable tone mapping (ACES Filmic,
  Reinhard, linear) and gamma.
- glTF/OBJ/FBX loading through Assimp, including PBR factor extraction,
  normal-map sampling with TBN reconstruction, and packed
  metallic-roughness textures.
- Embedded textures in `.glb` files (Assimp `*N` indices).
- Multi-scene registration with runtime cycling.
- Seven runtime debug views: final, albedo, normal, roughness, metallic,
  linear depth, cascade overlay.
- Window resize propagates through the entire pipeline (GBuffer, Lighting
  HDR target).

#### `Running` (new section, placed after the build instructions)
- Where to place models: `resources/models/` (directory is gitignored).
- Two suggested test models with download commands (curl + git
  sparse-checkout) for `DamagedHelmet.glb` and `Sponza/glTF/`.
- A key-bindings table:
  - `WASD` / mouse — camera fly-through
  - `N` — cycle registered scenes
  - `T` — cycle tone-map operator
  - `0..6` — switch debug view (final, albedo, normal, roughness,
    metallic, depth, cascade overlay)
  - `Esc` — quit

#### `Planned Rendering Phases`
Replace the bullet list with a short table:

| Phase | Status | Theme |
|-------|--------|-------|
| 1 | ✅ Complete | Foundation (GLAD2, RAII wrappers, App loop) |
| 2 | ✅ Complete | Deferred render pipeline (GBuffer, CSM, PBR+IBL) |
| 2.5 | ✅ Complete | Pipeline polish (PostProcess, glTF materials, debug views) |
| 3 | Planned | Scene system and ImGui debug UI |
| 4 | Planned | Bloom, TAA, improved tone mapping |
| 5–8 | Planned | GI techniques (RSM, SSGI, VXGI, DDGI) |

Add a single line below the table: *For detailed roadmap, see
[`docs/architecture.md`](docs/architecture.md).*

### Length target
Final README around 130–160 lines (up from 101).

---

## 3. AGENTS.md Update

### Sections kept unchanged
- "Project Summary".
- "Build Method on Windows".
- "Repository Baseline".

### Sections updated

#### `Key Technical Decisions`
Keep the existing bullets. Append these new bullets reflecting Phase 2 and
Phase 2.5 patterns:

- Reconstruct world position from depth and inverse view-proj in the
  lighting pass; do not write world position to a GBuffer attachment.
- Use `sampler2DArrayShadow` for CSM and let the hardware perform the
  depth comparison; cascade selection uses view-space Z.
- Tone mapping lives in `PostProcessPass`, not in the lighting shader.
  LightingPass writes raw HDR radiance to an RGBA16F target so future
  passes (Bloom, TAA) can read it.
- Re-orthogonalize the TBN basis in the fragment shader, not the vertex
  shader. Interpolation destroys vertex-side orthogonality.
- Treat Assimp texture paths starting with `*` as indices into
  `aiScene::mTextures` and decode the embedded bytes via
  `Texture::Load2DFromMemory`. This is the only correct way to load
  textures from `.glb`.
- `Material::packedMetallicRoughness` flag signals the glTF convention
  (G = roughness, B = metallic) so the shader samples the right channels.
- App registers scenes with soft failure: if a model file is missing or
  fails to load, the app logs and skips, continuing with the remaining
  registered scenes.
- `packed`, `near`, and `far` are reserved or potentially-reserved names
  in GLSL across drivers. Avoid them as GLSL identifiers.

#### `Current Progress`
Keep the Phase 1 table. Append two new tables.

**Phase 2: Render Pipeline — Complete**

| File | Responsibility |
|------|----------------|
| `src/pipeline/RenderPipeline.h/cpp` | Pass orchestrator, owns Shadow/GBuffer/Lighting/PostProcess passes |
| `src/pipeline/passes/ShadowPass.h/cpp` | Four-cascade CSM with sampler2DArrayShadow |
| `src/pipeline/passes/GBufferPass.h/cpp` | Deferred MRT fill (RGBA8 albedo+metallic, RGBA16F normal+roughness, D24 depth) |
| `src/pipeline/passes/LightingPass.h/cpp` | Cook-Torrance PBR + IBL (irradiance + prefilter + BRDF LUT) |
| `src/resource/ResourceManager.h/cpp` | weak_ptr texture and mesh cache with GC |
| `src/resource/MeshLoader.h/cpp` | Assimp wrapper, returns `LoadResult{mesh, materials}` |
| `src/scene/Scene.h` | Scene interface with mesh/material/light access |
| `src/scene/TestScene.h/cpp` | Procedural test scene (floor + spheres + PBR factor materials) |
| `src/renderer/Schema.h` | Mesh, SubMesh, Material, DirectionalLight, Vertex schemas |
| `src/core/Camera.h/cpp` | Free-fly camera, WASD plus mouse look |
| `shader/gbuffer/*.{vert,frag}` | GBuffer fill |
| `shader/shadow/csm.vert` | Cascade depth render |
| `shader/lighting/*.{vert,frag}` | PBR+IBL, IBL precompute (equirect→cubemap, irradiance, prefilter, BRDF LUT), fullscreen helper |

**Phase 2.5: Pipeline Polish — Complete**

| File | Responsibility |
|------|----------------|
| `src/pipeline/passes/PostProcessPass.h/cpp` | Tone mapping, gamma, debug visualization |
| `src/scene/ModelScene.h/cpp` | Loads a model via MeshLoader, adds optional floor |
| `shader/postprocess/postprocess.frag` | ACES / Reinhard / linear tone map plus 7 debug modes |
| `src/renderer/Texture.h/cpp` | Adds `Load2DFromMemory` for `.glb` embedded textures |
| `src/resource/MeshLoader.h/cpp` | Extends to extract PBR textures and handle `*N` embedded paths |
| `src/core/Input.h/cpp` | Adds `IsKeyJustPressed` via GLFW key callback |
| `src/core/App.h/cpp` | Multi-scene registration with N-key cycling, debug-key handling, resize propagation |

#### `Planned Phases`
Replace the bullet list with the same status table used in `README.md`,
then add a single line: *For phase deliverables and ordering rationale see
[`docs/architecture.md`](docs/architecture.md).*

#### `Design Documents`
Replace the list with the complete spec/plan inventory:

- Refactor design: `docs/superpowers/specs/2026-05-13-huangl-refactor-design.md`
- Repository cleanup design: `docs/superpowers/specs/2026-05-13-huangl-repository-cleanup-design.md`
- Phase 2 pipeline design: `docs/superpowers/specs/2026-05-14-huangl-phase2-pipeline-design.md`
- Phase 2.5 polish design: `docs/superpowers/specs/2026-05-16-huangl-phase2.5-polish-design.md`
- Docs overhaul design: `docs/superpowers/specs/2026-05-19-huangl-docs-overhaul-design.md`
- Phase 1 plan: `docs/superpowers/plans/2026-05-13-huangl-phase1-foundation.md`
- Repository cleanup plan: `docs/superpowers/plans/2026-05-13-huangl-repository-cleanup.md`
- Phase 2 pipeline plan: `docs/superpowers/plans/2026-05-14-huangl-phase2-pipeline.md`
- Phase 2.5 polish plan: `docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md`

### Length target
Final AGENTS.md around 220–260 lines (up from 139).

---

## 4. New File: `docs/architecture.md`

The most substantive doc. Combines current architecture with the
forward-looking roadmap so a reader can answer both "how is this built"
and "where is it going" from a single page.

### File location
`docs/architecture.md` (at the docs root, not under `superpowers/`,
because it is not a process artifact).

### Section outline

#### `Vision and Non-Goals`
Two short paragraphs.

- Vision: HuanGL demonstrates a modern OpenGL 4.6 deferred PBR renderer
  and progressively implements global-illumination techniques (RSM → SSGI →
  VXGI → DDGI). The terminal state is Phase 8 plus a polished
  demonstration scene.
- Non-goals: not a production engine, no RHI abstraction, no asset editor,
  no scripting layer, no networked or multi-window support. The project
  optimizes for clarity of demonstration over engine-grade flexibility.

#### `Current Capability`
A short narrative (~100 words) of what HuanGL can render today, followed
by the same key-bindings table that appears in `README.md` for in-context
reference. This section is expected to be updated at every phase
completion.

#### `Render Pipeline`
An ASCII block diagram of the per-frame pass order, with each box
annotated with its inputs, outputs, and texture formats:

```
Scene ──┬─► ShadowPass     → sampler2DArrayShadow (2048² × 4 cascades, D32)
        │
        ├─► GBufferPass    → RT0 RGBA8  (albedo.rgb,  metallic.a)
        │                    RT1 RGBA16F (normal.rgb, roughness.a)
        │                    Depth D24
        │
        └─► LightingPass   → reads GBuffer + shadow + IBL
                             writes RGBA16F HDR target
                                    │
                                    ▼
                             PostProcessPass
                                    │
                             tone map + gamma + debug overlay
                                    │
                                    ▼
                                Backbuffer
```

#### `Module Map`
A table mapping `src/` subdirectories to their responsibility and primary
classes. No file-level enumeration; that belongs in `AGENTS.md`.

| Subdirectory | Responsibility | Key types |
|--------------|----------------|-----------|
| `src/core/` | Window, input, app loop, camera | `App`, `Window`, `Input`, `Camera` |
| `src/renderer/` | OpenGL RAII wrappers and shared schemas | `Shader`, `Buffer`, `Texture`, `Framebuffer`, `Material`, `Mesh` |
| `src/pipeline/` | Render passes and per-frame orchestration | `RenderPipeline`, `*Pass` |
| `src/resource/` | Asset loading and caching | `ResourceManager`, `MeshLoader` |
| `src/scene/` | Scene definitions | `Scene`, `TestScene`, `ModelScene` |
| `src/ui/` | Reserved for ImGui (Phase 3) | — |

#### `Key Design Decisions`
Six to eight concise decisions, each in the form *Decision* / *Reason* /
*Trade-off*. Topics:

1. Deferred shading with depth-reconstructed world position (no world-pos
   GBuffer attachment).
2. `sampler2DArrayShadow` for CSM with hardware comparison and view-space
   Z cascade selection.
3. Direct State Access throughout (`glCreate*`, `glNamed*`,
   `glBindTextureUnit`).
4. No RHI abstraction — OpenGL is the demonstrated interface.
5. Tone mapping in a separate pass, not in the lighting shader, to keep
   the HDR signal available for future passes.
6. Tangent-space normal mapping with fragment-side Gram-Schmidt.
7. Embedded textures (`*N`) resolved against `aiScene::mTextures` for
   `.glb` support.
8. Multi-scene soft-failure loading so missing assets do not block startup.

#### `Roadmap`
The forward-looking section. For each planned phase, list:

- **Goal**: one sentence on what completing the phase unlocks.
- **Deliverables**: file-level outputs (new passes, new shaders, new
  scenes, new docs).
- **Depends on**: which prior phases are required, which can run in
  parallel with this one.
- **Risk / unknowns**: what is most likely to be hard or wrong.

Phases covered:

- **Phase 3 — Scene system and ImGui debug UI.** Unlocks runtime
  parameter tuning (tone map operator, debug mode, light direction,
  camera FOV) and a scene browser. Adds a `ui/` module wrapping
  Dear ImGui. Depends on Phase 2.5. Independent of Phase 4.
- **Phase 4 — Bloom, TAA, improved tone mapping.** First real
  post-processing chain. Adds bright-pass extract, separable Gaussian
  blur, history buffer for TAA, optional Uncharted 2 / AgX tone maps.
  Depends on Phase 2.5 (HDR target). Independent of Phase 3.
- **Phase 5 — Reflective Shadow Maps (RSM).** First indirect-lighting
  technique. Reuses ShadowPass infrastructure plus an RSM render target
  storing flux/normal. Depends on Phase 4 (TAA-stable target). Hardest
  unknown: sample budget for indirect gather.
- **Phase 6 — Screen-Space Global Illumination (SSGI).** Adds to the
  post-process chain. Depends on Phase 4. Independent of Phase 5
  (different cost/quality trade-off — having both lets us A/B).
- **Phase 7 — Voxel-based Global Illumination (VXGI).** Introduces
  scene voxelization into a 3D texture (`Texture::Create3D` already in
  place from Phase 1). Hardest infrastructure step in the roadmap.
  Depends on Phase 4. Independent of Phase 5/6.
- **Phase 8 — Dynamic Diffuse Global Illumination (DDGI).** Probe-based
  irradiance field. Depends on Phase 7 (visibility tracing benefits
  from voxel grid; otherwise needs separate ray tracing path).

End the section with a small ordering rationale: "RSM before SSGI
because RSM extends an existing pass (ShadowPass) while SSGI extends an
existing chain (PostProcess) — completing each one validates that
specific extension axis before introducing both at once. VXGI is placed
after both because it introduces a new resource class (3D textures)
unfamiliar to the existing passes."

#### `Known Limitations`
Numbered list. Each entry: *What is wrong*, *Impact*, *Estimated fix
cost*.

1. `mat3(model)` is used as the normal matrix in `gbuffer.vert`.
   Incorrect for non-uniform scaling. Impact: distorted lighting on
   non-uniformly scaled models. Fix cost: low (one line, plus passing
   the inverse-transpose as a uniform or computing it in shader).
2. `Vertex::tangent` is `vec3`, missing the glTF tangent handedness
   (`w`). The fragment shader recomputes the bitangent via
   `cross(N, T)`, which is correct *up to sign* but may be flipped on
   mirrored UV regions. Fix cost: medium (schema change, mesh loader
   change, vertex layout change, shader change).
3. No frustum culling; every registered mesh is drawn each pass. Impact:
   pure performance — fine for current scenes, will become limiting in
   Sponza-scale assets at higher cascades. Fix cost: medium (AABB
   accumulation in `Mesh`, plus per-pass cull).
4. No mesh batching; each sub-mesh issues its own `glDrawElements`.
   Impact: CPU overhead with many sub-meshes. Fix cost: medium.
5. ResourceManager always loads textures as sRGB. Linear textures
   (normal/roughness/metallic) currently bypass the cache. Fix cost:
   low (extend cache key to include sRGB flag).
6. PostProcess pass clears the backbuffer via `Framebuffer::BindDefault`
   then renders a fullscreen triangle. There is no explicit handling of
   the depth-buffer state if a future pass needs to read it post-tone
   map. Fix cost: low (documented in the pass itself).

#### `What This Doc Is NOT`
Three short bullets:

- Not an API reference. Source-code comments and module headers fill
  that role.
- Not a per-phase implementation detail document. Phase plans under
  `docs/superpowers/plans/` cover that.
- Not a changelog. `git log` is the changelog.

### Length target
Around 300–500 lines of markdown.

---

## Out of Scope

The following are *not* part of this docs overhaul, even though they could
be argued to belong:

- Screenshots or rendered comparisons. The renderer is not yet visually
  polished enough to make compelling captures, and adding placeholders
  invites them to go stale. Defer to Phase 3/4 wrap-up.
- An ADR (architecture decision record) directory. The "Key Design
  Decisions" section of `architecture.md` captures the same information
  with lower overhead; if the decision log grows beyond ten entries, we
  can split it then.
- Auto-generated docs (Doxygen). Header comments are enough for a
  project of this size, and Doxygen would add CI complexity without
  proportional benefit.
- Translation. Documentation stays English-only to match existing
  conventions and the GitHub-public surface.

---

## Implementation Order

1. Write Phase 2.5 plan archival (easy, no dependencies).
2. Write `docs/architecture.md` (defines roadmap wording).
3. Update `AGENTS.md` (references architecture.md for roadmap section).
4. Update `README.md` (references architecture.md for roadmap section).
5. Commit each document as its own change so the history reads as a
   coherent series.

---

## Verification

There is no executable verification for documentation. The proxies are:

- The four documents render correctly on GitHub (Markdown lints clean,
  no broken in-repo links).
- The Phase status table in `README.md` and `AGENTS.md` matches the table
  in `docs/architecture.md`.
- Every spec and plan currently under `docs/superpowers/` is listed in
  `AGENTS.md` "Design Documents".
- The "Key Bindings" table is identical in `README.md` and
  `architecture.md` (single source of truth, but copied for reader
  convenience — verify by diff).
- After the work lands, the question "what is HuanGL?" can be answered
  in five minutes by reading only `README.md` and skimming the
  `architecture.md` pipeline diagram. The question "where is HuanGL
  going?" can be answered by reading the Roadmap section of
  `architecture.md` alone.
