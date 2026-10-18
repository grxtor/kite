# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

project(kite-crashreporter)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

# Optional: libqrencode for QR code generation. Without it the dialog simply
# leaves the QR code out (see KITE_HAS_QRENCODE below).
#
# It is declared in the vcpkg manifest at the repo root, so under the presets
# it is installed — but on MSVC there is no pkg-config to find it with, so the
# headers and the library are looked up directly there. Everywhere else
# pkg-config sees both the vcpkg copy and a system one and hands over the right
# flags. Either way the result is one target name, or none.
set(KITE_QRENCODE_TARGET "")
if(MSVC)
    find_path(QRENCODE_INCLUDE_DIR qrencode.h)
    find_library(QRENCODE_LIBRARY qrencode)
    if(QRENCODE_INCLUDE_DIR AND QRENCODE_LIBRARY)
        add_library(kite_qrencode INTERFACE)
        target_include_directories(kite_qrencode INTERFACE "${QRENCODE_INCLUDE_DIR}")
        target_link_libraries(kite_qrencode INTERFACE "${QRENCODE_LIBRARY}")
        set(KITE_QRENCODE_TARGET kite_qrencode)
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(QRENCODE IMPORTED_TARGET QUIET libqrencode)
        if(QRENCODE_FOUND)
            set(KITE_QRENCODE_TARGET PkgConfig::QRENCODE)
        endif()
    endif()
endif()

# On Windows: use Win32 subsystem so no console window appears.
if(WIN32)
    set(CRASHREPORTER_WIN32_FLAG WIN32)
else()
    set(CRASHREPORTER_WIN32_FLAG "")
endif()

add_executable(kite-crashreporter ${CRASHREPORTER_WIN32_FLAG}
    main.cpp
    CrashReportDialog.h
    CrashReportDialog.cpp
)

target_link_libraries(kite-crashreporter
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

if(APPLE)
    set_target_properties(kite-crashreporter PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

if(KITE_QRENCODE_TARGET)
    target_compile_definitions(kite-crashreporter PRIVATE KITE_HAS_QRENCODE)
    target_link_libraries(kite-crashreporter PRIVATE ${KITE_QRENCODE_TARGET})
endif()

# Install alongside the main binary.
install(TARGETS kite-crashreporter
    RUNTIME DESTINATION "${BINARY_DEST_DIR}"
)
