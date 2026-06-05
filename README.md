# Qt UI Video Processing

Qt 6 Quick/QML과 C++17로 작성한 이미지/비디오 처리 UI입니다.

기존 `video_processing` 프로젝트의 28개 영상 처리 알고리즘을 데스크톱 UI에서 적용할 수 있도록 구성한 버전입니다. 정적 이미지, 영상 파일, RTSP/RTMP/HTTP 스트림 입력을 열 수 있고, 이미지 모드에서는 결과 저장과 undo/redo를 지원합니다.

## 주요 기능

- Qt Quick/QML 기반 3패널 UI
- 정적 이미지 열기, 처리 결과 저장, reset, undo/redo
- FFmpeg 기반 영상 파일 재생, 일시정지, seek, frame step
- RTSP/RTMP/HTTP 스트림 열기, reconnect, disconnect
- 이미지 모드의 직접 편집과 영상/스트림 모드의 effect stack
- Point, Geometry, Filter, Edge, Morphology, Grayscale 카테고리별 알고리즘 선택
- 알고리즘별 파라미터 편집 UI
- Qt Scene Graph 렌더링 경로와 OpenGL 기반 GPU 효과 파이프라인

## 알고리즘

총 28개 알고리즘을 제공합니다.

| ID | Algorithm | Category |
|---:|---|---|
| 1 | Brightness | Point |
| 2 | Multiply | Point |
| 3 | Gamma | Point |
| 4 | Fixed Threshold | Point |
| 5 | Average Threshold | Point |
| 6 | Bitwise AND | Point |
| 7 | Flip | Geometry |
| 8 | Rotate | Geometry |
| 9 | Emboss | Filter |
| 10 | Contrast Stretch | Filter |
| 11 | 3x3 Blur | Filter |
| 12 | 5x5 Blur | Filter |
| 13 | Gaussian Blur | Filter |
| 14 | Sharpen | Filter |
| 15 | High-Pass Sharpen | Filter |
| 16 | High-Boost | Filter |
| 17 | Diagonal Motion Blur | Filter |
| 18 | Horizontal Motion Blur | Filter |
| 19 | Horizontal Edge | Edge |
| 20 | Vertical Edge | Edge |
| 21 | Laplacian | Edge |
| 22 | DoG | Edge |
| 23 | Histogram Stretch | Edge |
| 24 | Endpoint Detection | Edge |
| 25 | Median Smoothing | Morphology |
| 26 | Grayscale Average | Grayscale |
| 27 | Grayscale Luminosity | Grayscale |
| 28 | Grayscale Lightness | Grayscale |

## 요구 사항

- CMake 3.16 이상
- C++17 compiler
- Qt 6: Core, Gui, Qml, Quick, QuickControls2
- FFmpeg libraries: `libavformat`, `libavcodec`, `libavutil`, `libswscale`

FFmpeg 경로는 환경변수에서 읽습니다. 로컬에서는 `.env`에 `QT_UI_FFMPEG_PREFIX`를 두고, 다른 환경에서는 `QT_UI_FFMPEG_PREFIX`, `FFMPEG_PREFIX`, 또는 활성화된 Conda의 `CONDA_PREFIX`를 사용하면 됩니다.

## 빌드

```bash
set -a
source .env
set +a
cmake -B build -G Ninja
cmake --build build --target qt_ui
```

실행:

```bash
./build/qt_ui
```

WSLg/Wayland 환경에서는 `QT_UI_WSLG_RUNTIME_DIR`, `QT_UI_QPA_PLATFORM`, `QT_UI_QSG_RHI_BACKEND` 환경변수를 사용해 `XDG_RUNTIME_DIR`, `QT_QPA_PLATFORM`, `QSG_RHI_BACKEND`를 보정합니다.

## 사용 방법

- `Open Image`: 이미지 파일 열기
- `Open Video`: 영상 파일 열기
- `Open Stream`: RTSP/RTMP/HTTP 스트림 URL 열기
- `Save`: 이미지 모드의 현재 결과 저장
- `Reset`: 원본 이미지 또는 현재 소스 상태로 되돌리기
- 오른쪽 알고리즘 패널에서 카테고리와 알고리즘을 선택한 뒤 파라미터를 조정하고 `Apply` 실행
- 영상 모드에서는 하단 transport bar로 재생, 일시정지, seek, frame step 제어

## 테스트

로컬 테스트 파일이 포함된 checkout에서는 다음 명령으로 테스트를 실행할 수 있습니다.

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

GUI 관련 테스트는 headless 환경에서 `QT_QPA_PLATFORM=offscreen`을 사용합니다.

## 프로젝트 구조

```text
include/   C++ public headers and QML-facing types
src/       Qt application, controller, image/video services, processing core
qml/       Qt Quick UI components
tests/     Local unit tests, when included in the checkout
```

## 라이선스

소스 코드는 MIT License로 공개됩니다. 자세한 내용은 `LICENSE`를 참고하세요.
