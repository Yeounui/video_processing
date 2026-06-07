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
- Real-time streams still use the existing FFmpeg producer/reconnect path
  pending timeout and reconnect parity review.
- The current public image boundary is `ImageBuffer`.
- OpenCV 4.13.0 is found in the `videoprocess` Conda environment for `core`,
  `imgproc`, `imgcodecs`, and `videoio`.
- Current GPU processing uses `GpuEffectPipeline` for a subset of algorithms.
- Current video stack optimization uses a CPU prefix plus GPU suffix model.
- `ProcessingBackend` now owns accelerated support reporting, fused-stack
  eligibility, and CPU-prefix/accelerated-suffix planning for video stacks.
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

- Should direct FFmpeg remain as a fallback if `cv::VideoCapture` does not
  satisfy stream timeout, reconnect, or metadata requirements?
- Which OpenCV acceleration profile is expected in the target environment:
  CPU-only, OpenCL/UMat, CUDA, or multiple runtime-selectable backends?
- Should initial backend selection become a user-visible setting, an
  environment override, or an automatic startup probe beyond the current
  failure-driven downgrade to `CpuReference`?
- What are the target performance budgets for static images, video files, and
  real-time streams?
