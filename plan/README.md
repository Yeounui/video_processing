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
| [[PHASES]] | generated | Suggested implementation order and phase gates. | Start Phase 2 CPU algorithm migration. |
| [[ARCHITECTURE]] | generated | Qt/OpenCV boundary, data model, algorithm stack model, and required specs. | Resolve open questions before code changes. |
| [[DECISIONS]] | generated | Decisions extracted from current plan notes and existing code behavior. | Add decisions as migration behavior changes. |
| [[REVIEW]] | generated | Parity, QA, benchmark, and regression policy. | Turn acceptance cases into tests/manual checks. |
| [[USER]] | generated | Local environment and user-specific constraints. | Fill missing hardware/OpenCV build facts. |

## Current Status

- Phase 1 implementation has started with an `ImageBuffer` / `cv::Mat` bridge.
- The current public image boundary is `ImageBuffer`.
- OpenCV 4.13.0 is found in the `videoprocess` Conda environment for `core`
  and `imgproc`.
- Current CPU processing uses `ImageProcessorCore` and 28 algorithm IDs.
- Current GPU processing uses `GpuEffectPipeline` for a subset of algorithms.
- Current video stack optimization uses a CPU prefix plus GPU suffix model.
- Plan documents are intended to be tracked through the `.gitignore`
  exception for `plan/*.md`.

## Source Evidence

- Root bootstrap: `plan.md`
- Current controller stack behavior: `src/ProcessingController.cpp`
- Current CPU algorithm behavior: `src/ImageProcessorCore.cpp`
- Current OpenCV bridge behavior: `src/OpenCvImageBridge.cpp`
- Current GPU algorithm behavior: `src/GpuEffectPipeline.cpp`
- Current viewport GPU suffix handoff: `src/ProcessingViewportItem.cpp`
- Stack support tests: `tests/tst_ProcessingController.cpp`
- Bridge tests: `tests/tst_OpenCvImageBridge.cpp`
- Prior local commits inspected:
  - `07906a5 Accelerate video effect stacks`
  - `fc7686b Reduce median and hybrid stack bottlenecks`

## Open Questions

- Should direct FFmpeg remain as a fallback if `cv::VideoCapture` does not
  satisfy stream timeout, reconnect, or metadata requirements?
- Which OpenCV acceleration profile is expected in the target environment:
  CPU-only, OpenCL/UMat, CUDA, or multiple runtime-selectable backends?
- What are the target performance budgets for static images, video files, and
  real-time streams?
