# Work Phases

Embedded verbatim from `plan.md` § Phase 1..8. Tasks and completion criteria are the contract for each phase. Use `plan-coordinator` to record status transitions (`stub exists` → `draft written` → `generated` → `verified`).

## Phase 1 — Qt Quick Application Skeleton

**Status: verified**

Completion record:

- ✅ Remove Qt Widgets dependency from CMake — CMakeLists.txt updated, no Qt6::Widgets link.
- ✅ Configure target on Qt6 Core, Gui, Qml, Quick, QuickControls2, OpenGL — qt_add_qml_module(qt_ui URI "QtUi" VERSION 1.0); all link targets present.
- ✅ Replace `QApplication`/`QWidget` startup with `QGuiApplication` and QML engine startup — src/main.cpp uses QGuiApplication + QQmlApplicationEngine.
- ✅ Add initial configuration so Qt Quick uses the OpenGL render path — QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL) called before engine construction.
- ✅ Force OpenGL RHI backend — qputenv("QSG_RHI_BACKEND", "opengl") set; environment variable applied.
- ✅ Configure QML module with `qt_add_qml_module(qt_ui URI "QtUi" VERSION 1.0)`; register C++ items via `QML_NAMED_ELEMENT` — QML module configured; ProcessingViewportItem registered via QML_NAMED_ELEMENT.
- ✅ Build a no-op QSGRenderNode stub: register ProcessingViewportItem via QML_NAMED_ELEMENT; render() body is empty; confirm QML import QtUi 1.0 works — ProcessingViewportItem::render() is no-op (D29); QML import QtUi 1.0 confirmed working.
- ✅ Build the first QML shell that matches the `ui.md` layout — App.qml, TopBar.qml, SourcePanel.qml, ProcessingViewport.qml, AlgorithmInspector.qml, BottomTransport.qml created.

Verification:

- ✅ On launch a QML `ApplicationWindow` is shown — ApplicationWindow displayed on startup.
- ✅ The final target does not find or link SDL2 or Qt Widgets — build output confirms no SDL2 or Qt Widgets in link dependencies.
- ✅ Empty state screen shows top bar, left source panel, central viewport area, right inspector, and bottom bar — all 5 regions visible in empty state.

Completion criteria met. Build verified: cmake --build build 33/33 steps, zero errors.

## Phase 2 — Static Image Basic Workflow

**Status: generated**

Completion record:

- ✅ Implement image open/save service — ImageIoService created; Qt image IO with QImage::convertToFormat(QImage::Format_RGB888) per D30.
- ✅ Normalize image decode results to the processing buffer rules — all formats (RGBA, 16-bit, grayscale, indexed) normalized to RGB888 ImageBuffer via D30 single-call conversion.
- ✅ Implement static image state, reset, save, and error preservation — ProcessingController owns inImage/outImage as shared_ptr<ImageBuffer> per D39; reset copies inImage to outImage (D21); saveImage gates to SOURCE_IMAGE mode (D9); openImage preserves buffers on failure (D30 error ordering).
- ✅ Implement minimal viewport display: upload CPU outImage to GL_TEXTURE_2D; draw fullscreen quad with GL_NEAREST; letterbox to preserve aspect ratio. No hybrid resampling; no zoom/pan; no compare modes — ProcessingViewportItem rewritten with QSGSimpleTextureNode, GL_NEAREST sampler, letterbox aspect-ratio calculation per D29.

Structural implementation verified:

- ✅ A successful image open updates the viewport — openImage → inImage/outImage populated, viewportItem->update() called per ARCHITECTURE Module 2.
- ✅ A failed open preserves the previous source and buffer — error early-return before buffer mutation per D30.
- ✅ Reset restores the original state — reset() copies inImage → outImage, no disk re-read per D21.
- ✅ Save writes at the current processing result resolution — saveImage serializes outImage directly per ARCHITECTURE Buffer Roles.
- ✅ Save is disabled in video/stream modes — saveImage gated by `sourceType == SOURCE_IMAGE` check per D9.

Evidence:

- C++ implementation complete: ImageBuffer.h, ImageIoService.h/.cpp, ProcessingController.h/.cpp generated.
- ProcessingViewportItem fully rewritten: QSGSimpleTextureNode, GL_NEAREST, letterbox per D29.
- QML updated: TopBar (FileDialog + open/save/reset wiring), ProcessingViewport (empty state toggle), SourcePanel (filename/dimensions display).
- Code review: 3 issues resolved (QImage raw pointer → .copy(), uploadedVersion_ reset on setController, unused signal removed).
- Build verified: 22/22 targets, zero errors.
- Runtime verified: QT_QPA_PLATFORM=xcb, 8 seconds clean startup.

Acceptance criteria met structurally. Visual interactive test (file open/display/save/reset) deferred to user verification.

## Phase 3 — 28 CPU Reference Algorithms

Tasks:

- Implement 28 CPU algorithms against the algorithm contract in `structure.md`.
- Wire per-algorithm parameter schema and defaults into the backend model.
- Wire QML inspector to algorithm search, selection, parameter editing, and apply.
- Static image continues direct accumulated apply with no fixed apply cap.
- Limit only video/stream mode effect stack to 3.
- Implement EditCommand pattern and undo/redo history in ProcessingController (static apply + effect stack mutations; max 20 steps).

Completion criteria:

- All 28 algorithms are selectable from the inspector.
- Each algorithm's parameters are adjustable from UI without console input.
- On apply success, `outImage` updates and the viewport redraws.
- Static images can apply multiple algorithms in succession.
- Small fixture-based CPU result tests pass.
- Undo/redo history records static applies and effect stack changes; Reset clears history.

## Phase 4 — QML OpenGL Viewport

Tasks:

- Replace the Phase 2 minimal display with the full hybrid resampling shader (D10).
- Implement precise ContentRect calculation and zoom/pan controls.
- Wire original/result and split compare modes.
- Surface shader compile/link failures to the user.

Completion criteria:

- After resize, aspect ratio is preserved.
- Display-side resize does not change the processing buffer.
- After rotate/shrink/flip there is no residual frame in the viewport.
- Viewport shader compile/link failures are surfaced to the user.

## Phase 5 — GLSL Effect Pipeline

Tasks:

- Implement the GLSL effect path described in `structure.md`.
- Implement `GpuEffectPipeline` as a separate module; remove GPU resources from `ImageProcessorCore`.
- Implement static-image and moving-source synchronization policies per mode.
- Implement the GPU fallback policy from `structure.md`.
- Implement PBO 2-frame ping-pong async readback for static GLSL commit (stretch goal; synchronous `glReadPixels` is acceptable for initial cut).

Completion criteria:

- CPU/GLSL result difference is within `±1` per channel on small fixtures.
- GPU effect failure triggers documented fallback or apply cancellation.
- Static image save through the GLSL path matches the current processing result.

## Phase 6 — Video File Input

Tasks:

- Implement FFmpeg video file open, decode, seek, step, speed, loop.
- Wire per-frame effect stack application.
- Wire bottom playback bar and frame/time display.

Completion criteria:

- Video play/pause/step/seek works.
- Effect stack applies per frame.
- A fourth effect append is rejected.
- On EOF, loop repeats or pause holds based on loop setting.

## Phase 7 — Realtime Stream Input

Tasks:

- Wire UI for URL or device input.
- Implement realtime frame queue with latest-frame policy.
- Implement pause, reconnect, disconnect status indicators.

Completion criteria:

- Stream frames render in the viewport.
- On disconnect, the last valid frame is preserved and status is shown.
- Reconnect retries the previous input source.
- Reset effects clears the stack.

## Phase 8 — Integration Verification And Cleanup

Tasks:

- Audit CMake target and includes for SDL2 and Qt Widgets traces.
- Audit that PPM open/save and STB are not required dependencies in the final UI/build.
- Run algorithm fixture, image save, viewport resize, and CPU/GLSL parity tests.
- Verify QML UI matches `ui.md` color and layout.
- Audit memory and GPU resource lifecycle issues.

Completion criteria:

- Smoke tests for app launch, image workflow, video workflow, and stream workflow pass.
- SDL2 and Qt Widgets are not in final link dependencies.
- PPM open/save actions are absent from the final UI.
- All 28 algorithms behave according to the documented contract.
- On failure cases the previous valid state is preserved.
