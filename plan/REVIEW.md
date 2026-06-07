# Review And Acceptance Policy

## Review Goals

The OpenCV migration is accepted by behavior, not by compilation alone.

Review must confirm:

- Existing public APIs remain stable unless a decision explicitly changes them.
- Existing unit tests pass after every phase that claims parity.
- OpenCV CPU behavior matches the current implementation or an approved
  tolerance.
- Accelerated behavior matches the CPU reference or an approved tolerance.
- Static images, video files, and streams preserve controller and viewport
  behavior.

## Required Automated Checks

Use checks directly tied to the migrated phase:

- Build the project.
- Run existing CTest/unit tests.
- Add or update focused tests for each migrated algorithm group.
- Add bridge tests for RGB/RGBA, empty buffers, changed dimensions, and invalid
  layouts.
- Add I/O round-trip tests for supported formats.
- Add controller tests for video effect stack prefix/suffix behavior.

Do not mark a phase as verified without recording the exact passing command and
result in [[README]] or the relevant phase note.

## Algorithm Parity Policy

For each algorithm, review:

- Output width and height.
- Output channel count.
- Parameter default behavior.
- Rounding and saturation.
- Border handling.
- Alpha preservation or cleanup.
- Static image result tolerance.
- Video stack result tolerance.

Byte-for-byte parity is preferred where practical. Tolerance is acceptable only
when OpenCV's documented behavior differs in rounding, interpolation, or border
handling and the visual behavior is approved.

## Rotate, Flip, And Transparency Regression Cases

These cases are mandatory because they have caused repeated background-color
regressions:

- `Rotate -> Brightness`: transparent corners remain transparent.
- `Rotate -> Grayscale`: transparent corners remain transparent.
- `Rotate -> Median`: background does not gain visible color.
- `Rotate -> Blur`: background does not gain visible color.
- `Rotate -> Sharpen`: background does not gain visible color.
- Repeated rotate does not grow the transparent canvas indefinitely.
- RGB input rotated output is RGBA with transparent outside pixels.
- RGBA input rotation bounds are based on non-transparent alpha content.
- `Flip -> Brightness -> Grayscale` accelerated/fused result matches CPU
  tolerance.
- `Rotate -> Sharpen` keeps rotate in the CPU prefix and allows sharpen in an
  accelerated suffix only if supported.

Current automated coverage:

- `tst_ImageProcessorCore` covers RGB rotate output as RGBA, transparent
  rotate borders, alpha-content rotate bounds, repeated rotate bounds, OpenCV
  flip alpha preservation, `Rotate -> Brightness` transparent-background
  cleanup, OpenCV filter alpha preservation, `Rotate -> Blur`
  transparent-background cleanup, OpenCV edge/DoG/endpoint alpha preservation,
  `Rotate -> Edge` transparent-background cleanup, OpenCV statistics alpha
  preservation, and flat-range statistics copy behavior with transparent RGB
  cleanup.
- `tst_ImageIoService` covers OpenCV-backed RGB PNG load color order,
  transparent PNG alpha preservation, RGB and RGBA PNG save/load round trips,
  missing-file load failure, and invalid-buffer save failure.
- `tst_VideoInputService` covers OpenCV-backed generated AVI open, file
  metadata, BGR-to-RGB frame conversion, seek-to-start frame delivery, EOF
  stepping behavior, timer playback stop at loop-disabled EOF, loop-enabled
  restart, invalid playback-speed fallback, and missing-file error reporting.
- `tst_ProcessingController` covers the backend suffix planner around rotate
  and statistics-dependent CPU-only algorithms, the current fused GPU support
  matrix, CPU effect-stack statistics computed from the current source image
  rather than the default statistic values, CPU fallback after static-image
  accelerated failure, accelerated-backend downgrade to `CpuReference` after
  failure, later CPU-only apply for GPU-supported algorithms, video GPU suffix
  failure notification with current-frame CPU application after downgrade, and
  stale accelerated-result rejection after source changes.

## Video And Stream Acceptance

Video file behavior must cover:

- Open file.
- First frame display.
- Play and pause.
- Seek.
- Step.
- Loop.
- Playback speed.
- End-of-file.
- Metadata fallback when fps, duration, or frame count is unavailable.

Current automated coverage:

- Generated local AVI open, metadata, step, RGB frame conversion, EOF stepping,
  seek-to-start delivery, timer play/pause state, loop-disabled EOF finish,
  loop-enabled restart, invalid playback-speed fallback, and missing-file
  errors are covered.
- Seek precision across codecs, exact playback-speed timing tolerance, and
  missing-metadata fallback still need targeted automated or manual coverage
  before Phase 4 can be marked fully verified.

Real-time stream behavior must cover:

- Open stream.
- Latest-frame display.
- Disconnect.
- Reconnect.
- Timeout.
- Error reporting.
- Slow-processing frame drop policy.

## Acceleration Acceptance

Acceleration review must cover:

- CPU-only application startup.
- Runtime capability detection.
- Per-algorithm support reporting.
- CPU fallback when an accelerated operation fails.
- Hybrid CPU prefix plus accelerated suffix stacks.
- Statistics-dependent CPU prefix effects compute `stat_*` from the current
  prefix image before any accelerated suffix runs.
- Fully accelerated display-time stacks with no unnecessary readback when the
  architecture supports it.
- Stale result rejection when source image/version changes mid-flight.

Current automated coverage:

- Static-image GPU failure is simulated through the controller API and must
  apply the same algorithm through the CPU reference path with normal history,
  downgrade the selected backend to `CpuReference`, and keep later
  GPU-supported algorithms on the CPU reference path.
- Static-image stale GPU completion is simulated after opening a different
  source and must not overwrite the new source or append history.
- CPU-reference backend selection is simulated through the controller API and
  must bypass static accelerated dispatch for algorithms that the OpenGL path
  otherwise supports.
- Environment-forced CPU-reference startup is covered by
  `tst_ProcessingController::testProcessingBackendEnvironmentSelectsCpuReference`:
  an environment RAII guard sets `QT_UI_PROCESSING_BACKEND=cpu-reference`,
  controller construction selects `CpuReference`, and brightness applies
  through CPU with no GPU pending state and normal history.
- Runtime unavailable startup probing is covered by
  `tst_ProcessingController::testProcessingBackendRuntimeProbeSelectsCpuReference`:
  with an empty backend env and runtime acceleration marked unavailable,
  controller construction selects `CpuReference` and brightness applies through
  CPU with no GPU pending state.
- Qt graphics API availability and env/probe priority are covered in
  `tst_ProcessingController`. `Unknown`, `OpenGL`, and `OpenGLRhi` are locked
  as accelerated-runtime available, while `Software`, `OpenVG`, `Direct3D11`,
  `Direct3D12`, `Vulkan`, `Metal`, and `Null` are locked as unavailable.
- Environment-forced CPU-reference video planning is covered by
  `tst_ProcessingController::testProcessingBackendEnvironmentDisablesVideoGpuSuffix`:
  even with runtime acceleration marked available, `QT_UI_PROCESSING_BACKEND=cpu-reference`
  prevents a video brightness effect from producing a GPU suffix and applies
  the result through CPU.
- CPU-reference application startup smoke is covered by CTest
  `qt_ui_cpu_reference_startup`, which runs `qt_ui --startup-check` with
  `QT_QPA_PLATFORM=offscreen`, `QT_UI_PROCESSING_BACKEND=cpu-reference`, and
  `QSG_RHI_BACKEND=software`.
- CPU-reference video-stack planning must produce a full CPU prefix and no
  accelerated suffix.
- Video GPU suffix apply failure handling is covered by
  `tst_ProcessingController::testVideoGpuSuffixFailureNotificationDisablesVideoGpu`:
  a generated MJPG AVI initially plans brightness as a GPU suffix, direct
  viewport failure notification downgrades the controller to `CpuReference`,
  video GPU use is disabled, the suffix is empty, and the current frame reflects
  the CPU-applied result.

## Manual QA

Manual QA should include:

- Load and save representative RGB and RGBA images.
- Apply each algorithm individually to a still image.
- Apply maximum-size stacks to video and stream sources.
- Compare original/output split view after migration.
- Exercise zoom, pan, split compare, and original/output switching.
- Confirm transparent rotated corners composite correctly over the viewport
  background.
- Validate startup and rendering on representative target hardware/platform
  combinations beyond the offscreen CPU-reference smoke, especially CPU-only
  and explicit non-OpenGL Qt Quick graphics APIs.

## Document Audit

After editing plan documents:

- Confirm `plan/README.md` lists every created `plan/*.md` file.
- Confirm Markdown links point to existing or intentionally future documents.
- Search for stale paths or old document names.
- Keep status terms to `stub exists`, `draft written`, `generated`, or
  `verified`.
