# HuanGL UX Fix & New Sponza Scene Design

Date: 2026-05-22
Status: Approved

## Purpose

Two motivations drove this work. First, the ImGui debug panel is unusable because the camera always captures the mouse, making it impossible to click UI controls. Second, the current demo scenes do not produce a visually impressive result. This spec fixes both.

## Section 1 — Mouse Mode Switch

### Problem

`App::Init` calls `Input::SetCursorCaptured(true)` at startup, locking the cursor for the lifetime of the session. `Camera::Update` reads the raw GLFW cursor position every frame regardless of ImGui focus. There is no way to interact with the debug panel without first toggling `freezeCamera` via a keyboard shortcut.

### Solution

Adopt a right-click-to-look interaction model used by Unreal Engine's viewport and most real-time editors:

- **UI mode (default):** Cursor is visible and free. Camera ignores mouse movement. WASD does not move the camera. ImGui is fully clickable.
- **Camera mode (right mouse button held):** Cursor is hidden and locked to the window center. Camera rotates with mouse delta. WASD translates the camera.

Releasing the right mouse button immediately returns to UI mode.

### Implementation Scope

| File | Change |
|------|--------|
| `src/app/ApplicationState.h` | Add `bool cameraActive = false` |
| `src/app/InputController.cpp` | On RMB press: set `cameraActive = true`, call `Input::SetCursorCaptured(true)`. On RMB release: set `cameraActive = false`, call `Input::SetCursorCaptured(false)`. Gate WASD shortcuts on `cameraActive`. |
| `src/core/App.cpp` | Remove `Input::SetCursorCaptured(true)` from `Init`. Pass `state_.cameraActive && !state_.debugSettings.freezeCamera` to `Camera::Update`. |
| `src/core/Camera.h` | Add `prevCapture_` bool member. When `capture` transitions from false to true, reset `first_ = true` to prevent a cursor-position jump on re-entry. |

### Non-Goals

- No Blender-style Alt+drag orbit.
- No click-to-select entities.
- No separate keyboard shortcut to toggle camera mode.

---

## Section 2 — Intel New Sponza Scene

### Asset

Intel Graphics Research New Sponza (also called "Sponza 2023" or "White Room Sponza"). It is a glTF scene designed specifically for PBR renderer evaluation, featuring detailed fabric, chains, and varied surface types. The expected local path is:

```
resources/models/NewSponza/NewSponza_Main_glTF/NewSponza_Main.gltf
```

The exact sub-path depends on the archive layout; adjust the registered path if needed.

### Implementation Scope

| File | Change |
|------|--------|
| `src/core/App.cpp` | Add `RegisterOptional` for NewSponza after the existing Sponza entry. Use scale `0.01f` and `centerScene = false`. |
| `src/core/Camera.h` | Set default camera position to `{0, 1.5, 0}` facing `+Z` so the first scene view is inside the hall looking down the colonnade. |

The existing `ModelScene` loader handles the glTF load; no new scene type is needed.

### Risk

Intel New Sponza is a high-polygon scene. HuanGL has no frustum culling or mesh batching. Frame rate may be low on integrated graphics. On a discrete GPU this should not be an issue at 1080p. If performance is a concern, a follow-up phase can add AABB frustum culling.

---

## Section 3 — Visual Polish (Default Parameters)

Once New Sponza is running, update the default parameters in code so the first launch looks good without manual tweaking.

### Bloom Defaults

```cpp
BloomSettings {
    enabled   = true,
    threshold = 0.8f,   // was 1.0f — lower catches more highlights
    softKnee  = 0.5f,
    intensity = 0.12f,  // was 0.08f — slightly stronger glow
    radius    = 5,
    mipCount  = 5,
};
```

### Tone Map Default

Change the default tone map operator from linear to ACES. ACES preserves highlight detail and gives the warm filmic look that suits an interior scene.

### IBL

Swap the default HDRI from `brown_photostudio_02_2k.hdr` to a warmer, softer studio or overcast-sky HDR that suits an indoor scene. If no suitable HDR is available locally, keep the existing one and note this as a follow-up.

### Implementation Scope

| File | Change |
|------|--------|
| `src/app/ApplicationState.h` | Update `BloomSettings` defaults (threshold, intensity). Change default `toneMapOperator` to ACES. |
| `src/core/App.cpp` | Optionally swap the HDR path if a better file is available. |

---

## Verification

1. Build and run. Confirm startup shows cursor, camera does not move.
2. Hold right mouse button. Confirm cursor hides, camera rotates.
3. Release right mouse button. Confirm cursor reappears, ImGui panels are clickable.
4. Press `N` to cycle to New Sponza scene. Confirm it loads and renders.
5. Confirm Bloom debug view (`7`) shows glow on bright surfaces.
6. Confirm ACES tone map is the default on first launch.
7. Confirm window resize does not crash.

## Non-Goals

- No TAA.
- No frustum culling (deferred to a follow-up phase if needed).
- No exposure automation.
- No new render passes.
- No asset editor or scene serialization.
