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
        qml/BottomTransport.qml
    SOURCES
        src/ImageBuffer.h
        src/ImageIoService.h
        src/ImageIoService.cpp
        src/ProcessingController.h
        src/ProcessingController.cpp
        src/ProcessingViewportItem.h
        src/ProcessingViewportItem.cpp
)

target_include_directories(qt_ui PRIVATE src)

target_link_libraries(qt_ui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
