cmake_minimum_required(VERSION 3.16)

project(qt_ui LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

# FFmpeg prefix comes from the local shell environment. For this repo, source
# .env before configuring or build from an activated Conda environment.
if(DEFINED ENV{QT_UI_FFMPEG_PREFIX})
    set(FFMPEG_PREFIX "$ENV{QT_UI_FFMPEG_PREFIX}")
elseif(DEFINED ENV{FFMPEG_PREFIX})
    set(FFMPEG_PREFIX "$ENV{FFMPEG_PREFIX}")
elseif(DEFINED ENV{CONDA_PREFIX})
    set(FFMPEG_PREFIX "$ENV{CONDA_PREFIX}")
else()
    message(FATAL_ERROR "Set QT_UI_FFMPEG_PREFIX, FFMPEG_PREFIX, or CONDA_PREFIX before configuring.")
endif()
file(TO_CMAKE_PATH "${FFMPEG_PREFIX}" FFMPEG_PREFIX)
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h HINTS "${FFMPEG_PREFIX}/include" REQUIRED)
# Use static libraries to avoid transitive dependency issues
find_library(AVFORMAT_LIB avformat HINTS "${FFMPEG_PREFIX}/lib" REQUIRED)
find_library(AVCODEC_LIB  avcodec  HINTS "${FFMPEG_PREFIX}/lib" REQUIRED)
find_library(AVUTIL_LIB   avutil   HINTS "${FFMPEG_PREFIX}/lib" REQUIRED)
find_library(SWSCALE_LIB  swscale  HINTS "${FFMPEG_PREFIX}/lib" REQUIRED)

# Stub shared lib: provides x264_bit_depth missing from conda videoprocess libx264.so.138
add_library(x264_compat SHARED src/x264_compat.c)

qt_standard_project_setup()

qt_add_executable(qt_ui
    src/main.cpp
)

qt_add_qml_module(qt_ui
    URI "QtUi"
    VERSION 1.0
    QML_FILES
        qml/App.qml
        qml/LocalFileDialog.qml
        qml/TopBar.qml
        qml/SourcePanel.qml
        qml/ProcessingViewport.qml
        qml/AlgorithmInspector.qml
        qml/ParameterEditor.qml
        qml/BottomTransport.qml
    SOURCES
        include/EditCommand.h
        include/AlgorithmModel.h
        src/AlgorithmModel.cpp
        include/ImageBuffer.h
        include/ImageIoService.h
        src/ImageIoService.cpp
        include/VideoInputService.h
        src/VideoInputService.cpp
        include/ProcessingController.h
        src/ProcessingController.cpp
        include/ProcessingViewportItem.h
        src/ProcessingViewportItem.cpp
        include/ImageProcessorCore.h
        src/ImageProcessorCore.cpp
        include/GpuEffectPipeline.h
        src/GpuEffectPipeline.cpp
)

target_include_directories(qt_ui PRIVATE include)
target_include_directories(qt_ui PRIVATE "${FFMPEG_PREFIX}/include")

target_link_libraries(qt_ui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB}
        x264_compat
)

set_target_properties(qt_ui PROPERTIES
    BUILD_RPATH "${FFMPEG_PREFIX}/lib;${CMAKE_BINARY_DIR}"
    LINK_FLAGS "-Wl,-rpath-link,${FFMPEG_PREFIX}/lib,--allow-shlib-undefined"
)

# ImageProcessorCore unit tests
find_package(Qt6 OPTIONAL_COMPONENTS Test)
if(Qt6Test_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    qt_add_executable(tst_ImageProcessorCore
        tests/tst_ImageProcessorCore.cpp
        include/ImageProcessorCore.h
        src/ImageProcessorCore.cpp
        include/ImageBuffer.h
    )
    target_include_directories(tst_ImageProcessorCore PRIVATE include)
    target_link_libraries(tst_ImageProcessorCore PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_ImageProcessorCore COMMAND tst_ImageProcessorCore)

    qt_add_executable(tst_ImageIoService
        tests/tst_ImageIoService.cpp
        include/ImageIoService.h
        src/ImageIoService.cpp
        include/ImageBuffer.h
    )
    target_include_directories(tst_ImageIoService PRIVATE include)
    target_link_libraries(tst_ImageIoService PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME tst_ImageIoService COMMAND tst_ImageIoService)
    set_tests_properties(tst_ImageIoService PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(tst_ProcessingController
        tests/tst_ProcessingController.cpp
        include/EditCommand.h
        include/AlgorithmModel.h
        src/AlgorithmModel.cpp
        include/ImageBuffer.h
        include/ImageIoService.h
        src/ImageIoService.cpp
        include/VideoInputService.h
        src/VideoInputService.cpp
        include/ProcessingController.h
        src/ProcessingController.cpp
        include/ImageProcessorCore.h
        src/ImageProcessorCore.cpp
        include/GpuEffectPipeline.h
        src/GpuEffectPipeline.cpp
    )
    target_include_directories(tst_ProcessingController PRIVATE include)
    target_include_directories(tst_ProcessingController PRIVATE "${FFMPEG_PREFIX}/include")
    target_link_libraries(tst_ProcessingController PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::OpenGL Qt6::Test ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB} x264_compat)
    set_target_properties(tst_ProcessingController PROPERTIES
        BUILD_RPATH "${FFMPEG_PREFIX}/lib;${CMAKE_BINARY_DIR}"
        LINK_FLAGS "-Wl,-rpath-link,${FFMPEG_PREFIX}/lib,--allow-shlib-undefined"
    )
    add_test(NAME tst_ProcessingController COMMAND tst_ProcessingController)
    set_tests_properties(tst_ProcessingController PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
