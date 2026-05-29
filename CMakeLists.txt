cmake_minimum_required(VERSION 3.16)

project(qt_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_standard_project_setup()

qt_add_executable(qt_ui
    src/main.cpp
)

qt_add_qml_module(qt_ui
    URI "QtUi"
    VERSION 1.0
    QML_FILES
        qml/App.qml
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

target_link_libraries(qt_ui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

# ImageProcessorCore unit tests
find_package(Qt6 OPTIONAL_COMPONENTS Test)
if(Qt6Test_FOUND)
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
    target_compile_definitions(tst_ImageIoService PRIVATE TEST_IMAGE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/ui_template/desktop.webp")
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
        include/ProcessingController.h
        src/ProcessingController.cpp
        include/ImageProcessorCore.h
        src/ImageProcessorCore.cpp
        include/GpuEffectPipeline.h
        src/GpuEffectPipeline.cpp
    )
    target_include_directories(tst_ProcessingController PRIVATE include)
    target_compile_definitions(tst_ProcessingController PRIVATE TEST_IMAGE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/ui_template/desktop.webp")
    target_link_libraries(tst_ProcessingController PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::OpenGL Qt6::Test)
    add_test(NAME tst_ProcessingController COMMAND tst_ProcessingController)
    set_tests_properties(tst_ProcessingController PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
