# Architecture

Canonical architecture document. Detailed algorithm formulae and module narratives originate in `structure.md`; this document holds the working contract and tables the rest of the plan refers to.

## Build And Runtime Baseline

- Language: C++17 (Qt/QML backend), QML (UI), C99-compatible interfaces (processing core).
- UI framework: Qt Quick / QML + Qt Quick Controls.
- OpenGL host: Qt Quick scene graph OpenGL backend; QML-visible `QSGRenderNode` viewport item.
- Video decode: FFmpeg (`avformat`, `avcodec`, `avutil`, `swscale`, `avdevice`).
- Static image IO: Qt image IO.
- Build system: CMake.

Required link targets: Qt6 Core, Qt6 Gui, Qt6 Qml, Qt6 Quick, Qt6 QuickControls2, Qt6 OpenGL, OpenGL, FFmpeg components above, math library when platform requires.

Removed dependencies: SDL2, Qt Widgets shell, PPM parser/saver, STB headers as required IO.

Implementation restrictions: no `GL_LINEAR`/mipmap/external resize substitution for algorithm or display resampling; algorithm formulae live in CPU code or GLSL only; Qt image IO is decode/encode only.

## Dependency Ownership

| Role | Final owner | Removed/replaced |
|------|-------------|------------------|
| Window | QML `ApplicationWindow` / `QQuickWindow` | `SDL_Window` |
| Event loop | `QGuiApplication` | SDL event loop |
| Panels/parameter controls | Qt Quick Controls | SDL console prompts |
| OpenGL context | Qt Quick scene graph OpenGL context | `SDL_GLContext` |
| GL function access | Qt OpenGL functions | `SDL_GL_GetProcAddress` |
| Display renderer | QML viewport render item | SDL renderer wrapper |
| GLSL effect processing | `GpuEffectPipeline` | GPU resources in `ImageProcessorCore` |
| Static image IO | Qt image IO | separate image IO libraries |
| Video/stream decode | FFmpeg | n/a |
| Threading | Qt thread / C++ standard thread | SDL thread/mutex/cond |
| Image algorithms | `ImageProcessorCore` | ad-hoc UI-layer algorithms |

## Modules

### Module 1 — QmlShell

QML top-level UI. Owns `ApplicationWindow`, top bar, source panel, algorithm panel, parameter panel, playback/status area, central `ProcessingViewport`. Responsibilities: build UI shell; dispatch source open; save only in static mode; forward algorithm/playback commands; reflect controller state; surface non-fatal errors without corrupting state.

### Module 2 — ProcessingController

Application state and command coordinator. Owns `ImageState`, `ImageProcessorCore`, `ImageIoService`, `VideoInputService`, viewport render interface pointer, thread-safe `EffectCommand` queue (mutex-protected std::deque; consumed by render node's GpuEffectPipeline), worker thread (QThread), `lastError`. Responsibilities: open image/video/stream with commit-on-success; static apply queues `EffectCommand` to worker thread and receives completion/failure via Qt::QueuedConnection; moving-source append rejects when stack >= 3; reset restores static image or clears moving stack; save only in static mode; tick video/stream processing and request render. Holds a raw pointer to `ProcessingViewportItem` (set at init via `setCommandQueue`); calls `viewportItem->update()` after pushing a GLSL EffectCommand (D37).

### Module 3 — ImageIoService

Static source read/write through Qt image IO. Receives normalized paths from `QmlShell`. Failed open preserves existing buffers. No SDL2, no STB, no PPM.

### Module 4 — VideoInputService

FFmpeg-based video file + realtime stream source. Owns format/codec/scale contexts, playback state (playing, loop, speed, PTS, duration), realtime producer state and frame queue, Qt or C++ standard sync primitives. Operations: open video, read next frame by PTS, seek, step, open stream, read latest stream frame, reconnect, close. Producer threads never touch QML items or OpenGL objects. Video/stream modes do not save output files.

### Module 5 — ImageProcessorCore

CPU reference implementation for the 28 algorithms. Owns scratch CPU buffer and algorithm spec table. Provides CPU effect functions, `AlgorithmSpec`/`ParameterSpec` metadata, backend defaults, static CPU apply, stack CPU apply (≤3), reset helpers. Owns no OpenGL resources; never calls OpenGL; unit-testable without GUI or GL context. Categories: point, geometric, area/filter, morphological.

### Module 6 — GpuEffectPipeline

GLSL implementation under the Qt-owned OpenGL context. Owns effect source texture, ping-pong processing textures, processing FBO, effect shader programs, fullscreen geometry, cached dimensions, freshness flags, temporary readback buffer for static commit. Operations: initialize GL while Qt GL context is current; apply static effect from committed `outImage` and read back before commit; apply moving-source stack (≤3) and return `GpuImage`; request CPU global stats for stats-dependent algorithms; readback only at commit boundaries; report structured failure for fallback; destroy GL resources while context valid.

Command/sync rules: GUI actions create pending GPU commands; render-thread path consumes them; static GLSL apply is incomplete until readback; second apply for same source is rejected or queued; moving-source uses latest frame snapshot and may drop stale GPU work; completion/error returns to `ProcessingController` via thread-safe notifications. Stats for stats-dependent algorithms (#5, #10, #23) are received via EffectCommand fields (CachedStats computed on worker thread before dispatch); GpuEffectPipeline reads stats from the command and does not make cross-thread stats calls at render time.

### Module 7 — ProcessingViewport

C++ class name: `ProcessingViewportItem` (QQuickItem subclass, registered via `QML_NAMED_ELEMENT("ProcessingViewportItem")`). QML panel file: `ProcessingViewport.qml` (contains a `ProcessingViewportItem` instance). The owned `QSGRenderNode` subclass is unnamed in QML.

QML-visible `QSGRenderNode` display node. Owns Qt Quick OpenGL context-owned display shader programs, source texture for CPU uploads, current GPU result texture reference, geometry resources, current item size and `ContentRect`, zoom/pan/fit/preview/compare snapshot. Operations: initialize GL after context current; upload CPU `outImage`; bind processed texture without readback; compute content rect and shader uniforms; render original/result toggle and split compare; clear backbuffer and draw shader quad; destroy GL.

Does not implement algorithms. Does not own effect-processing FBOs. May display a `GpuEffectPipeline` texture. Not implemented with `QQuickFramebufferObject` or window-level `beforeRendering` hooks.

OpenGL defaults: OpenGL 3.3 Core Profile; Qt Quick must use OpenGL scene graph backend; source texture `GL_RGB8`/`GL_RGB`/`GL_UNSIGNED_BYTE`; processing texture `GL_RGBA8`; filter `GL_NEAREST`; no mipmaps; unpack alignment 1 before RGB upload; pack alignment 1 before readback.

### Module 8 — ParameterEditor

Generates QML controls from algorithm schema, validates range/type, provides defaults, returns `EffectParams`. No console input.

## Undo/Redo Command Pattern

ProcessingController owns a `std::deque<std::unique_ptr<EditCommand>>` history with a current-index cursor. Maximum 20 steps; oldest entry is dropped when the limit is reached. Branching (new command after undo) truncates forward history.

Undoable operations:
- Static Apply: stores previous outImage snapshot (deep copy of RGB888 buffer).
- Effect stack append: stores previous stack state.
- Effect stack removeAt(index): stores previous stack state.
- Effect stack clear: stores previous stack state.
- Video/stream mode: only effect stack mutations; no image snapshots.
- `ParameterEditCommand` — records old/new parameter map for a single effect stack entry; applicable in video/stream mode only (static mode has no persistent effect stack).

Reset clears the undo/redo history.

EditCommand interface:
```
virtual void undo() = 0;
virtual void redo() = 0;
```

Memory Budget: Total undo history memory is capped at 512 MB. When a new entry is pushed and `total_bytes + new_bytes > 512 MB`, the oldest entries are evicted until the budget fits, with a minimum of 1 entry retained. The 20-step count cap is also enforced; both limits apply.

CPU stat cache (for algorithms #5, #10, #23) is invalidated whenever outImage is replaced, including by undo/redo.

## Thread Topology

- GUI thread: ProcessingController, QML engine, application state, command queue writes.
- Render thread: QSGRenderNode::render(), GpuEffectPipeline, ProcessingViewport display pass.
- updatePaintNode() is called on the render thread while the GUI thread is blocked; use it to copy GUI-thread state snapshots (image pointer, zoom, compare mode) into render-thread-safe structs.
- Worker thread (owned by ProcessingController): CPU algorithm execution (ImageProcessorCore). ProcessingController pushes Apply commands to the worker queue; completion/failure signals return via Qt::QueuedConnection. applyInFlight flag prevents re-entry; QML Apply button enabled binding follows this flag.
- Completion and error signals route from render thread to GUI thread via Qt::QueuedConnection.
- GpuEffectPipeline is initialized on first render() call and destroyed in the render node destructor while the GL context is still valid.
- GUI thread never calls OpenGL.
- Apply dispatch: All Apply commands (CPU and GLSL) enter via worker thread. Worker thread computes stats for stats-dependent algorithms if needed, then either executes the CPU algorithm directly or posts an EffectCommand (with CachedStats attached) to the render-thread queue. Both paths emit completion/failure via Qt::QueuedConnection to ProcessingController (GUI thread), which swaps outImage, pushes EditCommand, and resets applyInFlight. While applyInFlight=true, Undo, Redo, Load, and Reset are also disabled (D36).
- outImage write authority: only the GUI thread (in completion signal handlers) writes to outImage. Worker and render threads write to scratch ImageBuffers and deliver them via completion signals as `std::shared_ptr<ImageBuffer>` (D34, D39). updatePaintNode() copies the shared_ptr snapshot into render-thread-local state, keeping the buffer alive through render() even if GUI thread swaps outImage mid-frame. Worker captures a shared_ptr snapshot of outImage at task start to avoid race with concurrent GUI-thread swap (D39). Same rule applies to inImage: VideoInputService delivers new frames as shared_ptr via signal to GUI thread; GUI thread is the sole writer of inImage_ (D39).
- Video/stream inImage upload: when the effect stack is non-empty, GpuEffectPipeline uploads the inImage pointer snapshot (copied in updatePaintNode) to the effect source texture at the start of render() (D35).

## QSGRenderNode GL Contract

- render() wraps all raw GL calls with beginExternalCommands() / endExternalCommands().
- changedStates() declares every OpenGL state bit mutated: DepthState, BlendState, ViewportState, ScissorState at minimum.
- No manual glGet/glSet save-restore; Qt RHI restores states declared in changedStates().

## Texture Handoff: GpuEffectPipeline → ProcessingViewport

```
struct DisplaySource {
    const ImageBuffer* cpuImage = nullptr;  // render-thread-local; valid only during render()
    GLuint gpuTextureId = 0;                // 0 = use CPU path
};
```

Within a single render() call: GpuEffectPipeline executes first and writes gpuTextureId; ProcessingViewport display pass reads it. Both run in the same GL context activation window; no cross-frame sync needed.

## Shared Data Types

- `ImageBuffer` — CPU RGB888 (`data`, `width`, `height`, `channels=3`). `QImage` not used as processing target.
- `GpuImage` — `textureId`, `fboId`, `width`, `height`, `internalFormat` (default `GL_RGBA8`), `isFresh`. Created/destroyed only when Qt OpenGL context is current.
- `ContentRect` — `x`, `y`, `w`, `h` in viewport item pixels. Resize updates only `ContentRect` and shader uniforms; never `outImage`.
- `EffectCommand` — `algorithmId` (1..28), `backend` (CPU/GLSL/CPU+GLSL), `params`, `passCount`.
- `AlgorithmSpec` — id, name, category, default backend, parameter schema, pass plan, kernel/sampling contract, global-stat requirement, CPU fallback availability, GLSL availability, preview support, readback requirement.
- `ParameterSpec` — `key`, `label`, `valueType`, `controlType`, `min`/`max`/`step`, `defaultValue`, `enumValues`, `isAutomatic`.
- `SourceType` — `SOURCE_NONE`, `SOURCE_IMAGE`, `SOURCE_VIDEO_FILE`, `SOURCE_REALTIME_STREAM`.

## Buffer Roles

- `inImage` — source frame; read-only during pipeline pass. Qt image IO decode → `QImage::Format_RGB888` → `ImageBuffer` (all source formats normalized at load time; alpha stripped, 16-bit downsampled, grayscale expanded).
- `outImage` — processed working buffer; copied from `inImage` on load/reset; static apply mutates committed `outImage`; video/realtime use it as per-frame processed CPU output only when needed; save source in static modes only.
- display stage — display-only OpenGL stage; uses CPU `outImage` upload or GPU processed texture; computes `ContentRect`; never feeds back into `inImage`/`outImage`; never save source.
- compare source — display-only before/after reference; never feeds back into processing buffers.

## Pipeline Execution Policy

Static image mode: load fills `inImage`; `outImage = copy(inImage)`; apply event-driven (not per idle frame); GLSL effects read back into CPU `outImage` before commit; **no fixed apply cap**; reset copies `inImage` into `outImage`; save serializes `outImage`.

Video / realtime mode: each decoded frame replaces `inImage`; effect stack applied per frame; **stack limit = 3**; GPU-capable stack avoids CPU readback; CPU-only effects synchronize only at necessary boundaries; **save disabled**; reset clears the effect stack.

Screen refresh & clear: active Qt OpenGL framebuffer cleared before each render; black letterbox/pillarbox allowed; display resize output never reused as processing input; rotate/shrink/flip/reset/source replacement must not leave stale pixels.

## Algorithm Processing Contract

Input and output are RGB 3-channel `uint8_t`. Color-preserving algorithms process R/G/B independently. Luminance uses `Y = 0.299R + 0.587G + 0.114B`. Intermediate calculations may use `int`/`float`/`double`. Final output `round()` and clamp `0..255`. Convolution and interpolation boundaries use clamp-to-edge. Endpoint detection treats out-of-image as background. Geometric transforms fill unmapped source coordinates with black. CPU and GLSL paths share the same constants.

## Algorithm Backend Policy

| Category | Default backend | Detail |
|----------|-----------------|--------|
| Point processing | GLSL first | Independent per-pixel calculations. |
| Geometric | GLSL first | Backward coordinate mapping. |
| Local filter | GLSL first | Neighbor texel sampling. |
| Multipass filter | GLSL first | Ping-pong FBO processing. |
| Global statistics | CPU or CPU+GLSL | Average, min/max, or histogram pass required. |
| Byte bitwise | CPU first | Exact integer byte operations required. |

## Algorithm Catalog

All algorithms operate on RGB `ImageBuffer` values unless stated otherwise. Final outputs use `round()` and clamp to `0..255`.

| # | Algorithm | Backend | Parameters | Processing contract |
|---|-----------|---------|------------|---------------------|
| 1 | Brightness | GLSL/CPU | delta `-255..255` | Add delta to each channel. |
| 2 | Multiply | GLSL/CPU | factor `0..4` | Multiply each channel. |
| 3 | Gamma | GLSL/CPU | gamma `0.1..5` | Apply non-linear gamma mapping. |
| 4 | Fixed Threshold | GLSL/CPU | threshold `127` | Luminance threshold to black/white. |
| 5 | Average Threshold | CPU+GLSL | auto average | Compute average luminance, then threshold. |
| 6 | Bitwise AND | CPU | mask `0x00..0xFF` | Byte-accurate bitwise AND. |
| 7 | Flip | GLSL/CPU | H/V/Both | Coordinate mirror transform. |
| 8 | Rotate | GLSL/CPU | degrees | Center-based backward mapping; empty pixels black. |
| 9 | Emboss | GLSL/CPU | fixed | Diagonal kernel plus 128. |
| 10 | Contrast Stretch | CPU+GLSL | auto min/max | Linear stretch from min/max to 0..255. |
| 11 | 3x3 Blur | GLSL/CPU | fixed | Uniform 3x3 average. |
| 12 | 5x5 Blur | GLSL/CPU | fixed | Uniform 5x5 average. |
| 13 | Gaussian Blur | GLSL/CPU | kernel, sigma | Separable two-pass normalized Gaussian. |
| 14 | Sharpen | GLSL/CPU | alpha | Unsharp formula using blurred image. |
| 15 | High-Pass Sharpen | GLSL/CPU | strength | Add high-pass Laplacian detail. |
| 16 | High-Boost | GLSL/CPU | beta | Boost high-frequency component. |
| 17 | Diagonal Motion Blur | GLSL/CPU | odd distance | Average centered diagonal samples. |
| 18 | Horizontal Motion Blur | GLSL/CPU | odd distance | Average centered horizontal samples. |
| 19 | Horizontal Edge | GLSL/CPU | scale | Sobel `Gy`, grayscale output. |
| 20 | Vertical Edge | GLSL/CPU | scale | Sobel `Gx`, grayscale output. |
| 21 | Laplacian | GLSL/CPU | fixed offset | 4-neighbor Laplacian plus 128. |
| 22 | DoG | GLSL/CPU | sigmas, gain | Difference of two Gaussian blurred images. |
| 23 | Histogram Stretch | CPU+GLSL | auto histogram | Stretch luminance distribution. |
| 24 | Endpoint Detection | GLSL/CPU | threshold | Foreground pixel with exactly one foreground neighbor. |
| 25 | Median Smoothing | GLSL/CPU | fixed 3x3 | Channel-wise median of 9 samples. |
| 26 | Grayscale Average | GLSL/CPU | none | Average RGB channels. |
| 27 | Grayscale Luminosity | GLSL/CPU | none | Weighted perceptual luminance. |
| 28 | Grayscale Lightness | GLSL/CPU | none | Average of max and min channel. |

## Parameter Schema

| # | Parameter | Range | Default |
|---|-----------|-------|---------|
| 1 | delta | `-255..255` | `30` |
| 2 | factor | `0.0..4.0` | `1.2` |
| 3 | gamma | `0.1..5.0` | `2.2` |
| 4 | threshold | fixed/editable | `127` |
| 5 | average | automatic | image average |
| 6 | mask | `0x00..0xFF` | `0xC9` |
| 7 | flip mode | H/V/Both | H |
| 8 | degree | `-360..360` | `30` |
| 13 | kernel, sigma | `3/5/7`, `0.1..5.0` | `5`, `1.0` |
| 14 | alpha | `0.0..3.0` | `1.0` |
| 15 | strength | `0.0..3.0` | `1.0` |
| 16 | beta | `0.0..3.0` | `1.0` |
| 17 | distance | odd `3..31` | `9` |
| 18 | distance | odd `3..31` | `9` |
| 19 | scale | `0.1..4.0` | `1.0` |
| 20 | scale | `0.1..4.0` | `1.0` |
| 22 | sigmas, gain | `small < large`, `0.1..4.0` | `1.0`, `2.0`, `1.0` |
| 24 | threshold | `0..255` | `127` |

## Fixed Kernels And Rules

- Emboss: `[-1 0 0; 0 0 0; 0 0 1]`, then add 128.
- Blur3x3: all weights `1/9`.
- Blur5x5: all weights `1/25`.
- Sobel Gx: `[-1 0 1; -2 0 2; -1 0 1]`.
- Sobel Gy: `[-1 -2 -1; 0 0 0; 1 2 1]`.
- Laplacian: `[0 -1 0; -1 4 -1; 0 -1 0]`, then add 128.
- High-pass: `[-1 -1 -1; -1 8 -1; -1 -1 -1]`.
- Gaussian weights are generated from sigma and normalized to sum 1.
- Median outputs sorted sample index 4 from a 3x3 neighborhood.

## Display Resampling Contract

Render path: `QSGRenderNode` only. `QQuickFramebufferObject` and window-level `beforeRendering`/`afterRendering` hooks are not used for the central viewport. GL resources created/used/destroyed only while the Qt Quick render context is current. The render node does not read or mutate QML objects directly; GUI-thread state is copied into a render-thread-safe snapshot during the item/node sync step. Render node restores OpenGL state per Qt scene graph expectations. Item-local geometry/clipping/opacity/stacking/transform remain compatible with surrounding QML panels and overlays. Render node requests updates only on source frame, preview, compare mode, zoom, pan, item size, or backend status change.

Content rect: compare item size with source size; `scale = min(viewportW/sourceW, viewportH/sourceH)`; compute `scaledW`, `scaledH`, centered `ContentRect`; clear backbuffer before drawing; only fragments inside `ContentRect` map back to source texture coordinates.

Sobel edge map: no separate CPU edge buffer; fragment shader samples neighboring source texels; Sobel `Gx`/`Gy` produces edge strength; edge strength above threshold is edge region.

Hybrid interpolation:

| Direction | Edge | Flat |
|-----------|------|------|
| Upscale | Bicubic Catmull-Rom (a=-0.5) | Bilinear |
| Downscale | Lanczos (radius 3) | Box (footprint average) |

Defaults: Sobel edge detector, edge threshold `0.20`, boundary clamp-to-edge.

Viewport display steps: 1) CPU `outImage` uploads to source texture when CPU output is active; 2) GPU result texture used directly when available; 3) shader uniforms updated from source/viewport size + `ContentRect`; 4) shader quad drawn; 5) Qt Quick completes final composition.

## GPU Fallback Policy

CPU fallback replaces GPU algorithm execution only; it does not replace the Qt OpenGL display path.

| Failure tier | Required behavior |
|--------------|-------------------|
| GLSL effect shader compile/link failure | Run same algorithm through CPU `ImageBuffer` when available, then upload CPU result. |
| Effect FBO / GPU processing texture failure | Disable affected GPU path and try CPU fallback when available, preserve previous state if unavailable. |
| Display resampling shader failure | Report fatal render error for the viewport; do not claim frame rendered. |
| Qt Quick OpenGL context unavailable | Treat as unsupported runtime environment; no non-OpenGL interactive viewport fallback. |
| GpuEffectPipeline init failure (first render()) | Flush all queued EffectCommands: attempt CPU fallback for each; emit gpuApplyFailed for CPU-unavailable commands. Disable GLSL Apply for the remainder of the session. |

CPU fallback execution: input from CPU `outImage` (static) or decoded CPU `inImage` (moving); CPU writes to scratch `ImageBuffer`; on success static commits scratch to `outImage`, moving uses scratch as that frame's result; viewport uploads CPU RGB to OpenGL source texture for display; on failure previous committed state is preserved.

## Data Flow

Static image: QML file dialog → image IO decodes to RGB `inImage` → copy to `outImage` → upload to viewport source texture → display shader renders aspect-preserving → preview writes candidate only → apply commits to `outImage` → GLSL static result reads back to CPU before commit → save writes committed `outImage`.

Video / realtime: FFmpeg source → RGB frame into `inImage` → processor applies effect stack → result CPU `outImage` or GPU texture → compare may render `inImage` vs processed → display shader renders via `QSGRenderNode` viewport → save unavailable.

## Object Lifecycle

Startup: create `QGuiApplication` → configure Qt Quick OpenGL backend → load QML `ApplicationWindow` → create `ProcessingController` → create QML `ProcessingViewport` → create scene graph `QSGRenderNode` → initialize `ProcessingViewport` and `GpuEffectPipeline` GL resources when render context is current.

Runtime: user actions enter `QmlShell` → state-changing commands route through `ProcessingController` → rendering goes to `ProcessingViewport` → GLSL effects through `GpuEffectPipeline` → video/stream frame updates run on Qt timer/thread scheduling.

Shutdown: stop video/stream producers → release CPU buffers → release `GpuEffectPipeline` resources while context valid → release `ProcessingViewport` resources while context valid → destroy QML window/items → exit `QGuiApplication`.

## UI Layout And Color

Reference detail: `ui.md`. The condensed working contract follows.

Color tokens:

| Token | Hex | Use |
|-------|-----|-----|
| `sky.base` | `#8FA9C4` | main accent |
| `sky.deep` | `#6F86AB` | active buttons, selected controls |
| `sky.light` | `#DDEEF4` | navigation highlight, subtle bands |
| `paper` | `#F8F8F5` | app background |
| `paper.warm` | `#E9E3D4` | secondary quiet blocks |
| `ink` | `#2F3438` | primary text |
| `ink.muted` | `#6E747A` | secondary text |
| `line` | `#E5E8EB` | dividers, input borders |
| `white` | `#FFFFFF` | card and field base |

Layout regions: Top Bar (52 px) / Source Panel (260 px default, 220 px min, collapsible) / `ProcessingViewport` center / Algorithm Inspector (340 px default, 300 px min, collapsible) / Bottom Bar (44 px static, 64 px video/stream).

Spacing/shape: app padding 12 px, panel inner 10 px, card padding 12 px, control height 32 px, toolbar button 34 px, icon 18 px, card radius 6-8 px, button radius 6 px, input radius 4-6 px, pill radius 999 px.

QML components: `App.qml`, `TopBar.qml`, `SourcePanel.qml`, `ProcessingViewport.qml`, `AlgorithmInspector.qml`, `ParameterEditor.qml`, `BottomTransport.qml`, `StatusBadge.qml`, `IconButton.qml`, `PillButton.qml`, `IconRail.qml`, `SearchField.qml`, `ProgressMeter.qml`, `PanelCard.qml`.

C++ exposed to QML: `AppController` (source state, current metadata, algorithm list model, command methods open/save/apply/preview/reset/playback), `AlgorithmModel` (category, display name, parameters, backend support), `ProcessingViewportItem` (`QSGRenderNode`-backed custom render item receiving texture/image update notifications).

Inspector categories: Point / Geometry / Filter / Edge / Morphology / Grayscale.

UI acceptance criteria:

- First screen is the working processing interface, not a landing page.
- QML UI uses `desktop.webp` color mood.
- Dashboard-style controls are light, rounded, and elegant.
- All 28 algorithms discoverable from the inspector.
- Parameter controls visible without console input.
- Central viewport remains the visual focus.
- Static image, video, and stream modes have distinct but consistent states.
- The design can be implemented without Qt Widgets.
