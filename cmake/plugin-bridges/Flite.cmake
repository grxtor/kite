# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# Builds libraries/flite with upstream's autoconf + Makefile and exposes the
# static libraries as `KitePlugins::Flite`. The build runs in a mirror under
# ${CMAKE_BINARY_DIR}/flite-src because upstream only builds in-source.
#
# Plugins linking KitePlugins::Flite must add_dependencies(..., KitePlugins_Flite_ep)
# (also in the KITE_FLITE_EP_TARGET global property).

if(TARGET KitePlugins::Flite)
    return()
endif()

set(_flite_root "${CMAKE_SOURCE_DIR}/libraries/flite")

if(NOT EXISTS "${_flite_root}/configure" OR
   NOT EXISTS "${_flite_root}/include/flite.h")
    message(STATUS
        "Flite submodule not initialised at libraries/flite. "
        "The Flite plugin will not be built. "
        "Run: git submodule update --init libraries/flite")
    return()
endif()

find_program(_flite_make NAMES gmake make)
if(NOT _flite_make)
    message(STATUS
        "Flite bridge: no `make` found on PATH; skipping Flite plugin.")
    return()
endif()

include(ExternalProject)

set(_flite_mirror     "${CMAKE_BINARY_DIR}/flite-src")
set(_flite_install    "${CMAKE_BINARY_DIR}/flite-install")
set(_flite_stamps     "${CMAKE_BINARY_DIR}/flite-stamps")

set(_flite_cc "${CMAKE_C_COMPILER}")
if(NOT _flite_cc)
    set(_flite_cc "cc")
endif()

# -fPIC: the archives are linked into the .mmco shared object. -w: upstream's
# warnings would fail under Kite's strict flags.
set(_flite_cflags "-fPIC -w -O2")

set(_flite_libs
    libflite_cmu_us_kal.a
    libflite_usenglish.a
    libflite_cmulex.a
    libflite.a
)
set(_flite_lib_paths "")
foreach(_lib IN LISTS _flite_libs)
    list(APPEND _flite_lib_paths "${_flite_install}/lib/${_lib}")
endforeach()

if(NOT EXISTS "${_flite_mirror}/configure")
    file(COPY "${_flite_root}/" DESTINATION "${_flite_mirror}")
endif()

ExternalProject_Add(KitePlugins_Flite_ep
    PREFIX            "${_flite_stamps}"
    SOURCE_DIR        "${_flite_mirror}"
    BUILD_IN_SOURCE   1
    DOWNLOAD_COMMAND  ""
    UPDATE_COMMAND    ""
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
                          "CC=${_flite_cc}"
                          "CFLAGS=${_flite_cflags}"
                          "${_flite_mirror}/configure"
                          --prefix=${_flite_install}
                          # Playback goes through QAudioSink. Otherwise configure picks
                          # ALSA and libflite.a pulls in snd_pcm_* symbols the .mmco
                          # cannot resolve at dlopen().
                          --with-audio=none
    BUILD_COMMAND     ${_flite_make}
    INSTALL_COMMAND   ${_flite_make} install
    BUILD_BYPRODUCTS  ${_flite_lib_paths}
    LOG_CONFIGURE     ON
    LOG_BUILD         ON
    LOG_INSTALL       ON
)

# The directories must exist at configure time for target_include_directories.
file(MAKE_DIRECTORY "${_flite_install}/include")
file(MAKE_DIRECTORY "${_flite_install}/lib")

file(MAKE_DIRECTORY "${_flite_install}/include/flite")

add_library(KitePlugins::Flite INTERFACE IMPORTED GLOBAL)
# Both spellings work: <flite/flite.h> and <flite.h>.
target_include_directories(KitePlugins::Flite INTERFACE
    "${_flite_install}/include"
    "${_flite_install}/include/flite"
)
target_link_libraries(KitePlugins::Flite INTERFACE
    ${_flite_lib_paths}
    m
)

set_property(GLOBAL PROPERTY KITE_FLITE_EP_TARGET KitePlugins_Flite_ep)

message(STATUS
    "Flite bridge configured: libraries/flite -> KitePlugins::Flite "
    "(built via upstream autoconf, installed to ${_flite_install})")
