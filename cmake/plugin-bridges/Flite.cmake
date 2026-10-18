# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# Flite plugin bridge — builds the bundled flite submodule using
# upstream's own autoconf + Makefile build, and exposes the resulting
# static library as `KitePlugins::Flite`.
#
# Rationale: a hand-curated source list breaks whenever upstream
# rearranges files or tweaks struct definitions. Delegating to
# upstream's Makefile is the only build mode the Flite maintainers
# actually test.
#
# Strategy:
#   1. Mirror libraries/flite/ into ${CMAKE_BINARY_DIR}/flite-src/ so
#      upstream's mandatory in-source build doesn't dirty the
#      submodule checkout.
#   2. Run ./configure + make + make install inside that mirror via
#      ExternalProject_Add with BUILD_IN_SOURCE=1. (Upstream's
#      Makefiles reference relative paths that only resolve when
#      $PWD == source dir.)
#   3. Expose the resulting static archives through an IMPORTED
#      INTERFACE target.
#
# Public targets:
#   KitePlugins::Flite — INTERFACE target with include dirs + libs.
#
# Downstream contract:
#   Plugins linking KitePlugins::Flite MUST add_dependencies(...,
#   KitePlugins_Flite_ep) so the EP builds before the plugin tries
#   to link the static archives. The KITE_FLITE_EP_TARGET global
#   property holds that name for convenience.

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

# -fPIC because libflite*.a ends up linked into the .mmco shared
# object. -w because upstream emits an avalanche of warnings under
# modern compilers and Kite's strict global flags would otherwise
# treat them as errors.
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

# Mirror the source tree once at configure time. Using a `cmake -E
# copy_directory` step inside the EP would work too, but doing it at
# configure time gives clearer error reporting and avoids re-copying
# on every build.
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
                          # `--with-audio=none` is essential. The
                          # plugin pipes synthesised PCM through Qt's
                          # QAudioSink rather than letting Flite touch
                          # ALSA / PulseAudio / OSS directly. Without
                          # this flag, upstream's configure auto-picks
                          # ALSA on Linux (and OSS on FreeBSD, etc.),
                          # and the resulting libflite.a pulls in
                          # `snd_pcm_*` symbols the .mmco can't
                          # resolve at dlopen() time — we deliberately
                          # do not link against libasound so the plugin
                          # stays portable across Wayland-only or
                          # PipeWire-only systems. Setting the driver
                          # to `none` keeps Flite's audio backend a
                          # stub, which is exactly what we want for
                          # a launcher-driven playback path.
                          --with-audio=none
    BUILD_COMMAND     ${_flite_make}
    INSTALL_COMMAND   ${_flite_make} install
    BUILD_BYPRODUCTS  ${_flite_lib_paths}
    LOG_CONFIGURE     ON
    LOG_BUILD         ON
    LOG_INSTALL       ON
)

# Create the install include dir up-front so target_include_directories
# doesn't complain that the path doesn't exist at configure time.
file(MAKE_DIRECTORY "${_flite_install}/include")
file(MAKE_DIRECTORY "${_flite_install}/lib")

file(MAKE_DIRECTORY "${_flite_install}/include/flite")

add_library(KitePlugins::Flite INTERFACE IMPORTED GLOBAL)
# Upstream installs headers into `<prefix>/include/flite/*.h`, so the
# canonical include directive is `#include <flite/flite.h>`. We
# expose both `<prefix>/include` and `<prefix>/include/flite` so
# either spelling works (some plugins / examples use the unprefixed
# form for historical reasons).
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
