# Program Architecture

## Qt Quick + Core Module Pattern

최종 애플리케이션은 Qt6가 UI, 창, 이벤트 루프, OpenGL context를 소유한다. SDL2는 최종 구조에서 제거한다.

구조 원칙:

- UI 계층은 Qt Quick/QML과 Qt Quick Controls를 우선 사용한다.
- C++ 계층은 QML에 노출할 application backend, model, command API를 제공한다.
- 영상 처리 코어는 UI와 분리된 C-compatible 모듈로 유지한다.
- UI 모듈은 처리 알고리즘을 직접 구현하지 않는다.
- 처리 코어는 Qt Quick item이나 QML 객체를 직접 알지 않는다.
- OpenGL context가 필요한 GPU 처리와 렌더링은 Qt Quick scene graph가 OpenGL backend로 동작하는 조건에서 실행한다.
- 함수 포인터는 입력 backend처럼 실제 유연성이 필요한 곳에만 사용하고, 핵심 모듈은 명시적 함수 호출 중심으로 유지한다.

## Build And Runtime Baseline

Development baseline:

- Language: C++17 for Qt/QML backend, QML for UI, C99-compatible core interfaces for processing algorithms.
- UI framework: Qt Quick/QML + Qt Quick Controls.
- OpenGL host: Qt Quick scene graph OpenGL backend and a QML-visible `QSGRenderNode` viewport item.
- Video decode: FFmpeg.
- Static image IO: Qt image IO.
- Build system: CMake.

Required link targets:

- Qt6 Core
- Qt6 Gui
- Qt6 Qml
- Qt6 Quick
- Qt6 QuickControls2
- Qt6 OpenGL
- OpenGL
- FFmpeg: `avformat`, `avcodec`, `avutil`, `swscale`, `avdevice`
- math library when required by the platform

Removed dependencies:

- Final CMake targets do not find or link SDL2.
- Final source does not use `SDL_Window`, `SDL_Event`, `SDL_GLContext`, `SDL_GL_GetProcAddress`, `SDL_Thread`, `SDL_mutex`, or `SDL_cond`.
- Final source does not use Qt Widgets for the application shell.
- Final UI and services do not expose PPM open/save actions.
- Final static image IO does not require STB headers.

Implementation restrictions:

- OpenGL fixed-function filtering is not a substitute for image processing algorithms or display resampling.
- `GL_LINEAR`, mipmap filtering, or external resize/image-processing APIs must not replace the specified algorithm logic.
- Algorithm formulas, kernels, coordinate transforms, sampling radius, and normalization are implemented in CPU code or GLSL shader code.
- Qt image IO is only for file decode/encode, not for processing algorithm substitution.
- Static image save uses CPU `outImage`.
- Video and realtime stream modes do not save processed output files.
- PPM parsing/saving is not retained.

## Dependency Ownership

| 역할 | 최종 소유자 | 제거/대체 |
|------|-------------|-----------|
| Window | QML `ApplicationWindow` / `QQuickWindow` | `SDL_Window` |
| Event loop | `QGuiApplication` | SDL event loop |
| Panels/parameter controls | Qt Quick Controls | SDL console prompts |
| OpenGL context | Qt Quick scene graph OpenGL context | `SDL_GLContext` |
| GL function access | Qt OpenGL functions | `SDL_GL_GetProcAddress` loader |
| Display renderer | QML viewport render item | SDL renderer wrapper |
| GLSL effect processing | `GpuEffectPipeline` | GPU resources embedded in `ImageProcessorCore` |
| Static image IO | Qt image IO | separate image IO libraries as mandatory dependency |
| Video/stream decode | FFmpeg | 없음 |
| Threading | Qt thread or C++ standard thread | SDL thread/mutex/cond |
| Image algorithms | `ImageProcessorCore` | ad-hoc UI-layer algorithms |

SDL2는 빌드 의존성에서 제거한다. 알고리즘 계약, CPU 처리 코드, GLSL shader, FFmpeg 기반 입력, 검증 fixture는 Qt UI와 분리된 처리 코어에 속한다.

## Porting Boundary

Reusable or translatable parts from the existing SDL2/OpenGL source:

- RGB `ImageBuffer`, `EffectCommand`, algorithm ids, and the 28 CPU algorithm implementations.
- CPU/GLSL parity test intent and small fixture style.
- FFmpeg video and realtime decode behavior, after moving global decoder state into `VideoInputService`.
- GLSL effect and display-resampling shader logic, after moving GL ownership to Qt-managed modules.
- Existing moving-source behavior: video and realtime modes use an effect stack limited to `MAX_EFFECT_STACK = 3`.
- Existing static-image behavior: static image effects are applied directly and are not stack-limited. The Qt design preserves this as no fixed static apply cap.

Discarded or reimplemented parts:

- SDL window, SDL event loop, SDL OpenGL context, SDL swap path, and SDL GL function loader.
- SDL thread, mutex, and condition primitives.
- libedit/readline console UI, command parsing, and terminal completion.
- STB static image read/write as a required dependency.
- PPM parser and PPM saver. The final Qt application does not expose PPM open/save actions.
- SDL renderer shell. Only the shader math, uniform contract, and texture format choices are reusable.

## Shared Data Types

### ImageBuffer

CPU-side RGB image buffer.

Fields:

- `data`: RGB byte array.
- `width`, `height`: image dimensions.
- `channels`: always 3 after load/decode.

Policy:

- Qt `QImage`를 내부 처리 대상으로 직접 쓰지 않는다.
- Qt image IO 결과는 RGB888 `ImageBuffer`로 변환한다.
- 처리 알고리즘은 `ImageBuffer`를 기준으로 CPU/GLSL parity를 맞춘다.

### GpuImage

GPU-side image used by shader effects and display rendering.

Fields:

- `textureId`: OpenGL texture object.
- `fboId`: framebuffer object when renderable.
- `width`, `height`: texture size.
- `internalFormat`: normally `GL_RGBA8` for processing targets.
- `isFresh`: whether the texture reflects the latest processed result.

Policy:

- 객체 생성/삭제는 Qt OpenGL context가 유효할 때 수행한다.
- CPU upload source texture는 RGB 입력을 `GL_RGB8`로 올린다.
- processing/FBO texture는 `GL_RGBA8`를 기본으로 한다.

### ContentRect

Aspect-preserving destination rectangle inside the QML viewport item.

Fields:

- `x`, `y`: top-left offset in viewport item pixels.
- `w`, `h`: displayed image size in viewport item pixels.

Policy:

- viewport resize는 `ContentRect`와 shader uniform만 갱신한다.
- `outImage` 자체는 resize 때문에 변경하지 않는다.

### EffectCommand

One selected algorithm and its parameters.

Fields:

- `algorithmId`: 1 to 28.
- `backend`: CPU, GLSL, or CPU+GLSL.
- `params`: fixed-size parameter payload.
- `passCount`: number of CPU/shader passes.

Policy:

- Static image mode stores the current `outImage`; it does not use an effect stack or fixed apply cap.
- Video/realtime mode stores an effect stack up to 3 commands.

### AlgorithmSpec

Implementation contract for each algorithm.

Fields:

- `algorithmId`
- display name
- category
- default backend
- parameter schema
- pass plan
- kernel/sampling contract
- global-stat requirement
- CPU fallback availability
- GLSL availability
- preview support
- readback requirement

Policy:

- QML parameter controls are generated from, or kept consistent with, `AlgorithmSpec`.
- CPU and GLSL paths must share the same constants.
- Console command tables are not the canonical source for algorithm metadata.

### ParameterSpec

UI-facing parameter metadata for one algorithm parameter.

Fields:

- `key`: stable backend parameter name.
- `label`: UI label.
- `valueType`: integer, float, enum, bool, or automatic.
- `controlType`: slider, spin box, segmented control, combo box, or switch.
- `min`, `max`, `step`: numeric validation range when applicable.
- `defaultValue`: backend default.
- `enumValues`: allowed labels/values for enum parameters.
- `isAutomatic`: true when the user does not edit the value directly.

Policy:

- `ParameterEditor` reads parameter rules from `AlgorithmSpec`.
- UI validation and backend clamping use the same range/default definitions.
- Algorithms without editable parameters still have an empty parameter schema entry.

### SourceType

Supported source modes:

- `SOURCE_NONE`
- `SOURCE_IMAGE`
- `SOURCE_VIDEO_FILE`
- `SOURCE_REALTIME_STREAM`

## Buffer Roles

### inImage

Source frame buffer.

- Static image: original loaded image. It is not modified by algorithms.
- Video/realtime: current decoded RGB frame.
- Processor and renderer treat it as read-only during a pipeline pass.

### outImage

Processed working buffer.

- Static image: accumulated edit result.
- Image load/reset: copied from `inImage`.
- Static algorithm apply: current `outImage` is input, scratch/FBO result becomes new `outImage`.
- Video/realtime: per-frame processed CPU output only when needed.
- Save source only in static image modes.

### display stage

Display-only OpenGL stage.

- Uses CPU `outImage` upload or GPU processed texture.
- Computes `ContentRect`.
- Runs GLSL resampling shader into the Qt Quick viewport render target.
- Never becomes save source.
- Never feeds back into `inImage` or `outImage`.

### previewImage / previewTexture

Temporary candidate result used only while preview is enabled.

- Static image preview reads current `outImage` and writes to `previewImage` or `previewTexture`.
- Preview never replaces `outImage` until the user runs Apply.
- Changing source, changing selected algorithm, changing parameters with preview disabled, Apply, Reset, or Save clears stale preview state.
- Save always uses committed CPU `outImage`, not preview.
- If preview generation fails, the previous committed viewport result remains visible and the preview state is cleared.

### compare source

Display-only reference used for before/after inspection.

- Static image compare uses original `inImage` as before and committed or preview result as after.
- Video/realtime compare uses the current decoded `inImage` as before and the processed current frame as after.
- Compare output never feeds back into processing buffers.
- Split compare computes both source mappings in the viewport shader or render path without modifying CPU buffers.

## Algorithm Processing Contract

All algorithm implementations follow the same processing contract:

- Processing input is RGB 3-channel `uint8_t`.
- Final output is RGB 3-channel `uint8_t`.
- Color-preserving algorithms process R/G/B channels independently.
- Luminance-based algorithms use `Y = 0.299R + 0.587G + 0.114B`.
- Intermediate calculations may use `int`, `float`, or `double`.
- Final output is rounded and clamped to `0..255`.
- Convolution and interpolation boundaries use clamp-to-edge.
- Endpoint detection treats out-of-image coordinates as background.
- Geometric transforms fill unmapped source coordinates with black.
- CPU and GLSL paths follow the same algorithm constants and contracts.

## Pipeline Execution Policy

### Static Image Mode

- Load fills `inImage`.
- `outImage` is initialized as a copy of `inImage`.
- Algorithm apply is event-driven, not repeated every idle frame.
- GLSL effects read back into CPU `outImage` before commit.
- Static image mode preserves the existing direct-edit behavior: committed algorithms accumulate into `outImage` without a fixed apply count limit.
- Reset copies `inImage` back into `outImage` and clears preview state.
- Save serializes `outImage`.

### Video And Realtime Mode

- Each decoded frame replaces `inImage`.
- Each frame applies the effect stack from `inImage`.
- Effect stack limit is 3.
- GPU-capable stack should avoid CPU readback.
- CPU-only effects synchronize only at necessary boundaries.
- Save is disabled.
- Reset clears the effect stack.

### Screen Refresh And Clear Policy

- The active Qt OpenGL framebuffer is cleared before each render.
- Black letterbox/pillarbox regions are allowed around displayed content.
- Display resize output is never reused as processing input.
- Rotate, shrink, flip, reset, and source replacement must not leave stale frame pixels in the viewport.

## Source And Control Policy

### Source Open

Supported open actions:

- Open Image: 일반 이미지 파일 선택.
- Open Video: 비디오 파일 선택.
- Open Stream: URL 또는 장치 입력.

Rules:

- Console input is not used.
- A failed source open preserves the previous valid source and buffers.
- A failed decode preserves the previous valid source and buffers.
- QML file/source dialogs belong to `QmlShell`; services receive normalized paths or URLs.

### Static Image Controls

| UI action | Required behavior |
|-----------|-------------------|
| Process action | Select an algorithm and expose its parameters. |
| Preview | Show candidate result without committing to `outImage`. |
| Apply | Commit current parameters to `outImage`. |
| Reset | Restore `outImage` from `inImage` and clear preview state. |
| Save Image | Save current CPU `outImage`. |

Static image mode has no fixed apply cap. Each Apply mutates the committed `outImage`.

### Video Playback Controls

| UI action | Required behavior |
|-----------|-------------------|
| Play/Pause | Toggle playback state. |
| Step Forward | Decode one frame while paused. |
| Step Backward | Seek/decode the previous frame when the backend can do it. |
| Seek | Move to a requested position. |
| Speed | Support `0.5`, `0.75`, `1.0`, `1.5`, `2.0`. |
| Loop | Decide EOF repeat behavior. |
| Reset Effects | Clear effect stack. |

### Realtime Stream Controls

| UI action | Required behavior |
|-----------|-------------------|
| Open Stream | Open webcam, device input, URL, or RTSP source. |
| Pause Stream | Stop viewport updates without destroying the source. |
| Reconnect | Reopen the previous stream source. |
| Reset Effects | Clear effect stack. |

## Display Resampling Contract

The QML viewport render item owns display-only resampling. It renders through the Qt Quick render cycle instead of a separate window swap path.

### Qt Quick Render Integration

The selected implementation path is `QSGRenderNode`.

Rules:

- `QQuickFramebufferObject` is not the default implementation path.
- `QQuickWindow::beforeRendering` / `afterRendering` hooks are not used for the central viewport.
- The viewport is a QML item whose scene graph node is a `QSGRenderNode`.
- GL resources are created, used, and destroyed only while the Qt Quick render context is current.
- The render node does not read or mutate QML objects directly.
- GUI-thread state is copied into a render-thread-safe snapshot during the item/node sync step.
- The render node restores or declares affected OpenGL state according to Qt scene graph expectations.
- Item-local geometry, clipping, opacity, stacking, and transform behavior must remain compatible with surrounding QML panels and overlays.
- The render node requests updates only when source frame, preview, compare mode, zoom, pan, item size, or backend status changes.

### Direction And Content Rect

- Compare viewport item size with source size.
- Compute `scale = min(viewportW/sourceW, viewportH/sourceH)`.
- Compute `scaledW`, `scaledH`, and centered `ContentRect`.
- Clear the backbuffer before drawing.
- Only fragments inside `ContentRect` map back to source texture coordinates.

### Sobel Edge Map

- No separate CPU edge buffer is required for display resampling.
- The fragment shader samples neighboring source texels.
- Sobel Gx/Gy produces edge strength.
- Edge strength above threshold is edge region; the rest is flat region.

### Hybrid Interpolation

Upscale:

- Edge: Bicubic.
- Flat: Bilinear.

Downscale:

- Edge: Lanczos.
- Flat: Box filter.

Default parameters:

| Item | Value |
|------|-------|
| Edge detector | Sobel |
| Edge threshold | `0.20` |
| Bicubic | Catmull-Rom, `a=-0.5` |
| Lanczos | radius `3` |
| Box | footprint average |
| Boundary | clamp-to-edge |

### Viewport Display Steps

1. CPU `outImage` uploads to the source texture when CPU output is active.
2. GPU result texture is used directly when available.
3. Shader uniforms are updated from source size, viewport size, and `ContentRect`.
4. The shader quad is drawn.
5. Qt Quick completes final composition.

## GPU Fallback Policy

CPU fallback only replaces GPU algorithm execution. It does not replace the Qt OpenGL display path.

Fallback tiers:

| Failure tier | Required behavior |
|--------------|-------------------|
| GLSL effect shader compile/link failure | Run the same algorithm through CPU `ImageBuffer` implementation when available, then upload the CPU result to the OpenGL viewport. |
| Effect FBO or GPU processing texture failure | Disable the affected GPU effect path, run CPU fallback when available, preserve previous state if fallback is unavailable. |
| Display resampling shader failure | Report a fatal render error for the viewport; do not claim the frame rendered. |
| Qt Quick OpenGL context unavailable | Treat as unsupported runtime environment for the GUI renderer. CPU algorithm code may still be testable headlessly, but the application does not provide a non-OpenGL interactive viewport fallback. |

CPU fallback execution:

- Input comes from CPU `outImage` in static image mode or decoded CPU `inImage` in moving-source mode.
- The CPU implementation writes to a scratch `ImageBuffer`.
- On success, static image mode commits scratch to `outImage`.
- On success, video/stream mode uses scratch as the current frame result only for that frame.
- The viewport uploads the resulting CPU RGB buffer to its OpenGL source texture for display.
- If CPU fallback fails, the previous committed image/effect stack remains unchanged.

## Module 1 - QmlShell

Purpose: QML top-level UI.

Data members:

- `ApplicationWindow`
- top app bar / toolbars
- source panel
- algorithm panel
- parameter panel
- playback/status area
- central `ProcessingViewport`

Functions/responsibilities:

| Responsibility | Detail |
|----------------|--------|
| Build UI shell | File/source actions, process categories, playback controls, status |
| Open source | Dispatch image/video/stream open requests |
| Save image | Enabled only for static image modes |
| Algorithm action | Show parameter controls and forward command |
| Playback action | Forward play/pause/seek/speed/loop commands |
| Status update | Reflect controller state after every command |
| Error display | Show non-fatal errors without corrupting state |

## Module 2 - ProcessingController

Purpose: application state and command coordinator.

Data members:

- `ImageState`
- `ImageProcessorCore`
- `GpuEffectPipeline`
- `ImageIoService`
- `VideoInputService`
- pointer/reference to viewport render interface
- preview state
- `lastError`

Functions/responsibilities:

| Responsibility | Detail |
|----------------|--------|
| Open image | Replace source only after successful decode |
| Open video/stream | Initialize FFmpeg input and mode state |
| Apply static effect | Apply once to `outImage`, commit only on success |
| Append moving-source effect | Add to stack only when count < 3 |
| Preview effect | Generate candidate output without committing `outImage` or effect stack |
| Clear preview | Clear preview on source, algorithm, apply, reset, and save transitions |
| Reset | Restore static image or clear moving-source stack |
| Save | Save `outImage` only in static image modes |
| Tick video/stream | Pull frame, process stack, request render |

## Module 3 - ImageIoService

Purpose: static source read/write.

Data members:

- none required beyond temporary buffers and error text

Functions/responsibilities:

| Function | Responsibility |
|----------|----------------|
| Open image | Use Qt image IO, normalize to RGB `ImageBuffer` |
| Save image | Write `outImage` through Qt image IO |

Scope notes:

- QML file dialogs belong to `QmlShell`; this service receives paths.
- Failed open must preserve existing buffers.
- Static image IO should not depend on SDL2.
- Static image IO should not require STB.
- PPM open/save is not implemented.

## Module 4 - VideoInputService

Purpose: video file and realtime frame source.

Data members:

- FFmpeg format/codec/scale contexts.
- playback state: playing, loop, speed, PTS, duration.
- realtime producer state and frame queue.
- synchronization via Qt or C++ standard primitives.

Functions/responsibilities:

| Function | Responsibility |
|----------|----------------|
| Open video | Initialize FFmpeg decoder |
| Read next frame | Decode by PTS schedule into RGB `inImage` |
| Seek | Seek by fraction/time/frame |
| Step | Decode one frame while paused |
| Open stream | Start realtime capture/decoder |
| Read latest stream frame | Copy latest queued frame to `inImage` |
| Reconnect stream | Reopen previous URL/device |
| Close source | Release decoder/thread resources |

Scope notes:

- Producer threads never touch QML items or OpenGL objects.
- GUI thread owns processing and rendering.
- Video/stream modes do not save output files.

## Module 5 - ImageProcessorCore

Purpose: CPU reference implementation for the 28 image processing algorithms.

Data members:

- scratch CPU buffer.
- algorithm spec table.

Functions/responsibilities:

| Function group | Responsibility |
|----------------|----------------|
| CPU effect functions | Reference implementation and fallback |
| Algorithm metadata | Provide `AlgorithmSpec` and `ParameterSpec` |
| Parameter defaults | Provide backend defaults consistent with UI schema |
| Static CPU apply | Apply one effect to current `outImage` |
| Stack CPU apply | Apply up to 3 moving-source effects for fallback |
| Reset helpers | Restore buffers or clear stack state |

Scope notes:

- `ImageProcessorCore` owns no OpenGL texture, FBO, VAO, VBO, or shader program.
- `ImageProcessorCore` never calls OpenGL.
- `ImageProcessorCore` can be unit-tested without a GUI or OpenGL context.
- GLSL execution belongs to `GpuEffectPipeline`.

Algorithm categories:

- Point: brightness, multiply, gamma, thresholds, bitwise, contrast, histogram, grayscale.
- Geometric: flip, rotate.
- Area/filter: emboss, blur, Gaussian, sharpen, motion blur, Sobel, Laplacian, DoG, median.
- Morphological: endpoint detection.

## Module 6 - GpuEffectPipeline

Purpose: GLSL implementation for algorithm effects under the Qt-owned OpenGL context.

Data members:

- effect source texture.
- ping-pong processing textures.
- processing FBO.
- effect shader programs.
- fullscreen geometry resources.
- cached dimensions and freshness flags.
- temporary readback buffer when static image commit needs CPU `outImage`.

Functions/responsibilities:

| Function | Responsibility |
|----------|----------------|
| Initialize GL | Create effect textures, FBOs, shaders, and geometry while Qt GL context is current |
| Apply static effect | Run one GLSL effect from committed `outImage` and read back before commit |
| Apply moving-source stack | Run up to 3 effects and return a `GpuImage` texture for display |
| Compute CPU stats | Request CPU global stats for algorithms that need average/min/max/histogram data |
| Readback | Copy static GLSL result into CPU `outImage` only at commit boundaries |
| Report fallback | Return structured failure so controller can run CPU fallback |
| Destroy GL | Release GL resources while context is valid |

Scope notes:

- `GpuEffectPipeline` owns effect-processing GL resources.
- `ProcessingViewport` owns display-resampling GL resources.
- `GpuEffectPipeline` never owns the Qt window, QML item, or event loop.
- Producer threads never call `GpuEffectPipeline`.
- The pipeline is invoked only from the Qt Quick render context path or a context-safe command queued to that path.
- CPU fallback is handled by `ProcessingController` using `ImageProcessorCore`.

Command and synchronization rules:

- GUI actions create pending processing commands; they do not call OpenGL directly.
- The render-thread path consumes pending GPU commands when the Qt Quick OpenGL context is current.
- Static GLSL Apply is considered incomplete until readback has produced a CPU `outImage`.
- While a static GLSL Apply is pending, a second Apply for the same source is rejected or queued explicitly.
- If a newer preview command supersedes an older preview command, the older preview result is discarded.
- Video/realtime processing uses the latest frame snapshot and may drop stale pending GPU work rather than blocking playback.
- Completion and error reports return to `ProcessingController` through queued, thread-safe notifications.

## Module 7 - ProcessingViewport

Purpose: QML-visible OpenGL display node.

Data members:

- `QSGRenderNode` implementation.
- Qt Quick OpenGL context-owned display shader programs.
- source texture for CPU image uploads.
- current GPU result texture reference.
- VAO/VBO or Qt equivalent geometry resources.
- current item size and `ContentRect`.
- zoom, pan, fit mode, preview mode, and compare mode snapshot.

Functions/responsibilities:

| Function | Responsibility |
|----------|----------------|
| Initialize GL | Create shaders/textures/FBOs after context is current |
| Upload source | Upload CPU RGB `outImage` to texture |
| Use GPU result | Bind processed texture without readback |
| Resample | Compute content rect and shader uniforms |
| Compare render | Render original/result toggle or split compare without modifying buffers |
| Render | Clear backbuffer and draw shader quad |
| Destroy GL | Release GL resources while context is valid |

Scope notes:

- `ProcessingViewport` does not implement image processing algorithms.
- `ProcessingViewport` does not own effect-processing FBOs.
- `ProcessingViewport` may display a texture produced by `GpuEffectPipeline`.
- The central viewport is not implemented with `QQuickFramebufferObject` or window-level `beforeRendering` hooks.

OpenGL defaults:

- OpenGL 3.3 Core Profile target.
- Qt Quick must use the OpenGL scene graph backend for this renderer path.
- Source texture: `GL_RGB8`, `GL_RGB`, `GL_UNSIGNED_BYTE`.
- Processing texture: `GL_RGBA8`.
- Texture filtering: `GL_NEAREST`.
- No mipmaps for algorithm/display substitution.
- Set unpack alignment to 1 before RGB upload.
- Set pack alignment to 1 before readback.

## Module 8 - ParameterEditor

Purpose: expose algorithm parameters through the QML inspector.

Responsibilities:

- Create QML controls from algorithm schema.
- Validate range and type.
- Provide defaults.
- Return `EffectParams`.
- Avoid console input.

Examples:

- Brightness: integer value.
- Multiply/Gamma: floating value.
- Flip: mode selection.
- Gaussian: kernel size and sigma.
- DoG: two sigmas and gain.
- Endpoint: threshold.

## Algorithm Backend Policy

`ImageProcessorCore` provides CPU execution. `GpuEffectPipeline` provides GLSL execution. `ProcessingController` chooses the backend and handles fallback.

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

Parameter schema:

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

Fixed kernels and rules:

- Emboss: `[-1 0 0; 0 0 0; 0 0 1]`, then add 128.
- Blur3x3: all weights `1/9`.
- Blur5x5: all weights `1/25`.
- Sobel Gx: `[-1 0 1; -2 0 2; -1 0 1]`.
- Sobel Gy: `[-1 -2 -1; 0 0 0; 1 2 1]`.
- Laplacian: `[0 -1 0; -1 4 -1; 0 -1 0]`, then add 128.
- High-pass: `[-1 -1 -1; -1 8 -1; -1 -1 -1]`.
- Gaussian weights are generated from sigma and normalized to sum 1.
- Median outputs sorted sample index 4 from a 3x3 neighborhood.

## Failure Handling Policy

| Failure | Required behavior |
|---------|-------------------|
| Image open/decode failure | Preserve previous source and buffers. |
| Static algorithm failure | Preserve committed `outImage` and clear failed preview/candidate state. |
| Effect append failure | Preserve existing effect stack. |
| Shader compile/link failure | Use CPU fallback when available; otherwise reject effect. |
| FBO incomplete/GPU memory failure | Disable affected GPU path and try CPU fallback. |
| Display shader failure | Report fatal render error; do not mark frame rendered. |
| Save failure | Preserve `outImage` and report path/error. |
| Video EOF | Loop if enabled; otherwise pause on last frame. |
| Realtime disconnect | Keep last valid frame and wait for reconnect. |
| Allocation failure | Cancel in-progress replacement and preserve existing state. |

## Data Flow

Static image:

1. QML file dialog returns path.
2. Image IO decodes to RGB `inImage`.
3. Copy `inImage` to `outImage`.
4. Upload `outImage` to the viewport source texture.
5. Display shader renders aspect-preserving result.
6. Preview command writes candidate output to preview state only.
7. Apply command applies to committed `outImage`.
8. GLSL static result is read back to CPU `outImage` before commit completes.
9. Save writes committed `outImage`.

Video/realtime:

1. FFmpeg source produces current RGB frame into `inImage`.
2. Processor applies effect stack.
3. Result is CPU `outImage` or GPU texture.
4. Compare mode can render current `inImage` against the processed frame.
5. Display shader renders result through the `QSGRenderNode` viewport.
6. Save actions are unavailable.

## Object Lifecycle

Startup:

1. Create `QGuiApplication`.
2. Configure Qt Quick to use an OpenGL rendering backend.
3. Create and load QML `ApplicationWindow`.
4. Create `ProcessingController`.
5. Create QML `ProcessingViewport`.
6. Create the viewport scene graph node as a `QSGRenderNode`.
7. Initialize `ProcessingViewport` and `GpuEffectPipeline` GL resources when the Qt Quick render context is current.

Runtime:

- User actions enter through `QmlShell`.
- State-changing commands go through `ProcessingController`.
- Rendering requests go to `ProcessingViewport`.
- GLSL effect execution goes through `GpuEffectPipeline`.
- Video/stream frame updates run through Qt timer/thread scheduling.

Shutdown:

1. Stop video/stream producers.
2. Release CPU buffers.
3. Release `GpuEffectPipeline` resources while context is valid.
4. Release `ProcessingViewport` resources while context is valid.
5. Destroy QML window/items.
6. Exit `QGuiApplication`.

## Validation

- SDL2 does not appear in final CMake link dependencies.
- No final source includes SDL headers.
- Qt Quick/QML owns the window and UI; Qt owns the event loop and OpenGL context.
- Central viewport uses `QSGRenderNode`.
- `ImageProcessorCore` owns no GL resources.
- `GpuEffectPipeline` owns GLSL effect resources.
- Static image path supports load, process, display, save.
- Video/stream modes support effect stack but not save.
- Static image mode has no fixed apply cap.
- Video/stream effect stack rejects the fourth effect.
- PPM open/save actions are absent.
- `AlgorithmSpec` exposes parameter schema for QML controls.
- Display resize preserves aspect ratio and never rewrites `outImage`.
- CPU/GLSL parity is within `±1` per channel on small fixtures.
- Use valgrind to check memory leakage.
