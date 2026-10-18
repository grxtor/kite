# Example CMakeLists.txt for an out-of-tree Kite plugin (.mmco)
#
# Prerequisites:
#   1. Install Kite with SDK headers:
#        cmake --install <build-dir> --prefix /usr/local
#   2. Install Qt 6 development packages (Core, Widgets, Gui)
#
# Build:
#   cmake -B build -DCMAKE_PREFIX_PATH="/usr/local;/usr/lib/qt6"
#   cmake --build build
#
# Install:
#   cp build/MyPlugin.mmco ~/.local/lib/mmcmodules/

cmake_minimum_required(VERSION 3.21)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Must match the Qt major version Kite and its SDK were built against;
# the exported Kite::SDK target names Qt<major>::* targets concretely.
# The installed SDK config reports what it was built with in
# Kite_SDK_QT_VERSION_MAJOR.
set(Kite_QT_VERSION_MAJOR "6" CACHE STRING "Major Qt version to build against (5 or 6)")
set(QT_VERSION_MAJOR "${Kite_QT_VERSION_MAJOR}")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Gui)
find_package(Kite_SDK REQUIRED)

add_library(MyPlugin MODULE
    src/MyPlugin.cpp
)

target_link_libraries(MyPlugin PRIVATE
    Kite::SDK
)

set_target_properties(MyPlugin PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
)
