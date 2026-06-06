# User And Local Environment

## Known Local Context

Status: generated

- Repository path: `/home/seuoh/workspace/qt_ui_opencv`
- Document-editing language: English
- User communication language: Korean
- Target runtime mentioned in bootstrap notes: `videoprocess` Conda environment
- Existing stop-hook verification in this workspace uses CMake build and CTest,
  not pytest.
- OpenCV 4.13.0 is available through CMake in the `videoprocess` Conda
  environment for the current `core`, `imgproc`, and `imgcodecs` modules.
- The local CMake configure/build may warn that `WrapVulkanHeaders` is missing
  because `Vulkan_INCLUDE_DIR` is not set. This has not blocked the current
  Qt/OpenCV build or CTest suite.

## User Decisions Needed

- Confirm whether the target OpenCV build includes FFmpeg-backed `videoio`.
- Confirm whether expected GPU acceleration is CPU-only, OpenCL/UMat, CUDA, or
  runtime-selectable.
- Provide target hardware constraints if performance budgets depend on a
  specific CPU/GPU.

## Local Constraints To Measure

These facts should be measured before implementation phases that depend on
them:

- OpenCV modules available at build time.
- Runtime OpenCL availability.
- Runtime CUDA availability, if CUDA is in scope.
- Supported video codecs and containers through `cv::VideoCapture`.
- RTSP/HTTP stream timeout and reconnect behavior through `cv::VideoCapture`.
- Baseline processing time for maximum-size video effect stacks.

## Commands

Project-specific verification commands should be recorded here only after they
are confirmed in this workspace. Do not replace execution with documentation.

- `cmake -S . -B build`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `./build/tst_OpenCvImageBridge`
- `./build/tst_ImageProcessorCore`
- `QT_QPA_PLATFORM=offscreen ./build/tst_ImageIoService`
- `QT_QPA_PLATFORM=offscreen ./build/tst_ProcessingController`
