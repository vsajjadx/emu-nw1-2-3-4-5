cmake_minimum_required(VERSION 3.21)
project(EmuLauncher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(emu-launcher
    launcher/main.cpp
    launcher/MainWindow.cpp
    launcher/MainWindow.h
    launcher/EmulatorRegistry.cpp
    launcher/EmulatorRegistry.h
    launcher/resources.qrc
)

target_link_libraries(emu-launcher PRIVATE Qt6::Widgets)
target_compile_definitions(emu-launcher PRIVATE
    EMU_LAUNCHER_ROOT="${CMAKE_SOURCE_DIR}")

install(TARGETS emu-launcher RUNTIME DESTINATION bin)
