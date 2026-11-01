# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# mmco_sign_plugin(<target> [KEY <fingerprint>] [HOMEDIR <dir>]) attaches a GPG
# signature trailer to a built .mmco via launcher/scripts/mmco_sign.py. It does
# nothing when no key is configured. With Kite_PLUGIN_SIGN_ALL=ON,
# plugins/CMakeLists.txt signs every in-tree plugin.
#
# The plugin is signed after the build and again after install, because CMake's
# RPATH rewriting and stripping change the signed bytes. KITE_SKIP_INSTALL_RESIGN=1
# skips the second step; the installed plugin then fails verification.

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

    # Re-signs the installed copy; in-tree plugins install into MMCO_MODULES_DEST_DIR.
    if(NOT DEFINED MMCO_MODULES_DEST_DIR)
        message(FATAL_ERROR
            "mmco_sign_plugin(${target}): MMCO_MODULES_DEST_DIR is not set. "
            "It is defined alongside BINARY_DEST_DIR in the top-level "
            "CMakeLists.txt; make sure that block runs before plugin "
            "subdirectories are added.")
    endif()

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
