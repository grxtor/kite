# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# MMCOSign.cmake — attach a GPG signature trailer to a built .mmco
# target during the build, via scripts/mmco_sign.py.
#
# Usage:
#
#   include(MMCOSign)
#   mmco_sign_plugin(MyPluginTarget
#       KEY     "DEADBEEF..."         # explicit GPG fingerprint (optional)
#       HOMEDIR "${CMAKE_SOURCE_DIR}/.gnupg"  # optional
#   )
#
# The signing pipeline has TWO stages:
#
#   1. POST_BUILD custom command — signs the staged .mmco under
#      ${KITE_PLUGIN_STAGING_DIR} so the in-tree run finds a signed
#      plugin straight out of the build directory.
#
#   2. install(CODE …) hook — re-signs the *installed* copy after
#      CMake has done its RPATH-rewriting / debug-stripping surgery.
#      That surgery rewrites bytes inside the GPG-signed payload, so
#      without this step the installed plugin would always fail
#      verification with BadSignature.
#
# Both steps use the same key + GnuPG homedir. The function is a
# no-op when no key is configured — unsigned developer builds keep
# working, and the launcher will only enforce signatures on non-OSS
# modules at runtime.
#
# Re-signing at install time can be suppressed with the environment
# variable KITE_SKIP_INSTALL_RESIGN=1 — useful for distro packagers
# who don't have the upstream signing key handy. The installed plugin
# will then fail signature verification at runtime (which is the
# correct fail-safe behaviour for a plugin without a valid trailer).
#
# Cache-driven workflow for CI / release builds:
#
#   cmake -DKite_PLUGIN_SIGNING_KEY=<fpr>             \
#         -DKite_PLUGIN_SIGNING_HOMEDIR=<dir>         \
#         -DKite_PLUGIN_SIGN_ALL=ON                   \
#         ...
#
# When Kite_PLUGIN_SIGN_ALL is ON, plugins/CMakeLists.txt walks every
# in-tree .mmco target and calls mmco_sign_plugin() on it automatically.

include_guard(GLOBAL)

set(Kite_PLUGIN_SIGNING_KEY "" CACHE STRING
    "Default GPG key fingerprint used to sign .mmco plugins. Empty disables signing.")
set(Kite_PLUGIN_SIGNING_HOMEDIR "" CACHE PATH
    "Optional GnuPG home directory used when signing .mmco plugins.")
set(Kite_PLUGIN_SIGN_ALL OFF CACHE BOOL
    "If ON, every in-tree .mmco plugin is automatically signed POST_BUILD.")

find_package(Python3 COMPONENTS Interpreter QUIET)

function(mmco_sign_plugin target)
    set(_options)
    set(_one_value KEY HOMEDIR)
    set(_multi_value)
    cmake_parse_arguments(_arg "${_options}" "${_one_value}"
                          "${_multi_value}" ${ARGN})

    if(NOT _arg_KEY)
        set(_arg_KEY "${Kite_PLUGIN_SIGNING_KEY}")
    endif()
    if(NOT _arg_HOMEDIR)
        set(_arg_HOMEDIR "${Kite_PLUGIN_SIGNING_HOMEDIR}")
    endif()

    if(NOT _arg_KEY)
        # No key configured → quietly skip. The launcher will only
        # complain at runtime if the plugin's license is not OSS.
        return()
    endif()

    if(NOT TARGET ${target})
        message(WARNING "mmco_sign_plugin: target '${target}' does not exist")
        return()
    endif()

    if(NOT Python3_Interpreter_FOUND)
        message(WARNING
            "mmco_sign_plugin(${target}): Python 3 interpreter not found, "
            "skipping signature step. Install python3 to enable signing.")
        return()
    endif()

    set(_script "${CMAKE_SOURCE_DIR}/launcher/scripts/mmco_sign.py")
    if(NOT EXISTS "${_script}")
        message(WARNING
            "mmco_sign_plugin(${target}): ${_script} not found, "
            "skipping signature step.")
        return()
    endif()

    # ─── 1. Build-tree signing (POST_BUILD) ─────────────────────────
    #
    # Sign the staged artefact under ${KITE_PLUGIN_STAGING_DIR} so
    # the in-tree run (e.g. `./build/launcher/kite`) can discover
    # signed plugins straight out of the build directory.
    set(_cmd
        "${Python3_EXECUTABLE}" "${_script}"
        --key "${_arg_KEY}"
    )
    if(_arg_HOMEDIR)
        list(APPEND _cmd --homedir "${_arg_HOMEDIR}")
    endif()
    list(APPEND _cmd "$<TARGET_FILE:${target}>")

    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${_cmd}
        COMMENT "Signing .mmco module ${target} with key ${_arg_KEY}"
        VERBATIM
    )

    # ─── 2. Install-tree re-signing ─────────────────────────────────
    #
    # When CMake installs a MODULE library it may:
    #
    #   • Rewrite RPATH / RUNPATH (chrpath / patchelf on Linux,
    #     install_name_tool on macOS) so the installed copy resolves
    #     dependencies relative to the install prefix instead of the
    #     build tree, and
    #   • Strip debug symbols (CMAKE_INSTALL_DO_STRIP=ON, cpack-driven
    #     builds, distro packaging).
    #
    # Both operations rewrite bytes inside the very same payload that
    # the POST_BUILD signature covers, so the trailer we attached at
    # build time is no longer valid against the installed file. The
    # detached GPG signature can't accommodate that — it's a hash of
    # the exact payload bytes.
    #
    # The fix is to re-sign at install time, *after* CMake has done
    # all its RPATH / strip surgery. We schedule a CODE block that
    # locates the freshly-installed file (honouring DESTDIR + prefix)
    # and invokes mmco_sign.py against it in place.
    #
    # Skipping the re-sign is supported via the environment variable
    # KITE_SKIP_INSTALL_RESIGN=1 — useful when packagers don't have
    # the signing key handy. The launcher will then reject the
    # installed plugin as BadSignature at load time, which is the
    # correct fail-safe.
    #
    # In-tree plugins all share the convention
    #   LIBRARY DESTINATION "${MMCO_MODULES_DEST_DIR}"
    # (defined alongside BINARY_DEST_DIR in the top-level CMakeLists)
    # so we can predict the install path at configure time. If you
    # break that convention in a custom plugin, override the
    # destination with an explicit DESTINATION argument to
    # mmco_sign_plugin (left as a TODO for now — drop us an issue
    # if you need it).
    #
    # MMCO_MODULES_DEST_DIR is platform-aware:
    #   Linux    -> bin/mmcmodules
    #   Windows  -> ./mmcmodules
    #   macOS    -> Kite.app/Contents/PlugIns/mmcmodules
    # The macOS branch deliberately routes around Contents/MacOS to
    # avoid colliding with Apple's `codesign --strict` subcomponent
    # treatment of Mach-O files found there.
    if(NOT DEFINED MMCO_MODULES_DEST_DIR)
        message(FATAL_ERROR
            "mmco_sign_plugin(${target}): MMCO_MODULES_DEST_DIR is not set. "
            "It is defined alongside BINARY_DEST_DIR in the top-level "
            "CMakeLists.txt; make sure that block runs before plugin "
            "subdirectories are added.")
    endif()

    # Escape backslashes so the CODE template survives generation
    # on Windows paths.
    set(_resign_script      "${_script}")
    set(_resign_python      "${Python3_EXECUTABLE}")
    set(_resign_key         "${_arg_KEY}")
    set(_resign_homedir     "${_arg_HOMEDIR}")
    set(_resign_modules_dir "${MMCO_MODULES_DEST_DIR}")

    install(CODE
"
        set(_mmco_resign_target_name \"$<TARGET_FILE_NAME:${target}>\")
        set(_mmco_resign_install_dir
            \"\${CMAKE_INSTALL_PREFIX}/${_resign_modules_dir}\")
        if(DEFINED ENV{DESTDIR} AND NOT \"\$ENV{DESTDIR}\" STREQUAL \"\")
            set(_mmco_resign_install_dir
                \"\$ENV{DESTDIR}\${_mmco_resign_install_dir}\")
        endif()
        set(_mmco_resign_path
            \"\${_mmco_resign_install_dir}/\${_mmco_resign_target_name}\")

        if(NOT EXISTS \"\${_mmco_resign_path}\")
            message(WARNING
                \"mmco_sign_plugin(${target}): installed file '\${_mmco_resign_path}' \"
                \"not found; signature will not be re-applied. The build-tree \"
                \"signature is now stale.\")
        elseif(DEFINED ENV{KITE_SKIP_INSTALL_RESIGN}
               AND \"\$ENV{KITE_SKIP_INSTALL_RESIGN}\" STREQUAL \"1\")
            message(STATUS
                \"mmco_sign_plugin(${target}): KITE_SKIP_INSTALL_RESIGN=1, leaving \"
                \"build-time trailer attached to '\${_mmco_resign_path}' \"
                \"(the launcher will reject it as BadSignature).\")
        else()
            set(_mmco_resign_cmd
                \"${_resign_python}\" \"${_resign_script}\"
                --key \"${_resign_key}\")
            if(NOT \"${_resign_homedir}\" STREQUAL \"\")
                list(APPEND _mmco_resign_cmd
                    --homedir \"${_resign_homedir}\")
            endif()
            list(APPEND _mmco_resign_cmd \"\${_mmco_resign_path}\")

            message(STATUS
                \"Re-signing installed plugin: \${_mmco_resign_path}\")
            execute_process(
                COMMAND \${_mmco_resign_cmd}
                RESULT_VARIABLE _mmco_resign_rc
            )
            if(NOT _mmco_resign_rc EQUAL 0)
                message(FATAL_ERROR
                    \"mmco_sign_plugin(${target}): re-signing the installed copy \"
                    \"failed (exit \${_mmco_resign_rc}). Set \"
                    \"KITE_SKIP_INSTALL_RESIGN=1 to override (the installed plugin \"
                    \"will then fail signature verification at runtime).\")
            endif()
        endif()
")
endfunction()
