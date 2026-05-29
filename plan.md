# 영상 처리 Qt6 통합 구현 계획

이 문서는 작업 순서와 검증 기준만 정의한다. 프로그램 구조, 모듈 책임, 데이터 흐름, 렌더링 계약, 알고리즘 계약은 `structure.md`를 기준으로 한다. 화면 구성과 시각 스타일은 `ui.md`를 기준으로 한다.

## 목표

- Qt Quick/QML 기반 데스크톱 영상 처리 프로그램을 구현한다.
- 정적 이미지, 영상 파일, 실시간 스트림 입력을 지원한다.
- 28개 영상 처리 알고리즘을 모두 제공한다.
- UI, 창, 이벤트 루프, OpenGL context는 Qt6로 통일한다.
- 최종 빌드에서 SDL2와 Qt Widgets 의존성을 제거한다.

## 구현 원칙

- 구조 변경은 `structure.md`에 먼저 반영한 뒤 코드에 적용한다.
- 시각 디자인 변경은 `ui.md`에 먼저 반영한 뒤 QML에 적용한다.
- 처리 결과의 기준 구현은 CPU reference로 먼저 만든다.
- GLSL 구현은 CPU reference와 결과 parity를 맞춘 뒤 활성화한다.
- 저장 가능 범위와 effect 제한은 `structure.md`의 mode별 정책을 따른다.
- QML viewport는 `QSGRenderNode` 방식으로 구현한다.

## Phase 1 - Qt Quick 애플리케이션 뼈대

작업:

- CMake에서 Qt Widgets 의존성을 제거한다.
- Qt6 Core, Gui, Qml, Quick, QuickControls2, OpenGL 기반으로 target을 구성한다.
- `QApplication`/`QWidget` 시작 코드를 `QGuiApplication`/QML engine 시작 코드로 교체한다.
- Qt Quick이 OpenGL render path를 사용하도록 초기 설정을 추가한다.
- `QSGRenderNode` 기반 custom viewport item의 최소 등록 구조를 만든다.
- `ui.md`의 레이아웃을 기준으로 첫 화면 QML shell을 만든다.

완료 기준:

- 앱 실행 시 QML `ApplicationWindow`가 표시된다.
- 최종 target이 SDL2와 Qt Widgets를 찾거나 링크하지 않는다.
- 빈 상태 화면, 상단 바, 좌측 source panel, 중앙 viewport 영역, 우측 inspector, 하단 bar가 표시된다.

## Phase 2 - 정적 이미지 기본 workflow

작업:

- 이미지 열기/저장 service를 구현한다.
- 이미지 decode 결과를 `structure.md`의 처리 버퍼 규칙에 맞춰 정규화한다.
- 정적 이미지 상태, reset, save, error preservation을 구현한다.
- viewport에는 우선 CPU 결과를 표시할 수 있는 최소 경로를 연결한다.

완료 기준:

- 이미지 열기 성공 시 viewport가 갱신된다.
- 열기 실패 시 이전 source와 buffer가 유지된다.
- reset이 원본 상태를 복원한다.
- save가 현재 처리 결과 해상도로 저장된다.
- 영상/스트림 모드에서는 save가 비활성화된다.

## Phase 3 - 28개 CPU reference 알고리즘

작업:

- `structure.md`의 알고리즘 계약에 맞춰 28개 CPU 구현을 작성한다.
- 알고리즘별 parameter schema와 기본값을 backend model에 연결한다.
- QML inspector에서 알고리즘 검색, 선택, 파라미터 편집, apply를 연결한다.
- 정적 이미지는 기존 동작처럼 직접 누적 적용하고 fixed apply cap을 두지 않는다.
- 영상/스트림 mode의 effect stack만 최대 3개로 제한한다.

완료 기준:

- 모든 28개 알고리즘이 inspector에서 선택 가능하다.
- 각 알고리즘의 파라미터가 console 입력 없이 UI에서 조정된다.
- apply 성공 시 `outImage`가 갱신되고 화면이 다시 그려진다.
- 정적 이미지는 여러 알고리즘을 연속 적용할 수 있다.
- 작은 fixture 기반 CPU 결과 테스트가 통과한다.

## Phase 4 - QML OpenGL viewport

작업:

- QML에서 사용할 custom viewport item을 구현한다.
- viewport render path는 `QSGRenderNode`를 사용한다.
- CPU RGB buffer upload, aspect-preserving fit, clear, letterbox/pillarbox 처리를 구현한다.
- zoom, pan, original/result compare, split compare 표시를 연결한다.
- 표시용 resampling shader를 구현한다.

완료 기준:

- resize 후 aspect ratio가 유지된다.
- 표시용 resize가 처리 buffer를 변경하지 않는다.
- 회전, 축소, 반전 후 이전 frame 잔상이 남지 않는다.
- viewport shader compile/link 실패가 사용자에게 보고된다.

## Phase 5 - GLSL effect pipeline

작업:

- `structure.md`의 GLSL effect 경로를 구현한다.
- `GpuEffectPipeline` 별도 모듈을 구현하고 GPU resource를 `ImageProcessorCore`에서 분리한다.
- 정적 이미지와 moving-source mode별 동기화 정책을 구현한다.
- `structure.md`의 GPU fallback policy를 구현한다.

완료 기준:

- CPU/GLSL 결과 차이가 작은 fixture에서 채널당 `±1` 이내다.
- GPU effect 실패 시 문서화된 fallback 또는 적용 취소가 동작한다.
- GLSL 경로에서도 정적 이미지 save 결과가 현재 처리 결과와 일치한다.

## Phase 6 - 영상 파일 입력

작업:

- FFmpeg 기반 영상 파일 열기, decode, seek, step, speed, loop를 구현한다.
- frame별 effect stack 적용을 연결한다.
- 하단 playback bar와 frame/time 표시를 연결한다.

완료 기준:

- 영상 파일 재생/일시정지/step/seek가 동작한다.
- effect stack이 frame마다 적용된다.
- 4번째 effect append가 거부된다.
- EOF에서 loop 설정에 따라 반복 또는 pause가 동작한다.

## Phase 7 - 실시간 스트림 입력

작업:

- URL 또는 장치 입력을 여는 UI를 연결한다.
- realtime frame queue를 최신 frame 우선 정책으로 구현한다.
- pause, reconnect, disconnect 상태 표시를 구현한다.

완료 기준:

- 스트림 frame이 viewport에 표시된다.
- disconnect 시 마지막 정상 frame이 유지되고 상태가 표시된다.
- reconnect가 이전 입력으로 재연결을 시도한다.
- reset effects가 stack을 비운다.

## Phase 8 - 통합 검증과 정리

작업:

- CMake target과 include에서 SDL2, Qt Widgets 흔적을 검사한다.
- PPM open/save와 STB 필수 의존성이 최종 UI/빌드에 남지 않았는지 검사한다.
- 알고리즘 fixture, 이미지 저장, viewport resize, CPU/GLSL parity 테스트를 실행한다.
- QML UI가 `ui.md`의 색감과 레이아웃을 따르는지 확인한다.
- 메모리와 GPU resource 생명주기 문제를 점검한다.

완료 기준:

- 앱 실행, 이미지 workflow, 영상 workflow, 스트림 workflow의 smoke test가 통과한다.
- SDL2와 Qt Widgets가 최종 link dependency에 없다.
- PPM open/save action이 최종 UI에 없다.
- 모든 28개 알고리즘이 문서화된 계약대로 동작한다.
- 실패 케이스에서 이전 정상 상태가 보존된다.

## 우선순위

1. Qt Quick/QML 실행 뼈대 전환.
2. 정적 이미지 열기, 표시, 저장.
3. 28개 CPU reference 알고리즘과 UI parameter 연결.
4. OpenGL viewport와 표시용 resampling.
5. GLSL 가속과 parity 검증.
6. 영상 파일 입력.
7. 실시간 스트림 입력.
