# Decision History

Irreversible or high-impact technical choices. Use `plan-coordinator` to append new decisions.

## D1 — Qt Quick/QML as UI framework, drop Qt Widgets

- Decision: The final application uses Qt Quick/QML + Qt Quick Controls. Qt Widgets is removed from CMake link dependencies and source.
- Rationale: Qt6 unifies UI, window, event loop, and OpenGL context; Qt Widgets cannot meet the dashboard-style design in `ui.md` cleanly.
- Source: `plan.md` § Goal; `structure.md` § Qt Quick + Core Module Pattern, § Build And Runtime Baseline.

## D2 — Drop SDL2 entirely from final build

- Decision: SDL2 (`SDL_Window`, `SDL_Event`, `SDL_GLContext`, `SDL_GL_GetProcAddress`, `SDL_Thread`/`SDL_mutex`/`SDL_cond`) is not a final dependency.
- Rationale: Qt owns window, event loop, OpenGL context, and thread primitives; SDL2 is redundant.
- Source: `structure.md` § Dependency Ownership, § Porting Boundary.

## D3 — Central viewport uses `QSGRenderNode`

- Decision: The central image/video display item is implemented as a `QSGRenderNode`. `QQuickFramebufferObject` is not the default path. Window-level `beforeRendering`/`afterRendering` hooks are not used for the central viewport.
- Rationale: `QSGRenderNode` integrates with item-local geometry, clipping, opacity, stacking, and transforms while owning GL resources under the Qt Quick render context.
- Source: `plan.md` § 구현 원칙; `structure.md` § Qt Quick Render Integration.

## D4 — FFmpeg for video and realtime decode

- Decision: Video file decode and realtime stream decode use FFmpeg (`avformat`, `avcodec`, `avutil`, `swscale`, `avdevice`).
- Rationale: Existing decode logic ports over from the prior SDL2 source after moving global decoder state into `VideoInputService`.
- Source: `structure.md` § Build And Runtime Baseline, § Porting Boundary.

## D5 — Qt image IO for static images; drop STB and PPM

- Decision: Static image read/write uses Qt image IO. STB is not a required dependency. PPM parser/saver is not retained, and PPM open/save actions are absent from the final UI.
- Rationale: Qt image IO covers the supported formats without external dependencies.
- Source: `structure.md` § Build And Runtime Baseline, § Module 3 - ImageIoService.

## D6 — `ImageBuffer` RGB888 is the internal processing format

- Decision: All processing algorithms operate on RGB 3-channel `uint8_t` `ImageBuffer`. Qt `QImage` is not used as the internal processing target; Qt image IO results are converted to RGB888 `ImageBuffer`.
- Rationale: Keeps CPU and GLSL paths algorithmically identical with shared constants and clamp-to-edge semantics.
- Source: `structure.md` § Shared Data Types — ImageBuffer, § Algorithm Processing Contract.

## D7 — Per-mode effect policy: static = no cap, moving = stack of 3

- Decision: Static image mode applies algorithms directly to `outImage` with no fixed apply count limit. Video and realtime modes maintain an effect stack with `MAX_EFFECT_STACK = 3`; the fourth effect is rejected.
- Rationale: Preserves existing moving-source behavior while keeping the static editing flow unconstrained.
- Source: `plan.md` § Phase 3 작업, § Phase 6 완료 기준; `structure.md` § Pipeline Execution Policy.

## D8 — `GpuEffectPipeline` is separate from `ImageProcessorCore`

- Decision: GLSL effect resources and execution live in `GpuEffectPipeline`. `ImageProcessorCore` owns no GL resources and never calls OpenGL.
- Rationale: Keeps the CPU reference unit-testable without a GUI or OpenGL context and enforces a clean GUI-thread / render-thread boundary.
- Source: `structure.md` § Module 5 - ImageProcessorCore, § Module 6 - GpuEffectPipeline.

## D9 — Save is static-only

- Decision: Save serializes the committed CPU `outImage` and is enabled only in static image mode. Video and realtime modes do not save output files.
- Rationale: Output file semantics are only well-defined for the static accumulated edit result.
- Source: `structure.md` § Pipeline Execution Policy, § Source And Control Policy.

## D10 — Display resampling uses hybrid edge/flat interpolation

- Decision: The display shader uses Sobel-driven hybrid interpolation: upscale (edge=Bicubic Catmull-Rom a=-0.5 / flat=Bilinear), downscale (edge=Lanczos r=3 / flat=Box). Edge threshold 0.20. Boundary clamp-to-edge.
- Rationale: Preserves edge fidelity at upscale while avoiding aliasing at downscale, without using `GL_LINEAR` or mipmaps as a substitute for resampling logic.
- Source: `structure.md` § Hybrid Interpolation.

## D11 — Display shader failure is fatal; CPU fallback does not cover display

- Decision: Display resampling shader compile/link failure is reported as a fatal render error; no CPU display fallback is provided. CPU fallback covers only algorithm execution.
- Rationale: A non-OpenGL interactive viewport is out of scope.
- Source: `structure.md` § GPU Fallback Policy.

## D12 — OpenGL RHI backend forced

- Decision: In `main.cpp` before `QQmlApplicationEngine` construction, call `QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL)`. Also set environment variable `QSG_RHI_BACKEND=opengl` as belt-and-suspenders.
- Rationale: Qt6 default RHI backend is platform-dependent (Vulkan/Metal/D3D12); `QSGRenderNode` uses raw OpenGL requiring context guarantee.
- Source: Issue 1 resolution; `structure.md` § Build And Runtime Baseline.

## D13 — QSGRenderNode GL bracketing: beginExternalCommands / endExternalCommands

- Decision: `QSGRenderNode::render()` wraps all raw GL calls with `beginExternalCommands()` / `endExternalCommands()`. `changedStates()` declares every OpenGL state bit mutated: DepthState, BlendState, ViewportState, ScissorState at minimum. No manual glGet/glSet save-restore.
- Rationale: Qt RHI restores states based on `changedStates()` declaration. This is the official Qt 6 external-commands path.
- Source: Issue 2 resolution; Qt6 QSGRenderNode documentation.

## D14 — Thread topology: GUI thread command queue + render thread execution

- Decision: Do not use `QSG_RENDER_LOOP=basic` single-threaded mode. `ProcessingController` (GUI thread) writes processing results to thread-safe command structs; `QSGRenderNode::render()` (render thread) consumes them and executes `GpuEffectPipeline`. Completion/error signals route to GUI thread via `Qt::QueuedConnection`. `updatePaintNode()` (render thread, GUI blocked) copies GUI-thread state snapshots into render-thread-safe structs. `GpuEffectPipeline` initializes on first `render()` call and is destroyed in render node destructor while GL context is valid. GUI thread never calls OpenGL.
- Rationale: Maintain Qt Quick threaded render loop while isolating GL resources to render thread.
- Source: Issue 3 resolution; `structure.md` § Qt Quick Render Integration.

## D15 — Texture handoff GpuEffectPipeline → ProcessingViewport: DisplaySource struct

- Decision: `ProcessingViewport` render node owns render-thread-local struct `DisplaySource { const ImageBuffer* cpuImage; GLuint gpuTextureId; }`. Within a single `render()` call, `GpuEffectPipeline` executes first and writes `gpuTextureId`; then `ProcessingViewport` display pass reads it. Both run in the same GL context activation window; no cross-frame sync needed. `gpuTextureId == 0` means use CPU path.
- Rationale: Same GL context, sequential execution in same render() call; no cross-frame synchronization required.
- Source: Issue 4 resolution; `structure.md` § Module 6 - GpuEffectPipeline, § Module 7 - ProcessingViewport.

## D16 — Static GLSL readback: Phase 5 initial cut accepts synchronous glReadPixels; PBO is stretch goal

- Decision: Phase 5 initial implementation accepts synchronous `glReadPixels` stall when committing static GLSL effect. Static apply is user-triggered one-off; 1–5 ms stall is acceptable. Phase 5 task list includes "Implement PBO 2-frame ping-pong async readback (stretch goal)".
- Rationale: Correctness first; performance optimization after. PBO is Phase 5 stretch goal.
- Source: Issue 5 resolution; `structure.md` § Pipeline Execution Policy.

## D17 — CMake QML module: qt_add_qml_module + QML_NAMED_ELEMENT

- Decision: `CMakeLists.txt` uses `qt_add_qml_module(qt_ui URI "QtUi" VERSION 1.0 QML_FILES ...)`. C++ QML-exposed types registered via `QML_ELEMENT` or `QML_NAMED_ELEMENT` macros. `ProcessingViewportItem` registered as `QML_NAMED_ELEMENT("ProcessingViewportItem")` to be imported in QML as `import QtUi 1.0`. No separate library target; use `qt_ui` executable target directly.
- Rationale: Qt6 recommended QML module registration. Direct `qmlRegisterType` is deprecated.
- Source: Issue 6 resolution; Qt6 CMake documentation.

## D18 — Effect Stack 조작은 Undo/Redo 커맨드로 기록

- Decision: Effect stack 조작(append, removeAt(index), clear)은 모두 undo/redo 히스토리에 EditCommand로 기록된다. Undo가 지원되므로 개별 효과 제거(removeAt)를 허용한다. 스택 재정렬은 지원하지 않는다. 적용 순서는 인덱스 순(e0→e1→e2).
- Rationale: Undo로 실수를 되돌릴 수 있으므로 개별 삭제가 안전하게 제공 가능. 재정렬은 phase 범위 초과.
- Source: Issue 7 resolution; D22 (Undo/Redo in scope).

## D19 — CPU 통계 캐시: outImage 교체 시 무효화, Undo/Redo 포함

- Decision: 알고리즘 #5(Average Threshold), #10(Contrast Stretch), #23(Histogram Stretch)에 필요한 이미지 통계(평균, min/max, 히스토그램)는 ImageProcessorCore가 outImage를 교체할 때마다 무효화한다. Undo/Redo로 outImage가 복원될 때도 동일하게 무효화된다. 통계는 알고리즘 실행 직전 on-demand 계산, 멤버 캐시에 저장. 영상/스트림 모드에서는 프레임마다 새로 계산(캐시 없음).
- Rationale: outImage 교체 경로에 통합하면 별도 처리 불필요.
- Source: Issue 8 resolution; structure.md § Module 5 - ImageProcessorCore.

## D20 — Split Compare 분할선 좌표계: 정규화 비율, 뷰포트 기준

- Decision: 분할선 위치는 [0.0, 1.0] 정규화 비율로 저장하며 뷰포트 렌더 노드 내 렌더 스레드 상태에 보관한다. QML에서 드래그 이벤트 발생 시 updatePaintNode() 경로로 비율 값을 스냅샷 복사한다. 분할선은 이미지 좌표가 아닌 뷰포트 픽셀 기준으로 렌더링하되 ContentRect 영역 내에만 표시한다.
- Rationale: 뷰포트 크기가 변경되어도 비율로 저장하면 분할 위치가 자연스럽게 유지된다.
- Source: Issue 9 resolution; structure.md § Module 7 - ProcessingViewport.

## D21 — Reset 시맨틱: 정적=outImage 복원+스택 초기화, 영상/스트림=스택 초기화만

- Decision: 정적 이미지 모드의 Reset은 outImage를 inImage로 복원하고 effect stack을 비운다. inImage는 메모리에 유지되며 파일을 다시 읽지 않는다. GPU 텍스처는 다음 render() 사이클에서 교체된다. 영상/스트림 모드의 Reset은 effect stack 초기화만 수행하고 재생 상태를 유지한다. Reset은 undo/redo 히스토리를 초기화한다.
- Rationale: Reset은 "처음 상태로 돌아가기"이므로 히스토리도 함께 초기화하는 것이 자연스럽다.
- Source: Issue 10 resolution; structure.md § Source And Control Policy.

## D22 — Undo/Redo: Phase 1–8 범위 내 포함, Command 패턴

- Decision: Undo/Redo는 구현 범위 내다. EditCommand 기본 클래스(undo()/redo() 인터페이스) 채택. 기록 대상: 정적 모드 Apply(이전 outImage 스냅샷), effect stack 변경(append/removeAt/clear). 영상/스트림 모드는 effect stack 변경만 기록(이미지 스냅샷 없음). 히스토리 깊이 상한 20 스텝(초과 시 가장 오래된 커맨드 삭제). ProcessingController가 std::deque<EditCommand> + 현재 위치 인덱스 소유.
- Rationale: 사용자가 명시적으로 요구. 정적 이미지 스냅샷은 ~24MB/step × 20 = 최대 ~480MB; 허용 범위.
- Source: User requirement; D18, D19, D21 연계.

## D23 — 테스트 픽스처: test/fixtures/ 디렉토리, Qt 없이 실행 가능한 알고리즘 테스트

- Decision: test/fixtures/ 디렉토리에 소형 RGB888 원시 바이너리 파일({name}_{w}x{h}.rgb)과 기대 출력({name}_expected_{algo}.rgb)을 저장한다. 알고리즘 카테고리별 최소 1개 픽스처. CPU 알고리즘 테스트는 독립 실행 바이너리(`test_algorithms`)로, Qt GUI 없이 실행 가능해야 한다. GLSL 패리티 테스트는 별도 바이너리(`test_glsl_parity`)로 Phase 5에서 Qt + `QOffscreenSurface`로 실행한다. 두 바이너리는 독립 빌드 타겟이다.
- Rationale: GUI 없는 단위 테스트가 CI 환경에서 실행 가능해야 한다.
- Source: Issue 12 resolution; plan/REVIEW.md § QA Tooling.

## D24 — 파라미터 편집: Apply 버튼 기반, 실시간 프리뷰 없음

- Decision: 파라미터 슬라이더/입력 변경은 즉시 apply를 트리거하지 않는다. "Apply" 버튼 명시적 클릭으로만 outImage를 갱신한다. Inspector에서 파라미터 변경 시 현재 값 표시 업데이트만 수행한다. 실시간 프리뷰(스로틀링 포함)는 Phase 1–8 범위 외다.
- Rationale: structure.md의 apply 모델(버튼 기반)과 일치. 실시간 프리뷰는 스로틀링 메커니즘이 필요하고 범위를 초과한다.
- Source: Issue 13 resolution; structure.md § Pipeline Execution Policy.

## D25 — 저장 포맷: PNG/JPEG/BMP, JPEG 품질은 Save 시 사용자 입력

- Decision: Save는 PNG, JPEG, BMP 포맷을 지원한다. 포맷은 파일 확장자로 결정(QImageWriter에 위임). JPEG 저장 품질은 Save 다이얼로그에서 사용자가 직접 입력(범위 1–100, 기본 95). 마지막 사용 품질값은 앱 세션 내 메모리에 유지한다. PNG/BMP는 품질 설정 없음. PPM은 UI에서 제공하지 않는다(D5). 저장 해상도는 현재 outImage의 처리 해상도 그대로.
- Custom QML Save dialog (not QFileDialog) triggered by the Save button in the Bottom Bar. Dialog fields: file path text field + Browse button (Browse opens native QFileDialog for directory/filename), format selector (PNG / JPEG / BMP), quality slider + spinbox (1–100, visible only when JPEG is selected, default 95), OK / Cancel. Format auto-detected from file extension typed in the path field; if no extension or unrecognized extension, default PNG.
- Rationale: 사용자가 저장 시점에 품질을 결정하는 것이 명시적 요구사항.
- Source: Issue 14 resolution; D5, D9.

## D26 — GpuEffectPipeline 소유권: render node로 이관

- Decision: GpuEffectPipeline 소유권을 ProcessingController(GUI 스레드)에서 ProcessingViewport render node(render 스레드)로 이관한다. ProcessingController는 thread-safe EffectCommand 큐(mutex로 보호된 std::deque)만 보유한다. render()가 진입 시 큐를 drain하여 GpuEffectPipeline을 실행한다. GUI 스레드는 OpenGL을 절대 호출하지 않는다.
- Rationale: ProcessingController(GUI 스레드)가 GL 자원을 소유하면 D14("GUI thread never calls OpenGL")와 충돌한다. render node가 GpuEffectPipeline을 소유하면 GL 자원 생명주기가 render 스레드에 완전히 국한된다.
- Source: A1 resolution; D14; ARCHITECTURE.md Module 2 / Thread Topology.

## D27 — CPU 알고리즘 실행: 전용 worker thread

- Decision: CPU 알고리즘(ImageProcessorCore)은 ProcessingController가 보유하는 전용 QThread 1개에서 실행한다. 정적 Apply 명령은 worker 큐에 push되고 완료 시 Qt::QueuedConnection 시그널로 GUI 스레드에 통보된다. EditCommand는 완료 시그널 수신 후 history에 push된다. Apply 재진입 방지를 위해 ProcessingController는 applyInFlight boolean 플래그를 보유하며 QML Apply 버튼의 enabled 상태를 이 플래그에 바인딩한다. 완료/실패 시그널 수신 시 플래그를 false로 복귀한다.
- Rationale: 4K 이미지 기준 CPU 알고리즘(5×5 blur ≈ 50ms, median ≈ 100ms, Gaussian ≈ 80ms)을 GUI 스레드에서 실행하면 UI가 멈춘다. Worker thread 분리로 UI 반응성 유지.
- Source: A2 resolution; ARCHITECTURE.md Thread Topology.

## D28 — Preview 완전 제거

- Decision: Preview 기능(previewImage 버퍼, previewTexture, preview 명령)을 완전히 제거한다. Apply + Undo/Redo(D22)가 preview 사용 사례를 대체한다. Module 2 명령 목록에서 preview 제거. Buffer Roles에서 previewImage/previewTexture 제거.
- Rationale: D22(Undo/Redo 20 스텝)로 Apply 결과 되돌리기가 가능하므로 Preview는 redundant. 구현 복잡도 감소.
- Source: A3 resolution; D22; D24.

## D29 — Phase 2/4 viewport scope 분할

- Decision: Phase 2 viewport 범위 = CPU outImage → GL_TEXTURE_2D 업로드 + 전체 화면 quad + GL_NEAREST 필터 + 단순 letterbox 종횡비 유지. Phase 4 viewport 범위 = Hybrid 리샘플링 셰이더(D10) + ContentRect 정밀 계산 + zoom/pan + original/result toggle + split compare + 셰이더 컴파일 실패 surface. Phase 1 viewport = 빈 QSGRenderNode 등록, render()는 no-op.
- Rationale: Phase 3 완료기준이 "viewport redraws"를 요구하므로 Phase 2에서 최소 동작하는 viewport가 필요하다. 고품질 리샘플링과 compare 기능은 Phase 4에서 완성.
- Source: A4 resolution; ARCHITECTURE.md Phase 2/4 tasks.

## D30 — Qt Image IO format normalization

- Decision: All images loaded via `QImage`; immediately call `QImage::convertToFormat(QImage::Format_RGB888)` before copying to `ImageBuffer`.
- This single call handles all source formats: RGBA alpha stripped, 16-bit downsampled to 8-bit, grayscale channel-expanded to RGB, indexed color expanded to RGB.
- No STB; no PPM; Qt handles all decode and format conversion.
- `ImageBuffer` always receives RGB888 uint8_t data.
- Rationale: Simplifies format handling and eliminates manual conversion logic. Qt's built-in conversion is efficient and handles all edge cases.
- Source: B1 resolution (advisor review detail gap).

## D31 — Undo history memory budget

- Decision: Total memory budget for all undo entries: 512 MB.
- When adding a new entry: if `existing_total_bytes + new_entry_bytes > 512 MB`, drop oldest entries until the budget fits, or until only 1 entry remains (never reject a new entry that would be the only entry).
- The 20-step count limit still applies; both the count limit and the memory budget are enforced — whichever is hit first.
- For images where a single copy exceeds 512 MB (e.g., extreme resolutions), undo effectively holds 1 step only.
- Rationale: Prevents unbounded memory growth while preserving recent edits. 512 MB is a practical limit for interactive editing.
- Source: B5 resolution (advisor review detail gap).

## D32 — Video/stream effect stack parameter editing

- Decision: In video and stream modes, parameter editing on existing effect stack entries is allowed while playing or paused.
- Parameter changes take effect on the next frame application (video: next decoded frame; stream: next incoming frame).
- Each parameter change is recorded as a `ParameterEditCommand` in the undo history (subject to the 20-step / 512 MB limits).
- Adding or removing stack entries is a separate operation already covered by the existing effect stack mutation commands.
- Rationale: Allows real-time parameter adjustment without re-adding the effect, improving interactivity.
- Source: B6 resolution (advisor review detail gap).

## D33 — Apply 디스패치 완성 흐름 일원화

- Decision: 모든 Apply(CPU 및 GLSL 백엔드 불문)는 ProcessingController → worker thread 경유로 시작한다.
  1. Apply 버튼 → ProcessingController: `applyInFlight=true` → worker queue push.
  2. Worker thread: stats-dependent 알고리즘이면 현재 `outImage`에서 stats 계산(캐시 유효 시 재사용).
     - CPU 백엔드: 알고리즘 실행 → scratch `ImageBuffer` 생성 → `applyCompleted(scratch)` 시그널 emit.
     - GLSL 백엔드: stats를 `EffectCommand`에 첨부 → render thread queue push → render()에서 GpuEffectPipeline 실행 → glReadPixels readback → scratch `ImageBuffer` → `gpuApplyCompleted(scratch)` 시그널 emit.
  3. 두 경로 모두 completion 시그널은 `Qt::QueuedConnection`으로 GUI thread ProcessingController에 도착: `outImage` 포인터 교체(D34), EditCommand 히스토리 push, `applyInFlight=false`.
  4. 실패 시그널: `outImage` 유지, `applyInFlight=false`, EditCommand push 없음.
- `CachedStats { float avg; uint8_t minVal, maxVal; int histogram[256]; }` 는 worker thread가 outImage 교체 후 첫 stats-dependent Apply 시점에 on-demand 계산. EffectCommand에 첨부되어 render thread에 전달되므로 render thread는 stats를 별도 요청하지 않는다.
- Rationale: 단일 완성 경로로 applyInFlight 상태 관리와 EditCommand push를 일원화. stats 계산을 worker thread에 국한시켜 GUI thread 블로킹 방지.
- Source: P0 #1/#2 resolution; D26, D27, D19.

## D34 — outImage는 GUI thread만 기록

- Decision: `outImage`에 대한 쓰기는 GUI thread의 completion 시그널 핸들러(ProcessingController)에서만 수행한다. Worker thread(CPU Apply)와 render thread(GLSL readback)는 독립 scratch `ImageBuffer`에 결과를 기록하고 completion 시그널로 scratch를 GUI thread에 전달한다. GUI thread는 전달된 scratch를 `outImage`로 교체(포인터 스왑)한다. Render thread는 `updatePaintNode()` 중 복사된 `const ImageBuffer*` 스냅샷만 읽으므로 cross-thread 동시 쓰기 race가 없다.
- Rationale: outImage 교체 경로를 GUI thread 하나로 제한하면 별도 mutex 없이도 thread safety 확보.
- Source: P0 #3 resolution; D14, D26, D27.

## D35 — 영상/스트림 GLSL 스택: GpuEffectPipeline이 render()에서 inImage 업로드

- Decision: 영상/스트림 모드에서 effect stack이 비어 있지 않을 때, GpuEffectPipeline은 `render()` 첫 단계에서 `updatePaintNode()` 스냅샷으로 전달된 `inImage` 포인터를 effect source texture에 업로드한 뒤 스택을 실행한다. ProcessingController는 새 프레임 도착 시 `QQuickItem::update()`를 호출하여 scene graph render cycle을 트리거한다.
- Rationale: updatePaintNode()에서 inImage 포인터를 render-thread-safe 스냅샷으로 복사하는 기존 메커니즘을 재사용.
- Source: P0 #4 resolution; D14, D26, D15.

## D36 — applyInFlight 중 Undo/Redo/Load/Reset 비활성화

- Decision: `applyInFlight=true`인 동안 QML에서 Undo, Redo, Load(파일 열기), Reset 버튼도 모두 disabled. Apply 버튼과 동일하게 `enabled: !applyInFlight` 바인딩 적용. Worker thread 또는 render thread가 scratch buffer를 기록하는 동안 GUI thread가 `outImage`를 변경하는 race를 원천 차단한다.
- Rationale: D34(outImage GUI thread 단독 기록)의 전제 조건. applyInFlight 스코프를 Apply 버튼에만 한정하면 Undo/Reset이 동시에 실행될 수 있다.
- Source: P0 #6 resolution; D34, D27.

## D37 — GpuEffectPipeline ↔ EffectCommand queue 연결 메커니즘

- Decision: ProcessingController는 EffectCommand queue를 concrete 멤버로 소유한다(mutex-protected `std::deque<EffectCommand>`). 초기화 시점에 ProcessingController가 `viewportItem->setCommandQueue(&commandQueue_)`를 호출해 ProcessingViewportItem에 raw pointer를 주입한다. ProcessingViewportItem은 이 포인터를 `updatePaintNode()`에서 render node(QSGRenderNode)에 전달한다. GLSL Apply 시: ProcessingController가 queue에 push(mutex 보호) 후 GUI thread에서 `viewportItem->update()`를 호출해 render cycle을 트리거한다. render()는 queue를 drain(mutex)하여 GpuEffectPipeline에 전달한다. Queue 포인터 lifetime: ProcessingController가 ProcessingViewportItem보다 오래 살므로 raw pointer로 안전.
- 초기화 시퀀스: QML 엔진이 ProcessingViewportItem을 인스턴스화한 후 `componentComplete()` 또는 QML `Component.onCompleted`에서 AppController가 ProcessingController의 `setViewportItem(ProcessingViewportItem*)` 을 호출하고, 이 메서드 내부에서 `setCommandQueue`가 수행된다. ProcessingController 생성 직후에는 `viewportItem_`이 null이다.
- null guard: `viewportItem_`이 null인 상태에서 GLSL Apply 요청이 도달하면(소스 로드가 QML 완전 초기화보다 앞설 수 없으므로 정상 흐름에서는 발생하지 않음) CPU fallback을 시도하고, CPU fallback도 불가하면 apply failure signal을 emit한다.
- Rationale: setter injection으로 소유권(ProcessingController)과 소비(render node)를 분리. QML 쪽에서 별도 접근 경로 불필요. 초기화 시퀀스 명시로 null pointer 역참조 방지.
- Source: P1 #7 resolution; #5 resolution; D26, D14.

## D39 — ImageBuffer shared ownership: outImage/inImage는 shared_ptr로 관리

- Decision:
  - ProcessingController의 `outImage_`와 `inImage_`는 `std::shared_ptr<ImageBuffer>`로 저장한다.
  - completion 시그널(`applyCompleted`, `gpuApplyCompleted`) 인자 타입: `std::shared_ptr<ImageBuffer>`. Qt::QueuedConnection이 shared_ptr를 값 복사(refcount 증가)하므로 emit 후 worker/render thread가 로컬 포인터를 해제해도 GUI thread 수신 전 버퍼가 해제되지 않는다.
  - GUI thread의 outImage 교체: `outImage_ = received_ptr`. 구 shared_ptr refcount가 감소하지만, 아직 render thread 스냅샷이 보유 중이면 해제되지 않는다.
  - updatePaintNode(): render-thread-local 스냅샷으로 `shared_ptr<ImageBuffer>`를 복사(refcount 증가). render()가 실행되는 동안 GUI thread가 outImage를 교체해도 스냅샷이 구 버퍼를 안전하게 유지한다 (#2 해소).
  - Worker thread: Apply 태스크 시작 시 `outImage_`의 `shared_ptr` 스냅샷을 캡처한다. stats 계산 및 CPU 알고리즘 실행이 이 스냅샷을 참조하므로 GUI thread의 outImage 교체와 race가 없다 (#4 해소).
  - inImage write authority: VideoInputService 디코더 스레드는 새 `ImageBuffer`를 `std::shared_ptr<ImageBuffer>`로 생성하여 Qt::QueuedConnection 시그널로 GUI thread에 전달한다. GUI thread만 `inImage_` shared_ptr를 교체한다(D34의 outImage 규칙을 inImage에도 동일하게 적용). updatePaintNode()는 `inImage_` shared_ptr 스냅샷을 render-thread-local에 복사한다(D35) (#3 해소).
- Rationale: raw pointer swap으로는 updatePaintNode()~render() 구간에서 GUI thread가 구 버퍼를 해제하는 use-after-free를 방지할 수 없다. shared_ptr은 홀더가 존재하는 한 버퍼를 살려두므로 추가 mutex 없이 thread safety를 확보한다.
- Source: #1–#4 resolution; D34, D33, D35.

## D38 — CPU/GLSL 알고리즘 dispatch: switch(algorithmId)

- Decision: `ImageProcessorCore::apply(int algorithmId, ImageBuffer& dst, const ImageBuffer& src, const EffectParams& params)`는 `switch(algorithmId)` (case 1..28)로 구현 알고리즘을 선택한다. GpuEffectPipeline의 GLSL dispatch도 동일하게 `switch(algorithmId)`로 셰이더를 선택한다. `AlgorithmSpec`은 순수 데이터 구조체(id, name, category, 파라미터 스키마, 백엔드 지원 플래그)로 유지하며 function pointer를 포함하지 않는다.
- Rationale: 가장 단순하고 디버깅이 용이한 구현. AlgorithmSpec이 선언적 메타데이터 역할만 유지.
- Source: P2 #10 resolution; D8; ARCHITECTURE.md Algorithm Catalog.
