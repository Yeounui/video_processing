# OpenCV Migration Decisions

## D-001: Keep ImageBuffer At Public Boundaries

Status: generated

Decision: keep `ImageBuffer` as the controller, test, and viewport boundary
type during the migration.

Reason: current Qt/QML-facing APIs, viewport upload logic, tests, and controller
state already use `ImageBuffer`. Exposing OpenCV types would broaden the API
break beyond the migration goal.

Consequence: OpenCV integration needs a bridge layer with explicit lifetime,
layout, color, and copy rules.

## D-002: Keep RGB/RGBA As Application Boundary Format

Status: generated

Decision: `ImageBuffer` remains RGB/RGBA. OpenCV BGR/BGRA conversion is local
to bridge, I/O, video, or algorithm functions that require it.

Reason: the existing application and viewport expect RGB/RGBA. Letting BGR leak
across boundaries would create silent color regressions.

Consequence: every OpenCV entry/exit point needs an explicit color conversion
policy.

## D-003: CPU OpenCV Is The Reference Backend

Status: generated

Decision: build the CPU `cv::Mat` path first and treat it as the behavioral
reference.

Reason: OpenCV acceleration availability depends on build modules and hardware.
CPU behavior is the portable baseline needed for tests and fallback.

Consequence: OpenCL/UMat and CUDA paths must match CPU behavior by byte parity
or documented tolerance.

## D-004: Preserve CPU Prefix Plus Accelerated Suffix For Video Stacks

Status: generated

Decision: preserve the current video stack architecture that applies a CPU
prefix and an accelerated suffix.

Reason: prior local commits `07906a5` and `fc7686b` optimized video stacks by
avoiding CPU readback for fully supported display-time stacks and by allowing
hybrid stacks such as CPU rotate followed by GPU filters.

Consequence: the migrated backend should expose per-algorithm support,
fused/support capability, and fallback decisions without assuming every stack is
CPU-only.

## D-005: Rotate Is Geometry-Materializing, Flip Is Coordinate-Mappable

Status: generated

Decision: treat rotate and flip differently in backend planning.

Reason: flip preserves dimensions and can be represented as a sampling
coordinate transform. Rotate changes output dimensions, produces transparent
corners, outputs RGBA, and uses alpha-content bounds to avoid repeated canvas
growth.

Consequence: rotate remains CPU/materialized unless a future backend explicitly
supports geometry-changing outputs and viewport size/version updates.

## D-006: Transparent Pixels Are Protected Across Stacks

Status: generated

Decision: fully transparent pixels must remain `(0, 0, 0, 0)` after every
algorithm, including algorithms applied after rotate or flip.

Reason: rotated images create transparent borders. Later algorithms that modify
transparent RGB values can make the viewport composite unexpected background
colors.

Consequence: CPU paths need alpha-aware processing or post-operation RGB
cleanup, and accelerated paths need transparent-pixel skip or early-return
behavior.

## D-007: cv::VideoCapture Replacement Is Conditional

Status: generated

Decision: replace direct FFmpeg with `cv::VideoCapture` only after file and
stream behavior is proven acceptable in the target environment.

Reason: `VideoCapture` backend support, metadata accuracy, timeout behavior,
and reconnect control can vary by OpenCV build.

Consequence: direct FFmpeg removal is a late-phase dependency cleanup, not an
early migration assumption.

## D-008: Track plan/ Markdown Documents

Status: verified

Decision: track `plan/*.md` documents in git.

Reason: the migration plan documents are intended to guide implementation and
review. Keeping them ignored would make the implementation context incomplete.

Consequence: `.gitignore` keeps the broad `*.md` rule but adds a `plan/*.md`
exception.

## D-009: Introduce OpenCvImageBridge Before Algorithm Migration

Status: verified

Decision: introduce `OpenCvImageBridge` as the first OpenCV code path before
rewriting individual algorithms.

Reason: algorithm, I/O, and video migration all need the same layout, lifetime,
copy, and color-order contract between `ImageBuffer` and OpenCV.

Consequence: future OpenCV-backed code should use the bridge instead of
constructing ad hoc `cv::Mat` wrappers around `ImageBuffer` data.

## D-010: Migrate Alpha-Safe CPU Algorithm Groups First

Status: verified

Decision: migrate simple point, grayscale, and geometry CPU paths before
statistics-dependent filters.

Reason: these algorithms exercise the OpenCV bridge, saturation, rounding,
alpha preservation, and transparent RGB cleanup without also changing automatic
statistics semantics.

Consequence: IDs `1`, `2`, `3`, `4`, `6`, `7`, `8`, `26`, `27`, and `28` are
OpenCV-backed, while IDs `5`, `10`, and `23` remain legacy until the statistics
contract is specified and tested.

## D-011: Preserve Rotate Bounds And Stack Transparency With warpAffine

Status: verified

Decision: use `cv::warpAffine` for arbitrary-angle CPU rotate, but keep the
existing output-size and alpha-content bounds calculation.

Reason: OpenCV should own the sampling operation, while the application still
needs the established rotate contract: geometry-changing RGBA output,
transparent constant borders, and bounded repeated rotations after an RGBA
rotate.

Consequence: later color, grayscale, blur, sharpen, and morphology operations
must continue to treat `alpha == 0` pixels as protected background and clear RGB
after processing.

## D-012: Filter Operations Preserve Alpha While Processing RGB

Status: verified

Decision: OpenCV-backed emboss, blur, Gaussian blur, sharpen, high-pass
sharpen, high-boost, motion blur, and median smoothing operate on RGB channels
only, then write those channels back into the destination buffer while keeping
the copied source alpha.

Reason: the legacy CPU filters did not filter alpha, and post-rotate stacks
depend on transparent pixels remaining transparent after additional algorithms.
Filtering RGBA directly would let convolution or median operations modify alpha
near transparent borders.

Consequence: geometry remains the only current CPU path that intentionally
changes channel count and alpha shape. Non-geometry filters must use replicate
borders for legacy clamped-edge parity and must rely on final transparent-RGB
cleanup before returning.

## D-013: Edge And Endpoint Operations Preserve The Filter Alpha Contract

Status: verified

Decision: OpenCV-backed Sobel edge, Laplacian, DoG, and endpoint detection keep
alpha outside the OpenCV calculation and write only RGB output back into the
destination.

Reason: post-rotate stacks can feed transparent RGBA pixels into edge
algorithms. Running edge or frequency operations over alpha would create new
opaque or color-bearing border artifacts that did not exist in the legacy CPU
path.

Consequence: Sobel and Laplacian use replicate borders for clamped-edge parity,
DoG uses RGB Gaussian blur output as its OpenCV source, and endpoint detection
uses a zero-padded foreground mask so out-of-image neighbors remain background.

## D-014: Statistics Algorithms Consume CPU-Computed Parameters

Status: verified

Decision: keep the existing `stat_average`, `stat_min/stat_max`, and
`stat_hmin/stat_hmax` parameter contract and migrate only the per-pixel CPU
application of those statistics to OpenCV.

Reason: static image and GPU paths already compute statistics on the CPU before
dispatch. Changing the parameter contract would split CPU and GPU behavior and
would make hybrid stacks harder to reason about.

Consequence: average threshold uses an OpenCV luminance comparison, while
contrast stretch and histogram stretch use OpenCV LUT application whose entries
are generated with the legacy arithmetic ordering, `clamp8` rounding, and
saturation policy. Flat statistic ranges copy RGB from the source while
preserving alpha and still clear RGB for fully transparent pixels before
returning.

## D-015: ImageIoService Uses OpenCV imgcodecs

Status: verified

Decision: migrate still-image load/save from QImage to OpenCV `imgcodecs` while
keeping `ImageBuffer` as the controller-facing RGB/RGBA boundary.

Reason: image I/O should use the same OpenCV bridge and color-order contract as
the migrated processing path. This keeps BGR/BGRA conversion local to I/O and
prevents OpenCV channel order from leaking into controller or viewport state.

Consequence: `ImageIoService` decodes through `cv::imread`, converts 8-bit
grayscale/BGR/BGRA inputs to RGB/RGBA `ImageBuffer` output, and encodes through
`cv::imwrite` after converting RGB/RGBA to BGR/BGRA. Invalid buffers, failed
loads, unsupported layouts, and encoder exceptions return `nullptr` or `false`
without changing the public API. PNG RGB/RGBA round trips are covered by tests;
metadata preservation, EXIF auto-orientation, color profiles, and animated
formats remain out of scope for this phase.

## D-016: Video Files Use OpenCV VideoCapture While Streams Retain FFmpeg

Status: verified

Decision: migrate local video file acquisition in `VideoInputService::open()`
to OpenCV `cv::VideoCapture`, but keep real-time stream acquisition on the
existing FFmpeg producer path.

Reason: file playback maps cleanly to OpenCV open/read/seek/metadata behavior
and can be covered by deterministic generated-file tests. Stream behavior still
depends on explicit interrupt, latest-frame handoff, reconnect timer, timeout,
and status transitions that have not been proven equivalent with
`cv::VideoCapture` in the target environment.

Consequence: direct FFmpeg remains a build/runtime dependency until stream
timeout and reconnect parity are either implemented with OpenCV or accepted as a
revised behavior. `VideoInputService` now has two acquisition paths but one
public RGB/RGBA `ImageBuffer` boundary.

## D-017: Centralize Acceleration Capability Planning

Status: verified

Decision: introduce `ProcessingBackend` as the capability and stack-planning
boundary for acceleration.

Reason: `GpuEffectPipeline` should own OpenGL execution details, not the
generic decision of where a video effect stack splits between CPU prefix and
accelerated suffix. The OpenCV migration needs the same planning contract to
support CPU-only, OpenGL, UMat/OpenCL, CUDA, or future runtime-selected
backends.

Consequence: `ProcessingController` asks `ProcessingBackend` for video stack
planning, `GpuEffectPipeline` delegates support reporting to that boundary, and
future accelerated backends must report accelerated support, fused eligibility,
and statistics requirements through the same architectural layer.

## D-018: Compute Stack Statistics At The CPU Prefix Boundary

Status: verified

Decision: compute statistics-dependent algorithm parameters from the current
CPU prefix image immediately before applying IDs `5`, `10`, and `23`.

Reason: static-image and GPU paths use CPU-computed statistics. Video and
stream stacks need the same rule per frame, especially when earlier CPU prefix
effects change the image before a statistics-dependent effect runs.

Consequence: hybrid stacks keep statistics on CPU even when a later suffix runs
on an accelerated backend. If a future backend supports a statistics-dependent
algorithm directly, the backend planner must still arrange CPU statistic
computation before dispatch.

## D-019: Guard Static Accelerated Results With Controller Versions

Status: verified

Decision: static-image accelerated apply records the output image pointer,
output version, algorithm ID, CPU-prepared parameters, and history label at
dispatch time. A later accelerated completion is accepted only if that pending
request still matches the current controller output.

Reason: OpenGL execution completes asynchronously through the viewport render
path. The user can change source, reset, undo, or redo before the queued result
returns, so accepting a late result would corrupt the current controller state.

Consequence: source/output mutations invalidate pending accelerated work.
Accelerated failure falls back to `ImageProcessorCore::apply()` only when the
pending request is still current; stale completions and stale failures are
discarded without changing the new source or appending history.

## D-020: Make CPU Reference A Selectable Backend

Status: verified

Decision: `ProcessingController` owns a selected `ProcessingBackend::Kind`, and
`ProcessingBackend::Kind::CpuReference` is a valid non-QML selection.

Reason: Phase 5 requires CPU-only machines to run without corrupting state or
depending on OpenGL accelerated execution. The CPU path also needs to be
testable independently from runtime OpenGL availability.

Consequence: static-image apply checks accelerated support against the selected
backend before emitting GPU work. Video stack planning uses the same selection,
so CPU-reference mode produces a full CPU prefix and no accelerated suffix.
Changing the backend clears pending accelerated work and re-applies the current
video frame through the newly selected planning path.

## D-021: Downgrade Failed Accelerated Backends To CPU Reference

Status: verified

Decision: when the currently selected accelerated backend fails during static
apply or display-time video suffix apply, report that failure to
`ProcessingController` and downgrade the selected backend to
`ProcessingBackend::Kind::CpuReference`.

Reason: repeatedly dispatching work to a failing accelerated path causes
avoidable failures and can keep video stacks planned around a suffix that the
viewport cannot apply. The CPU OpenCV path is the reference backend and is
already required to preserve state when acceleration is unavailable.

Consequence: static accelerated failure/cancel first falls back to the CPU
reference implementation for the current pending request when it is still
current, then marks the accelerated backend unavailable. Video GPU suffix apply
failure is reported from `ProcessingViewportItem` to the controller and causes
the same downgrade. After downgrade, GPU-supported static algorithms apply
synchronously through CPU and video stack planning produces no accelerated
suffix until a future decision adds re-enable or probing behavior.

## D-022: Allow Environment-Controlled Initial Processing Backend

Status: verified

Decision: `ProcessingBackend::defaultKindFromEnvironment()` reads
`QT_UI_PROCESSING_BACKEND` when `ProcessingController` is constructed. Values
`cpu`, `cpu-reference`, and `cpureference` select
`ProcessingBackend::Kind::CpuReference`; unset, invalid, `opengl`, and `gl`
preserve the existing default `ProcessingBackend::Kind::OpenGl`.

Reason: Phase 5 needs a practical startup override for CPU-only or unreliable
accelerated environments before automatic capability probing and a user-visible
setting are designed. An environment variable is easy to document, test, and
use in CI or local troubleshooting without changing QML-facing APIs.

Consequence: users and tests can force the CPU reference path before any
accelerated dispatch is attempted. The override controls only the initial
controller backend; runtime accelerated failures can still downgrade to
`CpuReference`, and future automatic probing or UI settings need an explicit
precedence decision before changing this startup contract.

## D-023: Probe Initial Accelerated Backend Availability At Startup

Status: verified

Decision: keep `QT_UI_PROCESSING_BACKEND=cpu|cpu-reference|cpureference` as a
hard CPU reference override, but let empty, unset, invalid, `opengl`, and `gl`
values follow process-level runtime accelerated availability before choosing
the initial controller backend. `main.cpp` records that availability after
`QGuiApplication` construction from `QQuickWindow::graphicsApi()`: `Unknown`
and `OpenGL` are treated as accelerated-runtime available, while explicit
non-OpenGL graphics APIs are treated as unavailable.

Reason: the application should avoid dispatching OpenGL accelerated processing
when Qt Quick is explicitly running on a non-OpenGL graphics API, while still
preserving the documented CPU environment override and the existing OpenGL
default for normal or unknown startup conditions.

Consequence: controller construction can start in `CpuReference` without first
attempting and failing accelerated processing when the startup probe marks the
accelerated runtime unavailable. The probe is still a startup selection signal,
not full target-environment validation; CPU-only application startup and
hardware/backend-specific acceptance remain separate Phase 5 evidence.
