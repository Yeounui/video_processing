# Qt6 Image Processing — Plan Index

Current status entry point and canonical document map. Other plan documents are accessed through `plan-coordinator`.

## Current Status

- Overall: **verified** — Phase 1 (Qt Quick) verified, Phase 2 (Static image) generated, Phase 3 (28 CPU algorithms) verified. Image buffer, IO service, processing controller, algorithm selection, parameter editing, Apply, Undo/Redo all functional.
- Phase 1 (Qt Quick application skeleton): **verified** — CMakeLists.txt configured for Qt6 Core/Gui/Qml/Quick/QuickControls2; qt_add_qml_module(qt_ui URI "QtUi" VERSION 1.0); QML shell created.
- Phase 2 (Static image basic workflow): **generated** — ImageBuffer (RGB888), ImageIoService (Qt image IO with D30 format normalization), ProcessingController image state (inImage/outImage shared_ptr per D39), minimal viewport (QSGSimpleTextureNode GL_NEAREST + letterbox per D29), error preservation (D30), reset semantics (D21), save gating (D9).
- Phase 3 (28 CPU reference algorithms): **verified** — 28 algorithms implemented and selectable from inspector. Parameter adjustment, Apply, Undo/Redo, and reset functional. Image open/display/save/reset workflow confirmed.
- Phase 4 (QML OpenGL viewport): **verified** — hybrid resampling shader with Sobel edge detection implemented. Original/A|B compare modes functional. Resize preserves aspect ratio. No residual frames on algorithm application. Known Phase 3 bug (rotate clipping) identified and deferred.
- Phase 5 (GLSL effect pipeline): pending.
- Phase 6 (Video file input): pending.
- Phase 7 (Realtime stream input): pending.
- Phase 8 (Integration verification and cleanup): pending.

## Next Action

Fix Phase 3 rotate clipping bug: ImageProcessorCore.cpp rotate case clips output to input dimensions — expand canvas or center-crop without accumulation. Then proceed to Phase 5 (GLSL effect pipeline).

## Document Map

| Information | Canonical Location |
|-------------|--------------------|
| Project goal and scope | `plan/OVERVIEW.md` |
| Current status and document map | `plan/README.md` (this file) |
| Decision history | `plan/DECISIONS.md` |
| Work phases and procedure | `plan/PHASES.md` |
| Code structure and architecture | `plan/ARCHITECTURE.md` |
| Review, QA, fallback | `plan/REVIEW.md` |
| User-run tasks and local constraints | `plan/USER.md` |
| Executable scripts | `scripts/` (not created yet) |
| Reference code and templates | `snippets/` (not created yet) |

## Source Specs

The following root documents are the original specification sources. The plan documents above are the canonical working copies. Where plan documents summarize, they cite these sources by name.

- `plan.md` — phase definitions and priorities
- `structure.md` — full architecture, module contracts, 28-algorithm catalog
- `ui.md` — visual design, color palette, component model
- `ui_template/desktop.webp` — color mood source
- `ui_template/light-collection-blue-elegant-ui-ux-elements-...jpg` — element style source
