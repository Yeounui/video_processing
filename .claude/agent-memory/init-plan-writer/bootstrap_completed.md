---
name: bootstrap_init_plan_qt_ui
description: Qt6 image processing project initial plan bootstrap completed 2026-05-28
metadata:
  type: project
---

Bootstrap initialization of `plan/` documents completed on 2026-05-28 for Qt6 image processing desktop application.

**Artifacts Created**

Seven canonical plan documents created in `/home/seuoh/workspace/qt_ui/plan/`:
- `README.md` — status index and document map
- `OVERVIEW.md` — project goal, scope, principles
- `DECISIONS.md` — 11 irreversible technical decisions with rationale and source citations
- `PHASES.md` — 8 phases with embedded tasks and completion criteria from plan.md
- `ARCHITECTURE.md` — module contracts, algorithm catalog (28 rows), parameter schema (18 rows), UI layout/color, GPU fallback policy
- `REVIEW.md` — validation checklist (15 items), failure handling matrix (10 rows), per-phase verification, QA tooling
- `USER.md` — user-local tasks and intentional placeholders for Qt6 path, FFmpeg path, OpenGL baseline, GPU constraints

**Key Decisions Embedded**

D1–D11 decisions locked:
- Qt Quick/QML + no Qt Widgets or SDL2
- `QSGRenderNode` for viewport, separate `GpuEffectPipeline`
- Static image no apply cap; video/stream stack limit = 3
- Static save only; save disabled for video/stream
- Display resampling shader failure is fatal (no CPU fallback for display)
- FFmpeg video/realtime input, Qt image IO for static
- CPU/GLSL parity within ±1 per channel

**Verbatim Content**

Algorithm Catalog, Parameter Schema, Fixed Kernels, Failure Handling Matrix all copied byte-for-byte from `structure.md`.

**Status**

All phases: `stub exists`. Phase 1 entry point: replace QWidget shell with QGuiApplication + QML engine, drop SDL2/Widgets from CMake, wire QSGRenderNode viewport stub, build QML ApplicationWindow matching ui.md layout.

**Next**

Plan-coordinator ready to track phase transitions and record architecture changes as implementation proceeds.
