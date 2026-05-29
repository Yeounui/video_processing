# Qt6 Image Processing — Plan Index

Current status entry point and canonical document map. Other plan documents are accessed through `plan-coordinator`.

## Current Status

- Overall: **generated** — Phase 1 (Qt Quick) verified, Phase 2 (Static image) generated. ImageBuffer, ImageIoService, ProcessingController, and minimal viewport (GL_NEAREST + letterbox) implementation complete and building without errors.
- Phase 1 (Qt Quick application skeleton): **verified** — CMakeLists.txt configured for Qt6 Core/Gui/Qml/Quick/QuickControls2/OpenGL; qt_add_qml_module(qt_ui URI "QtUi" VERSION 1.0); QSGRenderNode stub registered; QML shell created.
- Phase 2 (Static image basic workflow): **generated** — ImageBuffer (RGB888), ImageIoService (Qt image IO with D30 format normalization), ProcessingController image state (inImage/outImage shared_ptr per D39), minimal viewport (QSGSimpleTextureNode GL_NEAREST + letterbox per D29), error preservation (D30), reset semantics (D21), save gating (D9).
- Phase 3 (28 CPU reference algorithms): pending.
- Phase 4 (QML OpenGL viewport): pending.
- Phase 5 (GLSL effect pipeline): pending.
- Phase 6 (Video file input): pending.
- Phase 7 (Realtime stream input): pending.
- Phase 8 (Integration verification and cleanup): pending.

## Next Action

Begin Phase 3: implement 28 CPU reference algorithms against the algorithm contract in `structure.md`, wire per-algorithm parameter schema and defaults into backend model, wire QML inspector to algorithm search/selection/parameter editing/apply, and implement EditCommand + undo/redo history (max 20 steps, 512 MB budget).

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
