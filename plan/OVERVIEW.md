# Project Overview

## Goal

Build a Qt Quick/QML desktop image processing application that unifies UI, window management, event loop, and OpenGL context under Qt6, providing static image, video file, and realtime stream inputs with all 28 image processing algorithms available from a QML inspector.

## In Scope

- Qt6 Core/Gui/Qml/Quick/QuickControls2/OpenGL application shell.
- Static image open, process, preview, apply, reset, save.
- Video file decode (FFmpeg), playback (play/pause/step/seek/speed/loop), per-frame effect stack.
- Realtime stream input (URL or device), latest-frame queue, pause/reconnect/disconnect indicators.
- 28 algorithms implemented as CPU reference and (where applicable) GLSL accelerated paths with `±1`-per-channel parity.
- QML inspector exposing algorithm parameters from `AlgorithmSpec` schema.
- Central `QSGRenderNode`-backed viewport with aspect-preserving fit, zoom, pan, original/result and split compare.
- UI matching `ui.md` palette, layout, and acceptance criteria.

## Out Of Scope (final build)

- SDL2 dependency (window, event loop, GL context, thread/mutex/cond primitives).
- Qt Widgets shell.
- PPM open/save actions in UI and services.
- STB headers as required static image IO dependency.
- Console-driven command input.
- Video/stream output file save.
- `QQuickFramebufferObject` or window-level `beforeRendering`/`afterRendering` hooks for the central viewport.
- OpenGL fixed-function filtering (`GL_LINEAR`, mipmaps) as substitute for algorithm logic or display resampling.

## Implementation Principles

- Structure changes are recorded in `plan/ARCHITECTURE.md` (and underlying `structure.md` source) before code.
- Visual design changes are recorded against `ui.md` source before QML.
- CPU reference implementation comes first; GLSL is enabled only after parity is verified.
- Save scope and effect limits follow the per-mode policy in `plan/ARCHITECTURE.md`.
- The QML viewport uses the `QSGRenderNode` render path.

## Priorities

1. Qt Quick/QML execution skeleton transition (Phase 1).
2. Static image open, display, save (Phase 2).
3. 28 CPU reference algorithms wired to UI parameters (Phase 3).
4. OpenGL viewport and display resampling (Phase 4).
5. GLSL acceleration with CPU parity verification (Phase 5).
6. Video file input (Phase 6).
7. Realtime stream input (Phase 7).
8. Integration verification and cleanup (Phase 8).
