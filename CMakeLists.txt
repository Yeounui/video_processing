cmake_minimum_required(VERSION 3.16)

project(qt_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)

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
        include/ProcessingBackend.h
        src/ProcessingBackend.cpp
        include/OpenCvImageBridge.h
        src/OpenCvImageBridge.cpp
        include/GpuEffectPipeline.h
        src/GpuEffectPipeline.cpp
)

target_include_directories(qt_ui PRIVATE include)
target_include_directories(qt_ui PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(qt_ui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        ${OpenCV_LIBS}
)

# ImageProcessorCore unit tests
find_package(Qt6 OPTIONAL_COMPONENTS Test)
if(Qt6Test_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    qt_add_executable(tst_ImageProcessorCore
        tests/tst_ImageProcessorCore.cpp
        include/ImageProcessorCore.h
        src/ImageProcessorCore.cpp
        include/OpenCvImageBridge.h
        src/OpenCvImageBridge.cpp
        include/ImageBuffer.h
    )
    target_include_directories(tst_ImageProcessorCore PRIVATE include)
    target_include_directories(tst_ImageProcessorCore PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(tst_ImageProcessorCore PRIVATE Qt6::Core Qt6::Test ${OpenCV_LIBS})
    add_test(NAME tst_ImageProcessorCore COMMAND tst_ImageProcessorCore)

    qt_add_executable(tst_OpenCvImageBridge
        tests/tst_OpenCvImageBridge.cpp
        include/OpenCvImageBridge.h
        src/OpenCvImageBridge.cpp
        include/ImageBuffer.h
    )
    target_include_directories(tst_OpenCvImageBridge PRIVATE include)
    target_include_directories(tst_OpenCvImageBridge PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(tst_OpenCvImageBridge PRIVATE Qt6::Core Qt6::Test ${OpenCV_LIBS})
    add_test(NAME tst_OpenCvImageBridge COMMAND tst_OpenCvImageBridge)

    qt_add_executable(tst_ImageIoService
        tests/tst_ImageIoService.cpp
        include/ImageIoService.h
        src/ImageIoService.cpp
        include/OpenCvImageBridge.h
        src/OpenCvImageBridge.cpp
        include/ImageBuffer.h
    )
    target_include_directories(tst_ImageIoService PRIVATE include)
    target_include_directories(tst_ImageIoService PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(tst_ImageIoService PRIVATE Qt6::Core Qt6::Gui Qt6::Test ${OpenCV_LIBS})
    add_test(NAME tst_ImageIoService COMMAND tst_ImageIoService)
    set_tests_properties(tst_ImageIoService PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(tst_VideoInputService
        tests/tst_VideoInputService.cpp
        include/VideoInputService.h
        src/VideoInputService.cpp
        include/OpenCvImageBridge.h
        src/OpenCvImageBridge.cpp
        include/ImageBuffer.h
    )
    target_include_directories(tst_VideoInputService PRIVATE include)
    target_include_directories(tst_VideoInputService PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(tst_VideoInputService PRIVATE Qt6::Core Qt6::Test ${OpenCV_LIBS})
    add_test(NAME tst_VideoInputService COMMAND tst_VideoInputService)

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
        include/ProcessingViewportItem.h
        src/ProcessingViewportItem.cpp
        include/ImageProcessorCore.h
        src/ImageProcessorCore.cpp
        include/ProcessingBackend.h
        src/ProcessingBackend.cpp
        include/OpenCvImageBridge.h
        src/OpenCvImageBridge.cpp
        include/GpuEffectPipeline.h
        src/GpuEffectPipeline.cpp
    )
    target_include_directories(tst_ProcessingController PRIVATE include)
    target_include_directories(tst_ProcessingController PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(tst_ProcessingController PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::Test ${OpenCV_LIBS})
    add_test(NAME tst_ProcessingController COMMAND tst_ProcessingController)
    set_tests_properties(tst_ProcessingController PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_test(NAME qt_ui_cpu_reference_startup COMMAND qt_ui --startup-check)
    set_tests_properties(qt_ui_cpu_reference_startup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_UI_PROCESSING_BACKEND=cpu-reference;QT_QUICK_BACKEND=software"
        PASS_REGULAR_EXPRESSION "processing-backend=cpu-reference"
    )

    add_test(NAME qt_ui_software_startup_uses_cpu_reference COMMAND qt_ui --startup-check)
    set_tests_properties(qt_ui_software_startup_uses_cpu_reference PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        PASS_REGULAR_EXPRESSION "processing-backend=cpu-reference"
    )
endif()
