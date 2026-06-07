# Plan Documents

Status: generated

This directory contains the implementation plan for migrating the image and
video processing internals to OpenCV while keeping the Qt/QML application
surface stable.

These documents were generated from root `plan.md`, current source inspection,
and local git history. They have not yet been reviewed as final implementation
requirements.

## Document Map

| Document | Status | Purpose | Next action |
| --- | --- | --- | --- |
| [[README]] | generated | Current status, document map, source evidence, and open questions. | Review generated status and source evidence. |
| [[OVERVIEW]] | generated | Goal, scope, non-goals, and constraints. | Review scope before implementation. |
| [[PHASES]] | generated | Suggested implementation order and phase gates. | Continue Phase 5 runtime backend fallback work. |
| [[ARCHITECTURE]] | generated | Qt/OpenCV boundary, data model, algorithm stack model, and required specs. | Keep backend selection and fallback rules current. |
| [[DECISIONS]] | generated | Decisions extracted from current plan notes and existing code behavior. | Add decisions as migration behavior changes. |
| [[REVIEW]] | generated | Parity, QA, benchmark, and regression policy. | Turn acceptance cases into tests/manual checks. |
| [[USER]] | generated | Local environment and user-specific constraints. | Fill missing hardware/OpenCV build facts. |

## Current Status

- Phase 1 is verified with an `ImageBuffer` / `cv::Mat` bridge.
- Phase 2 CPU migration is implemented in `ImageProcessorCore`.
- Phase 3 still-image load/save is implemented in `ImageIoService` with OpenCV
  `imgcodecs`.
- Algorithm IDs `1` through `28` currently use OpenCV-backed CPU paths.
- Rotate remains geometry-materializing, always outputs RGBA, and keeps
  transparent borders `(0, 0, 0, 0)` for later stack operations.
- Blur, sharpen, motion blur, emboss, and median smoothing process RGB only,
  preserve source alpha, and clear RGB for fully transparent pixels after the
  operation.
- Sobel edge, Laplacian, DoG, and endpoint detection also process RGB-derived
  data only, preserve source alpha, and clear RGB for fully transparent pixels.
- Statistics-dependent algorithms consume CPU-computed `stat_*` parameters and
  apply them through OpenCV mask/LUT operations while preserving alpha.
- Still-image I/O uses OpenCV `cv::imread` and `cv::imwrite`; the public
  boundary remains RGB/RGBA `ImageBuffer`.
- Image loads accept 8-bit grayscale, BGR, and BGRA decoded images, expanding
  grayscale to RGB and converting BGR/BGRA to RGB/RGBA.
- Image saves reject invalid or empty buffers, convert RGB/RGBA to BGR/BGRA for
  OpenCV, and drop alpha for JPEG output.
- Video file playback uses OpenCV `cv::VideoCapture` inside
  `VideoInputService` while preserving RGB/RGBA `ImageBuffer` output and the
  existing controller API.
- Video file playback has automated coverage for generated-file metadata,
  BGR-to-RGB conversion, stepping, seek-to-start delivery, timer playback,
  loop-disabled EOF, loop-enabled restart, and invalid speed fallback.
- Stream acquisition now uses a separate OpenCV `cv::VideoCapture` producer
  path in `VideoInputService` while preserving the existing latest-frame mutex
  handoff, display timer, reconnect timer, and `StreamStatus` surface.
- Stream open configures `CAP_PROP_OPEN_TIMEOUT_MSEC=3000` and
  `CAP_PROP_READ_TIMEOUT_MSEC=1000`. Non-network/local sources fall back to
  plain OpenCV open when timeout-parameter open fails.
- Local generated-AVI stream coverage verifies OpenCV producer frame delivery
  plus `Connected` and `Disconnected` stream statuses. Missing stream coverage
  verifies error reporting.
- RTSP/HTTP target stream timeout, reconnect, and error-status validation still
  need real target-environment coverage.
- The current public image boundary is `ImageBuffer`.
- OpenCV 4.13.0 is found in the `videoprocess` Conda environment for `core`,
  `imgproc`, `imgcodecs`, and `videoio`.
- Current GPU processing uses `GpuEffectPipeline` for a subset of algorithms.
- Current video stack optimization uses a CPU prefix plus GPU suffix model.
- `ProcessingBackend` now owns accelerated support reporting, fused-stack
  eligibility, and CPU-prefix/accelerated-suffix planning for video stacks.
- `ProcessingBackend::defaultKindFromEnvironment()` reads
  `QT_UI_PROCESSING_BACKEND` during controller construction. Values `cpu`,
  `cpu-reference`, and `cpureference` force `CpuReference`. Empty, unset,
  invalid, `opengl`, and `gl` follow the runtime accelerated availability
  probe: unavailable selects `CpuReference`, otherwise the default remains
  `OpenGl`.
- `ProcessingBackend` exposes runtime accelerated availability probe state via
  `setRuntimeAcceleratedBackendAvailable(bool)`,
  `clearRuntimeAcceleratedBackendAvailability()`, and
  `runtimeAcceleratedBackendAvailable()`.
- `ProcessingBackend::acceleratedBackendAvailableForGraphicsApi()` maps Qt
  scene-graph graphics APIs to accelerated backend availability. `Unknown`,
  `OpenGL`, and `OpenGLRhi` are available; `Software`, `OpenVG`,
  `Direct3D11`, `Direct3D12`, `Vulkan`, `Metal`, and `Null` are unavailable.
- `main.cpp` records the startup probe after `QGuiApplication` construction by
  checking `QQuickWindow::graphicsApi()` through the backend helper.
- `qt_ui --startup-check` is a hidden smoke-check path that creates
  `QGuiApplication`, records the startup accelerated runtime probe, verifies
  QML module loading, and exits with `0` before entering the event loop.
- CTest `qt_ui_cpu_reference_startup` runs the startup smoke with
  `QT_QPA_PLATFORM=offscreen`, `QT_UI_PROCESSING_BACKEND=cpu-reference`, and
  `QSG_RHI_BACKEND=software`.
- CPU-applied effect-stack entries compute `stat_*` parameters from the current
  prefix image before applying statistics-dependent algorithms, preserving the
  CPU-statistics contract for hybrid CPU/GPU stacks.
- Static-image accelerated apply requests now record the source output version
  used for dispatch. Late accelerated results are ignored if the controller
  source/output changed before completion.
- Static-image accelerated failures fall back to the CPU reference path when
  the pending request still matches the current output image.
- `ProcessingController` can now be switched to
  `ProcessingBackend::Kind::CpuReference` through a non-QML API. In that mode,
  GPU-supported static algorithms apply synchronously through the CPU reference
  path and video stacks produce no accelerated suffix.
- `ProcessingController::markAcceleratedBackendUnavailable()` now marks the
  accelerated backend unavailable by switching to `CpuReference`. Current
  static accelerated failure/cancel paths CPU-fallback first, then downgrade
  future static and video planning to the CPU reference backend.
- `ProcessingViewportItem` reports video GPU suffix apply failures back to the
  controller, causing the same `CpuReference` downgrade so later frames stop
  planning an accelerated suffix.
- `tst_ProcessingController::testVideoGpuSuffixFailureNotificationDisablesVideoGpu`
  now covers the video failure-notification path with a generated MJPG AVI,
  brightness as an accelerated suffix candidate, direct viewport failure
  notification, backend downgrade, empty GPU suffix, and CPU-applied frame
  output after downgrade.
- `.env.example` and root `README.md` document
  `QT_UI_PROCESSING_BACKEND=cpu-reference` as the startup override for forcing
  CPU reference processing.
- Plan documents are intended to be tracked through the `.gitignore`
  exception for `plan/*.md`.

## Verification Evidence

- 2026-06-06: `cmake --build build` passed after OpenCV bridge integration.
- 2026-06-06: `ctest --test-dir build --output-on-failure` passed with
  `tst_ImageProcessorCore`, `tst_OpenCvImageBridge`, `tst_ImageIoService`, and
  `tst_ProcessingController`.
- 2026-06-06: `git diff --check` and `git diff --cached --check` passed for
  the CPU point/grayscale and geometry migration commits.
- 2026-06-06: `cmake --build build` and
  `ctest --test-dir build --output-on-failure` passed after migrating CPU
  blur, sharpen, emboss, motion blur, and median smoothing paths to OpenCV.
- 2026-06-06: edge, DoG, and endpoint CPU algorithms were migrated to OpenCV
  and covered with alpha-preservation and post-rotate transparent-background
  tests. `cmake --build build` and
  `ctest --test-dir build --output-on-failure` passed after the migration.
- 2026-06-06: statistics-dependent CPU algorithms were migrated to OpenCV
  while preserving CPU-computed `stat_*` parameter semantics and flat-range
  copy behavior. `cmake --build build` and
  `ctest --test-dir build --output-on-failure` passed after the migration.
- 2026-06-06: still-image load/save was migrated from QImage to OpenCV
  `imgcodecs` while preserving RGB/RGBA `ImageBuffer` color order, PNG alpha
  round trips, and failure behavior. `cmake --build build` and
  `ctest --test-dir build --output-on-failure` passed after the migration.
- 2026-06-06: video file playback was migrated from direct FFmpeg decode to
  OpenCV `cv::VideoCapture`, with FFmpeg retained for real-time stream
  producer/reconnect behavior. `tst_VideoInputService` covers generated AVI
  open, metadata, RGB frame conversion, EOF stepping, and missing-file errors.
  `cmake --build build` and `ctest --test-dir build --output-on-failure`
  passed after the migration.
- 2026-06-06: OpenCV-backed video file playback coverage was extended for
  seek-to-start delivery, timer play/pause, loop-disabled EOF finish,
  loop-enabled restart, and invalid speed fallback. `cmake --build build` and
  `ctest --test-dir build --output-on-failure` passed after the coverage
  update.
- 2026-06-06: Phase 5 backend planning started with `ProcessingBackend`,
  centralizing the current OpenGL support matrix, fused-stack eligibility, and
  CPU-prefix/accelerated-suffix splitting. CPU prefix application now computes
  per-frame `stat_*` parameters for statistics-dependent effects before
  dispatching OpenCV CPU algorithms. `cmake --build build` and
  `ctest --test-dir build --output-on-failure` passed after the change.
- 2026-06-07: Static-image accelerated apply state now tracks the requested
  output version, rejects stale completions after source/output changes, and
  falls back to the OpenCV CPU reference implementation on accelerated failure
  when the request is still current. `.codex/hooks/run-in-conda.sh cmake
  --build build` and `.codex/hooks/run-in-conda.sh ctest --test-dir build
  --output-on-failure` passed after the change.
- 2026-06-07: Controller-level backend selection was added for the CPU
  reference path. `tst_ProcessingController` covers CPU-reference planning with
  no accelerated suffix and static CPU-only apply for a GPU-supported
  algorithm. `.codex/hooks/run-in-conda.sh cmake --build build` and
  `.codex/hooks/run-in-conda.sh ctest --test-dir build --output-on-failure`
  passed after the change.
- 2026-06-07: Accelerated backend failure now degrades the controller to the
  CPU reference backend. `tst_ProcessingController::testGpuFailureFallsBackToCpuReference`
  covers static GPU failure CPU fallback, backend downgrade to
  `ProcessingBackend::Kind::CpuReference`, and a later GPU-supported algorithm
  applying through CPU without emitting new GPU work. `.codex/hooks/run-in-conda.sh
  cmake --build build` passed, `.codex/hooks/run-in-conda.sh ctest --test-dir
  build --output-on-failure` passed with `5/5` tests, and `git diff --check`
  passed.
- 2026-06-07: Video GPU suffix failure notification now has automated
  controller coverage. `tst_ProcessingController::testVideoGpuSuffixFailureNotificationDisablesVideoGpu`
  creates a temporary MJPG AVI, opens it through `ProcessingController::openVideo()`,
  verifies brightness is planned as a video GPU suffix, calls
  `ProcessingViewportItem::notifyGpuPipelineFailure()`, and verifies downgrade
  to `ProcessingBackend::Kind::CpuReference`, disabled video GPU usage, empty
  suffix, and CPU-applied current-frame output. `.codex/hooks/run-in-conda.sh
  cmake --build build` passed with only the existing Qt policy and
  `WrapVulkanHeaders` warnings, `.codex/hooks/run-in-conda.sh ctest --test-dir
  build --output-on-failure` passed with `5/5` tests, and `git diff --check`
  passed.
- 2026-06-07: Initial backend environment override was added through
  `ProcessingBackend::defaultKindFromEnvironment()` and
  `QT_UI_PROCESSING_BACKEND`. `tst_ProcessingController::testProcessingBackendEnvironmentSelectsCpuReference`
  uses an environment RAII guard with `QT_UI_PROCESSING_BACKEND=cpu-reference`,
  constructs a controller, verifies the initial backend is
  `ProcessingBackend::Kind::CpuReference`, and confirms brightness applies
  synchronously through CPU with no GPU pending state and normal history.
  `.codex/hooks/run-in-conda.sh cmake --build build` passed,
  `.codex/hooks/run-in-conda.sh ctest --test-dir build --output-on-failure`
  passed with `5/5` tests, and `git diff --check` passed.
- 2026-06-07: Initial runtime accelerated backend availability probing was
  added. `main.cpp` records Qt Quick graphics API availability after
  `QGuiApplication` construction, and `ProcessingBackend::defaultKindFromEnvironment()`
  now combines CPU env hard override with the runtime probe. `tst_ProcessingController::testProcessingBackendRuntimeProbeSelectsCpuReference`
  covers empty env plus runtime unavailable selecting `CpuReference` and
  applying brightness synchronously on CPU with no GPU pending state.
  `tst_ProcessingController::testProcessingBackendEnvironmentDisablesVideoGpuSuffix`
  covers `QT_UI_PROCESSING_BACKEND=cpu-reference` keeping video brightness off
  the GPU suffix even when runtime acceleration is marked available. The
  existing environment override test now guards runtime availability to confirm
  the CPU env override is independent of the probe. `.codex/hooks/run-in-conda.sh
  cmake --build build` passed, `.codex/hooks/run-in-conda.sh ctest --test-dir
  build --output-on-failure` passed with `5/5` tests, and `git diff --check`
  passed.
- 2026-06-07: CPU-reference startup smoke coverage was added. `ProcessingBackend::acceleratedBackendAvailableForGraphicsApi()`
  fixes the Qt graphics API availability matrix in unit tests:
  `Unknown`/`OpenGL`/`OpenGLRhi` are available, while `Software`, `OpenVG`,
  `Direct3D11`, `Direct3D12`, `Vulkan`, `Metal`, and `Null` are unavailable.
  `main.cpp` now uses that helper for the startup probe and supports hidden
  `--startup-check`, which creates `QGuiApplication`, records the runtime
  probe, verifies QML module loading, and returns `0` before the event loop.
  CTest `qt_ui_cpu_reference_startup` runs `qt_ui --startup-check` with
  `QT_QPA_PLATFORM=offscreen`, `QT_UI_PROCESSING_BACKEND=cpu-reference`, and
  `QSG_RHI_BACKEND=software`. `tst_ProcessingController` also covers the
  graphics API availability matrix and env/probe priority. `.codex/hooks/run-in-conda.sh
  cmake --build build` passed, `.codex/hooks/run-in-conda.sh ctest --test-dir
  build --output-on-failure` passed with `6/6` tests, and `git diff --check`
  passed.
- 2026-06-07: Direct FFmpeg stream open/decode was removed from
  `VideoInputService` and stream acquisition moved to OpenCV `cv::VideoCapture`
  through a dedicated stream capture path. Local generated-AVI stream tests
  cover OpenCV producer frame delivery, `Connected`/`Disconnected` status
  transitions, and missing-stream errors while retaining the existing
  latest-frame handoff, display timer, reconnect timer, and `StreamStatus`
  surface. `.codex/hooks/run-in-conda.sh cmake --build build` passed and
  `.codex/hooks/run-in-conda.sh ctest --test-dir build --output-on-failure`
  passed. Direct FFmpeg build dependencies and `x264_compat` references remain
  for Phase 6 cleanup.

## Source Evidence

- Root bootstrap: `plan.md`
- Current controller stack behavior: `src/ProcessingController.cpp`
- Current CPU algorithm behavior: `src/ImageProcessorCore.cpp`
- Current OpenCV bridge behavior: `src/OpenCvImageBridge.cpp`
- Current GPU algorithm behavior: `src/GpuEffectPipeline.cpp`
- Current backend capability and stack planning behavior:
  `src/ProcessingBackend.cpp`
- Current viewport GPU suffix handoff: `src/ProcessingViewportItem.cpp`
- Stack support tests: `tests/tst_ProcessingController.cpp`
- Bridge tests: `tests/tst_OpenCvImageBridge.cpp`
- Image I/O tests: `tests/tst_ImageIoService.cpp`
- Video input tests: `tests/tst_VideoInputService.cpp`
- Prior local commits inspected:
  - `07906a5 Accelerate video effect stacks`
  - `fc7686b Reduce median and hybrid stack bottlenecks`

## Open Questions

- Which RTSP/HTTP target streams still need timeout, reconnect, and status
  validation after moving stream acquisition to OpenCV `cv::VideoCapture`?
- When should the remaining direct FFmpeg build dependencies and
  `x264_compat` documentation be removed?
- Which OpenCV acceleration profile is expected in the target environment:
  CPU-only, OpenCL/UMat, CUDA, or multiple runtime-selectable backends?
- Should initial backend selection add a user-visible setting beyond the
  current environment override, startup Qt Quick graphics API probe, and
  failure-driven downgrade to `CpuReference`?
- Which target hardware and platform combinations still need validation beyond
  the offscreen CPU-reference startup smoke for CPU-only or non-OpenGL Qt Quick
  graphics APIs?
- What are the target performance budgets for static images, video files, and
  real-time streams?
