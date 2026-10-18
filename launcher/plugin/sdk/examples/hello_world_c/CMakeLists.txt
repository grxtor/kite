# Example CMakeLists.txt for an out-of-tree Kite plugin (.mmco) in PLAIN C
#
# Unlike the C++ example, this one needs NO Qt: it links the Qt-free
# Kite::SDK_C target and compiles a single .c file.
#
# Prerequisites:
#   1. Install Kite with SDK headers:
#        cmake --install <build-dir> --prefix /usr/local
#   (No Qt development packages are required to build a C plugin.)
#
# Build:
#   cmake -B build -DCMAKE_PREFIX_PATH="/usr/local"
#   cmake --build build
#
# Install:
#   cp build/HelloWorldC.mmco ~/.local/lib/mmcmodules/

cmake_minimum_required(VERSION 3.21)
project(HelloWorldC LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)

find_package(Kite_SDK REQUIRED)

add_library(HelloWorldC MODULE
    hello_world.c
)

target_link_libraries(HelloWorldC PRIVATE
    Kite::SDK_C            # C ABI only — no Qt
)

set_target_properties(HelloWorldC PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
)
