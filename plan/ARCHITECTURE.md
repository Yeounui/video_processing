# OpenCV Migration Architecture

## Current Processing Shape

```text
VideoInputService
    decodes file frames with cv::VideoCapture
    decodes stream frames with direct FFmpeg APIs
    emits ImageBuffer frames

ProcessingController
    owns source state, history, effect stack, and CPU/GPU selection

ImageProcessorCore
    applies 28 CPU algorithms through hand-written C++ loops

GpuEffectPipeline
    applies selected algorithms through OpenGL 3.3 GLSL shaders

ProcessingBackend
    reports backend capabilities and plans CPU prefix plus accelerated suffix

ImageIoService
    loads and saves still images through QImage

ProcessingViewportItem
    displays ImageBuffer output through the Qt Quick scene graph
```

The shared public image type is `ImageBuffer`: packed `uint8_t` RGB/RGBA data
plus width, height, and channel count.

## Target Processing Shape

```text
Qt/QML UI
    -> ProcessingController
        -> VideoInputService       uses cv::VideoCapture for files; FFmpeg remains for streams
        -> ImageIoService          uses cv::imread / cv::imwrite
        -> ImageProcessorCore      uses cv::Mat + cv::imgproc
        -> ProcessingBackend       uses cv::Mat, cv::UMat, or cv::cuda::GpuMat
    -> ProcessingViewportItem      keeps Qt scene-graph display responsibility
```

Boundary rule:

- OpenCV handles decoding, image I/O, image transforms, filters, statistics, and
  optional acceleration.
- Qt handles UI state, QML exposure, timers, signals, URLs, viewport rendering,
  and user interaction.
- `ImageBuffer` remains the external contract until a reviewed decision changes
  it.

## ImageBuffer / cv::Mat Bridge Spec

Required contract:

- Accepted channel counts: 3 and 4.
- Accepted depth: 8-bit unsigned.
- Application boundary channel order: RGB/RGBA.
- OpenCV-local channel order may be BGR/BGRA only inside functions that require
  it.
- Rows must be treated as packed and continuous unless the bridge explicitly
  records stride.
- Empty image behavior must be deterministic and must not crash.
- Zero-copy `cv::Mat` views are allowed only when `ImageBuffer` owns the storage
  for the full lifetime of the view.
- Deep-copy when OpenCV returns non-contiguous data, a changed channel layout, or
  temporary storage that cannot be tied to `ImageBuffer`.
- Unsupported layouts fail explicitly.

## Color And Alpha Contract

- `ImageBuffer` remains RGB/RGBA.
- OpenCV `imread`, `VideoCapture`, and BGR/BGRA-producing APIs must convert to
  RGB/RGBA before exposing frames to controller or viewport code.
- RGB algorithms modify color channels only.
- Existing alpha is preserved where current behavior preserves it.
- Threshold and grayscale-style algorithms keep alpha unchanged when the source
  has alpha.
- Fully transparent pixels must remain `(0, 0, 0, 0)` after every CPU or
  accelerated operation.
- CPU paths must clear RGB for `alpha == 0` after processing.
- Accelerated paths must skip or early-return transparent pixels.

## Algorithm Behavior Spec

The migration must keep the visible algorithm contract stable:

- Algorithm IDs remain 1 through 28.
- Parameter keys, types, ranges, defaults, names, and categories remain stable.
- Static image behavior should match existing tests or reviewed new goldens.
- Video effect stack behavior remains bounded by the current maximum stack size
  of 3 unless a separate decision changes it.

For every algorithm, record:

- Algorithm ID and name.
- Parameter keys and automatic-stat dependencies.
- Input channel requirements.
- Output dimensions and output channel count.
- Border handling.
- Rounding and saturation behavior.
- Alpha behavior.
- CPU OpenCV reference implementation.
- Accelerated eligibility.
- Test tolerance.

Algorithm groups:

| Group | IDs | Required attention |
| --- | --- | --- |
| Point operations | 1-6, 10 | Saturating arithmetic, LUT behavior, statistics, alpha preservation. |
| Geometry | 7-8 | Flip is dimension-preserving; rotate changes dimensions and creates transparent borders. |
| Filters | 9, 11-18 | Kernel definitions, border mode, normalization, transparent pixel cleanup. |
| Edges | 19-24 | Grayscale conversion, signed responses, offsets, scaling, tolerance. |
| Morphology/smoothing | 25 | Kernel size, border handling, alpha cleanup. |
| Grayscale | 26-28 | Formula, rounding, channel replication, alpha preservation. |

## Flip And Rotate Rules

Flip and rotate must remain architecturally different.

Flip:

- Does not change output dimensions.
- Can be accelerated.
- Can participate in fused display-time stacks.
- In a fused backend, flip should be represented as a coordinate transform before
  color operations.
- Example: `Flip -> Brightness -> Grayscale` samples the flipped coordinate
  once, then applies color operations to that sampled pixel.

Rotate:

- Changes output dimensions.
- Materializes a new `ImageBuffer`.
- Outputs RGBA even when the input is RGB.
- Fills pixels outside the rotated source as `(0, 0, 0, 0)`.
- For RGBA inputs, computes rotation bounds from non-transparent alpha content,
  not the full canvas, to avoid repeated canvas growth.
- Does not belong in an accelerated suffix unless a future backend explicitly
  supports geometry-changing outputs and viewport size/version updates.

OpenCV implication:

- `cv::flip` may be sufficient for basic flip after channel/layout rules are
  handled.
- `cv::rotate` is not sufficient for arbitrary-angle rotate.
- `cv::warpAffine` rotate must define output size, center, interpolation,
  border mode, transparent border value, alpha behavior, and bounds policy.

## Multi-Algorithm Stack Model

Previous local commits optimized video effect stacks with a CPU prefix plus GPU
suffix model. Preserve this model for OpenCV migration, but name the concept
generically enough to cover CPU, OpenCL/UMat, CUDA, and any future backend.

For video and stream sources:

- Find the longest accelerated-supported suffix from the end of the effect
  stack with `ProcessingBackend::planVideoStack()`.
- Apply effects before that suffix on CPU.
- Expose the CPU prefix result as `outImage`.
- Apply the accelerated suffix during display or backend execution.
- If the suffix starts at index 0, skip CPU processing for that frame.
- If no suffix exists, apply the whole stack on CPU.
- For CPU-applied prefix entries that require statistics (`5`, `10`, `23`),
  compute the required `stat_*` values from the current prefix image before
  applying the algorithm. This keeps GPU/static-image semantics aligned: GPU
  or hybrid paths consume CPU-computed statistics rather than recomputing them
  inside an accelerated shader.

This matters for stacks such as `Rotate -> Sharpen -> Median`: rotate must run
as the CPU prefix because it changes dimensions and alpha bounds, while
sharpen/median may remain an accelerated suffix if supported.

It also matters for stacks such as `Brightness -> Average Threshold -> Flip`:
brightness changes the image that average threshold must measure, average
threshold runs in the CPU prefix because it requires CPU statistics, and flip
may remain in the accelerated suffix if it is still part of the longest
supported suffix.

## Current GPU Support Baseline

The current OpenGL path supports these algorithm IDs:

`1, 2, 3, 4, 6, 7, 9, 11, 12, 14, 19, 20, 21, 25, 26, 27, 28`

The current fused path supports:

`1, 2, 3, 4, 7, 26, 27, 28`

Current tests assert:

- `Flip -> Brightness -> Grayscale` is fused-supported.
- `Flip -> Blur` is GPU-supported but not fused.
- `Rotate` is not GPU-supported.
- `Median` is GPU-supported but not fused.

The OpenCV migration does not have to preserve this exact support matrix, but it
must preserve the architectural separation between CPU reference behavior,
accelerated eligibility, fused eligibility, and fallback.

Current implementation note:

- `ProcessingBackend` owns the support matrix and stack planner.
- `GpuEffectPipeline` owns OpenGL execution and delegates support reporting to
  `ProcessingBackend`.
- `ProcessingController` depends on backend planning for video suffix splitting
  and no longer queries the OpenGL pipeline directly for stack boundaries.
- `ProcessingController` owns the currently selected accelerated backend kind.
  `CpuReference` is a valid selection and forces static apply plus video stack
  planning through the CPU reference path without emitting GPU work.

## Video Source Spec

The OpenCV video path must define:

- Supported file extensions and actual codec/container expectations.
- Supported stream schemes such as RTSP and HTTP.
- Metadata requirements: width, height, fps, duration, frame count, and
  position.
- Fallback behavior when metadata is unavailable.
- Seeking accuracy expectations.
- Playback speed accuracy expectations.
- End-of-file and loop behavior.
- Stream timeout and reconnect policy.
- Frame drop policy for slow processing.
- Thread-safety and signal-thread rules.

`cv::VideoCapture` can replace direct FFmpeg only after these behaviors are
accepted. If OpenCV cannot provide equivalent timeout, reconnect, or metadata
control in the target environment, direct FFmpeg may need to remain as a
fallback.

Implemented file-video contract:

- Local video files opened through `VideoInputService::open()` use
  `cv::VideoCapture` and keep the existing controller-facing API.
- Frames decoded from OpenCV are treated as BGR/BGRA unless decoded as
  grayscale, then converted to RGB/RGBA `ImageBuffer` output through
  `OpenCvImageBridge`.
- Unsupported decoded frame depths or channel counts fail frame conversion
  instead of leaking invalid layouts.
- FPS metadata uses `CAP_PROP_FPS`; invalid or unavailable FPS falls back to
  `25.0`.
- Duration is `CAP_PROP_FRAME_COUNT / fps` when frame count is available and
  remains `0.0` otherwise.
- File width and height come from `CAP_PROP_FRAME_WIDTH` and
  `CAP_PROP_FRAME_HEIGHT`.
- `seekToSecs()` maps to OpenCV position properties and then decodes the next
  frame, preserving the previous public behavior.
- File EOF still returns no frame from `stepForward()` and lets timer playback
  handle loop or `playbackFinished()`.
- Timer playback restarts the timer when playback speed changes, clamps invalid
  non-positive speed input back to `1.0`, stops and emits
  `playbackFinished()` at EOF when loop is disabled, and seeks to the first
  frame before continuing when loop is enabled.
- Real-time stream open, latest-frame display, disconnect, reconnect, timeout
  interrupt, and status reporting still use the existing FFmpeg path.

## Image I/O Spec

The OpenCV image I/O path must define:

- Officially supported file formats.
- BGR/BGRA to RGB/RGBA conversion rules.
- Empty or failed load behavior.
- Save behavior for RGB and RGBA buffers.
- Whether metadata or EXIF orientation is honored.
- Whether animated image formats are out of scope.

Implemented Phase 3 contract:

- Still-image I/O is implemented by `ImageIoService` through OpenCV
  `imgcodecs`.
- Supported formats are the formats provided by the linked OpenCV `imgcodecs`
  build; tests lock PNG RGB/RGBA behavior because it is the lossless reference
  format used for parity checks.
- Load uses `cv::imread(..., cv::IMREAD_UNCHANGED)`.
- Loaded 8-bit grayscale images are expanded to 3-channel RGB.
- Loaded 8-bit BGR and BGRA images are converted to RGB and RGBA before leaving
  `ImageIoService`.
- Unsupported channel counts, non-8-bit decoded images, missing files, and
  decoder failures return `nullptr`.
- Save accepts only valid, non-empty 3-channel RGB or 4-channel RGBA
  `ImageBuffer` values.
- Save converts RGB/RGBA to BGR/BGRA before calling `cv::imwrite`.
- RGBA JPEG output drops alpha before encoding because JPEG has no alpha
  channel and the previous QImage save path effectively produced color-only
  JPEG output.
- Encoder failures, unsupported writers, invalid buffers, and thrown OpenCV
  exceptions return `false`.
- Metadata preservation, EXIF auto-orientation, color profiles, and animated
  image formats are out of scope for this phase.

## Statistics Spec

Automatic parameters depend on image statistics. The OpenCV path must define:

- Average luminance formula and rounding.
- Global min/max scope: color channels only or all channels.
- Histogram luminance formula.
- Histogram min/max selection for empty or flat histograms.
- Whether alpha-transparent pixels contribute to statistics.
- Whether stats are recomputed per video frame.

## Acceleration Backend Spec

Backend names should be explicit:

- CPU `cv::Mat`
- OpenCL/UMat
- CUDA/GpuMat

Required rules:

- CPU is the reference backend.
- Accelerated backend availability is detected at runtime.
- CUDA is not required for application startup.
- Per-algorithm support is represented as backend capability, not hard-coded UI
  behavior.
- Accelerated failure falls back or reports without corrupting controller state.
- Upload/download ownership and caching policy must be explicit.
- Accelerated output must be byte-compatible or tolerance-compatible with the
  CPU reference according to [[REVIEW]].
- Display-only video suffixes should avoid CPU readback where the backend and
  viewport boundary permit it.

Current controller rules:

- Static-image accelerated apply stores the algorithm ID, CPU-prepared
  parameter map, previous output image, and output version at dispatch time.
- `commitGpuResult()` accepts the result only when the pending output pointer
  and version still match the current controller output.
- `cancelGpuApply()` treats accelerated failure as a CPU-reference fallback
  when the pending request is still current; stale failures are discarded.
- Source changes, reset, undo, redo, and direct output replacement invalidate
  any pending static-image accelerated request.

## Viewport Boundary

The viewport remains Qt-owned:

- Input format is `ImageBuffer`.
- Upload format is RGB/RGBA.
- Transparent pixels composite through the existing viewport background rule.
- Split compare, zoom, pan, and original/output switching stay viewport
  responsibilities.
- Stale accelerated results must be rejected if the source changes mid-flight.
- OpenCV types must not cross into QML-facing APIs.
