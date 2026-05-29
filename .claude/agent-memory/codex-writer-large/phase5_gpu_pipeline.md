---
name: phase5_gpu_pipeline_implementation
description: Phase 5 (GLSL effect pipeline) successfully implemented with 16 GPU-accelerated algorithms
metadata:
  type: project
---

## Summary

Phase 5 (GLSL effect pipeline) completed successfully. GPU shader support added for 16 image processing algorithms while maintaining CPU fallback for all 28 algorithms.

## Implementation Details

**New Module: GpuEffectPipeline**
- Header: `include/GpuEffectPipeline.h`
- Implementation: `src/GpuEffectPipeline.cpp`
- Manages OpenGL 3.3 Core FBO, textures, and shader programs
- Supports 16 algorithms via GLSL fragment shaders
- Handles both RGB and RGBA input (converts RGBA to RGB on upload)
- Synchronous glReadPixels readback to CPU

**GPU-Supported Algorithms**
IDs: 1, 2, 3, 4, 6, 7, 9, 11, 12, 14, 19, 20, 21, 26, 27, 28
- Brightness, Multiply, Gamma, Fixed Threshold
- Bitwise AND, Flip
- Emboss, 3x3 Blur, 5x5 Blur, Sharpen
- Horizontal Edge, Vertical Edge, Laplacian
- Grayscale (Average, Luminosity, Lightness)

**CPU-Only Algorithms**
- #8 Rotate (no GPU, returns RGBA, used as pre-processing)
- #5, #10, #23 (stats-dependent, require CPU pre-computation)

## Threading Architecture

**Signal/Slot Flow:**
1. Main thread: `applyAlgorithm()` → checks `supportsAlgorithm()`
2. Main thread: emit `pendingGpuApply(src, algorithmId, params)` + set `gpuApplyPending_=true`
3. Main thread: `onPendingGpuApply()` captures command in `pendingGpuCmd_`, calls `update()`
4. Sync phase: `updatePaintNode()` transfers `pendingGpuCmd_` to snapshot, calls `setCallbackItem(this)`
5. Render thread: `ViewportRenderNode::render()` processes GPU command before display render
6. Render thread: `QMetaObject::invokeMethod(callbackItem_, lambda, Qt::QueuedConnection)` returns result
7. Main thread: `deliverGpuResult()` calls `commitGpuResult()` or `cancelGpuApply()`

**No Mutex Required:** GUI thread is blocked during sync phase (beginExternalCommands → endExternalCommands), so `pendingGpuCmd_` access is safe.

## Key Design Decisions

**Why synchronous glReadPixels?** Stretch goal allows async PBO readback later. Current cut uses simple synchronous path with shared_ptr memory management.

**Why lambda capture instead of Q_ARG?** Avoids need to register ImageBuffer type in Qt metatype system. Lambda captures `std::shared_ptr<ImageBuffer>` directly.

**Why plain struct for PendingGpuCommand?** No Q_OBJECT needed; simple data pod passed between threads. Reduces complexity.

**Shader Compilation Error Handling:** Shaders cached per algorithmId. Compile failure marks program as failed, returning nullptr from `applyStaticEffect()`, triggering `cancelGpuApply()` on main thread.

## Testing

All 3 test suites pass:
- tst_ImageProcessorCore: 28 CPU algorithms ✓
- tst_ImageIoService: Image IO (JPEG, PNG, WEBP) ✓
- tst_ProcessingController: History, undo/redo, algorithm dispatch ✓

CPU and GPU paths tested indirectly via controller tests. Full render path tested at runtime (not unit tested).

## Build Status

- CMake: ✓ (added GpuEffectPipeline.h/cpp to qt_add_qml_module and tst_ProcessingController)
- Qt6::OpenGL linked to tst_ProcessingController for OpenGL headers
- Binary: 9.4 MB

## Files Changed

**Created:**
- include/GpuEffectPipeline.h (42 lines)
- src/GpuEffectPipeline.cpp (685 lines, all 16 fragment shaders embedded)

**Modified:**
- include/ProcessingController.h: +3 public methods, +1 signal, +2 private members
- src/ProcessingController.cpp: applyAlgorithm() refactored + commitGpuResult/cancelGpuApply
- include/ProcessingViewportItem.h: +PendingGpuCommand struct, +2 GPU methods
- src/ProcessingViewportItem.cpp: GPU signal connection, snapshot transfer, render dispatch
- CMakeLists.txt: GpuEffectPipeline sources + Qt6::OpenGL link

**Commit:** c41bee5 Phase 5: GLSL effect pipeline (16 GPU-accelerated algorithms)

## Next Steps

Phase 6: Video file input (pending).
