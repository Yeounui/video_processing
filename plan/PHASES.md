# OpenCV Migration Phases

## Phase 0: Lock Compatibility Specs

Status: verified

Before code changes, record the compatibility contract for data layout, color,
alpha, statistics, algorithm behavior, video source behavior, and acceleration
fallback in [[ARCHITECTURE]].

Exit criteria:

- Each algorithm group has an intended OpenCV replacement policy.
- Rotate, flip, transparent-background, and stack-order behavior are explicitly
  specified.
- Review cases in [[REVIEW]] are accepted as the migration gate.

## Phase 1: Add ImageBuffer/OpenCV Bridge

Status: verified

Introduce a narrow bridge concept between `ImageBuffer` and OpenCV images while
preserving `ImageBuffer` as the public controller and viewport type.

Required behavior:

- Accept only 8-bit 3-channel RGB and 4-channel RGBA buffers at the application
  boundary.
- Create zero-copy `cv::Mat` views only when lifetime, continuity, stride, and
  mutability rules are safe.
- Deep-copy non-contiguous or temporary OpenCV results before returning to
  application-owned buffers.
- Keep BGR/BGRA conversion local to OpenCV entry and exit points.

Exit criteria:

- Bridge tests cover empty buffers, RGB, RGBA, changed dimensions, and invalid
  layouts.
- Existing controller and viewport APIs remain unchanged.

Implementation note:

- `OpenCvImageBridge` provides RGB/RGBA `cv::Mat` views, RGB/RGBA and
  BGR/BGRA copy conversion, invalid-layout rejection, and non-contiguous
  `cv::Mat` deep-copy behavior.
- Verified with `tst_OpenCvImageBridge`, `tst_ImageProcessorCore`,
  `tst_ImageIoService`, and `tst_ProcessingController`.

## Phase 2: Migrate CPU Algorithms

Status: generated

Replace hand-written CPU algorithms with OpenCV-backed implementations in small
groups while keeping algorithm IDs, names, parameter keys, defaults, and output
contracts stable.

Suggested order:

1. Point and grayscale operations. Implemented for IDs `1`, `2`, `3`, `4`,
   `6`, `26`, `27`, and `28`.
2. Flip and rotate geometry. Implemented for IDs `7` and `8`.
3. Blur, sharpen, smoothing, and morphology. Implemented for IDs `9`, `11`,
   `12`, `13`, `14`, `15`, `16`, `17`, `18`, and `25`.
4. Edge and frequency-like filters. Implemented for IDs `19`, `20`, `21`,
   `22`, and `24`.
5. Statistics-dependent algorithms. Implemented for IDs `5`, `10`, and `23`.

Exit criteria:

- Each migrated group has parity tests or documented tolerances.
- Transparent pixels remain transparent after post-rotate stacks.
- Repeated rotate does not grow the transparent canvas indefinitely.

Implementation note:

- Point and grayscale paths use OpenCV `cv::Mat` operations while preserving
  alpha and clearing RGB for fully transparent pixels.
- Flip uses `cv::flip` and remains dimension-preserving.
- Rotate uses `cv::warpAffine` with the existing alpha-content bounds policy,
  nearest-neighbor sampling, RGBA output, and transparent constant borders.
- Emboss, box blur, Gaussian blur, sharpen, high-pass sharpen, high-boost,
  diagonal motion blur, horizontal motion blur, and median smoothing use
  OpenCV CPU paths over RGB channels with alpha preserved from the input.
- Filter border handling uses OpenCV replicate borders to match the prior
  clamped-edge sampling contract.
- Sobel edge, Laplacian, DoG, and endpoint detection use OpenCV CPU paths while
  preserving the same alpha policy as other non-geometry filters.
- Endpoint detection keeps out-of-image neighbors as background by padding the
  foreground mask with a constant zero border before counting neighbors.
- Average threshold uses OpenCV luminance comparison with the CPU-computed
  `stat_average` parameter.
- Contrast stretch and histogram stretch use OpenCV LUT application with the
  CPU-computed `stat_min/stat_max` and `stat_hmin/stat_hmax` parameters,
  preserving the existing flat-range copy behavior.

## Phase 3: Migrate Image I/O

Status: generated

Move still-image load/save behind OpenCV `imgcodecs` while preserving the
application's RGB/RGBA `ImageBuffer` contract.

Exit criteria:

- Load/save round trips preserve dimensions, channel count, and expected color
  order for supported formats.
- Unsupported or failed loads report errors without changing controller state.
- Metadata/EXIF orientation behavior is either implemented or explicitly out of
  scope.

## Phase 4: Migrate Video Capture

Status: generated

Replace direct FFmpeg video acquisition with `cv::VideoCapture` only if it
satisfies current file and stream behavior.

Exit criteria:

- File playback supports open, first frame, play, pause, seek, step, loop,
  speed, and EOF handling.
- Streams support open, latest-frame display, disconnect, reconnect, timeout,
  and error reporting.
- Missing metadata has defined fallback behavior.
- Frame dropping policy under processing load is preserved or explicitly
  revised.

## Phase 5: Introduce Acceleration Backend

Status: generated

Generalize the current GPU processing path into an acceleration backend that can
choose CPU, OpenCL/UMat, CUDA, or another backend without exposing OpenCV types
to QML.

Required behavior:

- CPU remains the reference backend.
- Accelerated results are tolerance-compatible with CPU results.
- Acceleration is runtime-detected and optional.
- Video effect stacks preserve the CPU prefix plus accelerated suffix model.

Exit criteria:

- CPU-only machines run the application and tests.
- Accelerated failures fall back without corrupting controller state.
- Display-only video suffixes avoid unnecessary CPU readback where the backend
  and viewport boundary support it.

## Phase 6: Remove Replaced Dependencies

Status: generated

Remove direct FFmpeg and old GLSL processing dependencies only after the OpenCV
paths meet the previous phase gates.

Exit criteria:

- Build files no longer require removed dependencies.
- No source path still depends on removed headers or shader programs.
- Review checks in [[REVIEW]] pass for static images, video files, streams, and
  accelerated fallback.
