# Review, QA, Fallback

## Validation Checklist

- SDL2 is not in final CMake link dependencies.
- No final source includes SDL headers.
- Qt Quick/QML owns the window and UI; Qt owns the event loop and OpenGL context.
- Central viewport uses `QSGRenderNode`.
- `ImageProcessorCore` owns no GL resources.
- `GpuEffectPipeline` owns GLSL effect resources.
- Static image path supports load, process, display, save.
- Video/stream modes support an effect stack but not save.
- Static image mode has no fixed apply cap.
- Video/stream effect stack rejects the fourth effect.
- PPM open/save actions are absent.
- `AlgorithmSpec` exposes parameter schema for QML controls.
- Display resize preserves aspect ratio and never rewrites `outImage`.
- CPU/GLSL parity is within `±1` per channel on small fixtures.
- Use valgrind to check memory leakage.

## Failure Handling Matrix

| Failure | Required behavior |
|---------|-------------------|
| Image open/decode failure | Preserve previous source and buffers. |
| Static algorithm failure | Preserve committed `outImage` and clear failed preview/candidate state. |
| Effect append failure | Preserve existing effect stack. |
| Shader compile/link failure | Use CPU fallback when available; otherwise reject effect. |
| FBO incomplete / GPU memory failure | Disable affected GPU path and try CPU fallback. |
| Display shader failure | Report fatal render error; do not mark frame rendered. |
| Save failure | Preserve `outImage` and report path/error. |
| Video EOF | Loop if enabled; otherwise pause on last frame. |
| Realtime disconnect | Keep last valid frame and wait for reconnect. |
| Allocation failure | Cancel in-progress replacement and preserve existing state. |

## Per-Phase Verification

- **Phase 1**: launch app and confirm QML `ApplicationWindow` shows top bar, source panel, viewport area, inspector, bottom bar; CMake reports no SDL2/Widgets link.
- **Phase 2**: image open success updates viewport; failed open preserves previous; reset restores; save writes at processing resolution; save disabled in video/stream modes.
- **Phase 3**: every algorithm selectable + tunable from inspector without console; apply updates `outImage`; static accumulates; CPU fixture tests pass.
- **Phase 4**: resize preserves aspect; display-side resize does not change processing buffer; rotate/shrink/flip leave no residue; viewport shader failure surfaced.
- **Phase 5**: CPU/GLSL parity `±1` on fixture set; GPU effect failure triggers documented fallback or apply cancel; GLSL static save matches processing result.
- **Phase 6**: video play/pause/step/seek work; effect stack applies per frame; fourth effect rejected; EOF respects loop setting.
- **Phase 7**: stream frames render; disconnect preserves last frame and shows status; reconnect retries previous source; reset effects clears stack.
- **Phase 8**: app, image, video, stream smoke tests pass; SDL2/Widgets absent from final link; PPM actions absent; 28-algorithm contracts match; failures preserve prior valid state; valgrind clean on representative runs.

## QA Tooling

- Algorithm fixtures: small RGB fixtures with hand-checked outputs for each algorithm category.
- CPU/GLSL parity: run identical fixture through CPU and GLSL paths; diff channel values per pixel; assert `|d| ≤ 1`.
- Memory: valgrind on representative static/video/stream sessions.
- Manual: visual confirmation against `ui.md` palette and layout regions; verify central viewport remains the visual focus.
