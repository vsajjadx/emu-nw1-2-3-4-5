cmake_minimum_required(VERSION 3.21)
project(EmuLauncher VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(EMU_LAUNCHER_BUILD_TESTS "Build the detector unit tests" ON)

# Game/system detection: plain C++, no Qt.
add_library(detector STATIC launcher/Detector.cpp launcher/Detector.h)
target_include_directories(detector PUBLIC launcher)

if(EMU_LAUNCHER_BUILD_TESTS)
    enable_testing()
    add_executable(detector_test tests/detector_test.cpp)
    target_link_libraries(detector_test PRIVATE detector)
    add_test(NAME detector COMMAND detector_test)
endif()

find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    qt_add_executable(emu-launcher WIN32
        launcher/main.cpp
        launcher/MainWindow.cpp
        launcher/MainWindow.h
        launcher/EmulatorRegistry.cpp
        launcher/EmulatorRegistry.h
        launcher/Library.cpp
        launcher/Library.h
        launcher/resources.qrc
    )
    target_link_libraries(emu-launcher PRIVATE Qt6::Widgets detector)
    install(TARGETS emu-launcher RUNTIME DESTINATION .)
else()
    message(WARNING "Qt6 not found: only the detector library and its tests are built.")
endif()
