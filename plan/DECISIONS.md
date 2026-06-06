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
