cmake_minimum_required(VERSION 3.24)
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

# Required for CMAKE_MSVC_DEBUG_INFORMATION_FORMAT to work (avoids /Zi PDB conflicts with sccache)
cmake_policy(SET CMP0141 NEW)

# Lets CMAKE_MSVC_RUNTIME_LIBRARY control the CRT. Must be set before project().
cmake_policy(SET CMP0091 NEW)

# Keeps find_package() from finding a previous `cmake --install` into
# <source>/install (a half-installed ECM broke configure). Must be set before
# project().
set(CMAKE_FIND_NO_INSTALL_PREFIX TRUE)

project(Kite)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ECM NO_MODULE REQUIRED)
set(CMAKE_MODULE_PATH "${ECM_MODULE_PATH};${CMAKE_MODULE_PATH}")

include(CTest)
include(ECMAddTests)
if(BUILD_TESTING)
    enable_testing()

    # The script the release workflow makes and signs the update feeds with: its tests use a fake
    # release, so they run with every build rather than only on a tag.
    find_package(Python3 3.9 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME UpdateFeedScript
            COMMAND "${Python3_EXECUTABLE}" -I -B -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/packaging/update-feed" -t "${CMAKE_CURRENT_SOURCE_DIR}/packaging/update-feed")
    endif()
endif()

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BUILD_DIR}" IS_IN_SOURCE_BUILD)
if(IS_IN_SOURCE_BUILD)
    message(FATAL_ERROR "You are building Kite in-source. Please separate the build tree from the source tree.")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_HOST_SYSTEM_VERSION MATCHES ".*[Mm]icrosoft.*" OR
        CMAKE_HOST_SYSTEM_VERSION MATCHES ".*WSL.*"
    )
        message(FATAL_ERROR "Building Kite is not supported in Linux-on-Windows distributions.")
    endif()
endif()

set(Kite_COMPILER_NAME ${CMAKE_CXX_COMPILER_ID})
set(Kite_COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION})
set(Kite_COMPILER_TARGET_SYSTEM ${CMAKE_SYSTEM_NAME})
set(Kite_COMPILER_TARGET_SYSTEM_VERSION ${CMAKE_SYSTEM_VERSION})
set(Kite_COMPILER_TARGET_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})

##################################### Set CMake options #####################################
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/;${CMAKE_MODULE_PATH}")

# Output all executables and shared libs in the main build folder, not in subfolders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
if(UNIX)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()
set(CMAKE_JAVA_TARGET_OUTPUT_DIR ${PROJECT_BINARY_DIR}/jars)

######## Set compiler flags ########
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

include(GenerateExportHeader)
if(MSVC)
    set(CMAKE_CXX_FLAGS " /W4 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DQT_NO_DEPRECATED_WARNINGS=Y")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release "")
else()
    # FIXME: readd -Werror flag
    set(CMAKE_CXX_FLAGS " -Wall -pedantic -Wno-deprecated-declarations -fstack-protector-strong --param=ssp-buffer-size=4 -O3 -D_FORTIFY_SOURCE=2 ${CMAKE_CXX_FLAGS}")
    if(UNIX AND APPLE)
        set(CMAKE_CXX_FLAGS " -stdlib=libc++ ${CMAKE_CXX_FLAGS}")
    endif()
    # FIXME: readd -Werror=return-type for Debug
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_NO_DEPRECATED_WARNINGS=Y")
endif()

include(ECMQtDeclareLoggingCategory)

option(ENABLE_LTO "Enable Link Time Optimization" off)
option(Kite_DISABLE_JAVA_DOWNLOADER "Disable the built-in Java downloader feature" OFF)
option(Kite_ENABLE_CLANG_TIDY "Run clang-tidy during C/C++ compilation" OFF)
option(Kite_PLUGINS "Build plugins" OFF)
option(Kite_STAGING_PLUGINS "Build Staging plugins" OFF)

find_program(Kite_CLANG_TIDY_EXECUTABLE NAMES clang-tidy clang-tidy-19 OPTIONAL)
if(Kite_ENABLE_CLANG_TIDY)
    if(Kite_CLANG_TIDY_EXECUTABLE)
        set(CMAKE_C_CLANG_TIDY
            "${Kite_CLANG_TIDY_EXECUTABLE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
        set(CMAKE_CXX_CLANG_TIDY
            "${Kite_CLANG_TIDY_EXECUTABLE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
        message(STATUS "clang-tidy enabled: ${Kite_CLANG_TIDY_EXECUTABLE}")
    else()
        message(WARNING "Kite_ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
    endif()
endif()

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
        if(CMAKE_BUILD_TYPE)
            if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
                message(STATUS "IPO / LTO enabled")
            else()
                message(STATUS "Not enabling IPO / LTO on debug builds")
            endif()
        else()
            message(STATUS "IPO / LTO will only be enabled for release builds")
        endif()
    else()
        message(STATUS "IPO / LTO not supported: <${ipo_error}>")
    endif()
endif()

################################ 3rd Party Libs ################################

# Qt 6 only. Subdirectories reach Qt through ${QT_VERSION_MAJOR}.
set(QT_VERSION_MAJOR 6)

# A cache variable so that -DKite_QT_VERSION_MAJOR=5 fails loudly.
set(Kite_QT_VERSION_MAJOR "6" CACHE STRING "Major Qt version to build against (6 only)")
set_property(CACHE Kite_QT_VERSION_MAJOR PROPERTY STRINGS "6")

if(NOT Kite_QT_VERSION_MAJOR STREQUAL "6")
    message(FATAL_ERROR
        "Kite requires Qt 6; Qt ${Kite_QT_VERSION_MAJOR} is no longer supported. "
        "The QML user interface depends on Qt 6 modules.")
endif()

# Windows: Qt loads OpenSSL at run time, so deployment misses it. MSYS2's Qt
# needs the 3.x ABI (libssl-3-x64.dll, libcrypto-3-x64.dll). CI fills this in.
set(Kite_BUNDLE_OPENSSL_LIBS "" CACHE STRING
    "Semicolon-separated list of OpenSSL runtime libraries to ship inside the package")

set(Kite_QT_COMPONENTS
    Core
    Widgets
    Concurrent
    Network
    NetworkAuth
    Test
    Xml
    Qml
    Quick
    QuickControls2
    # The QML icons are SVG files, read by the Qt SVG image plugin.
    Svg
    # The friends server's realtime socket (launcher/friends/FriendsSocket.cpp).
    WebSockets
)

# Debian 12 and Ubuntu 24.04 ship Qt 6.4.
set(Kite_QT_MINIMUM_VERSION "6.4.0")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${Kite_QT_COMPONENTS})

if(DEFINED Qt${QT_VERSION_MAJOR}Core_VERSION)
    set(_projt_qt_version "${Qt${QT_VERSION_MAJOR}Core_VERSION}")
elseif(DEFINED Qt${QT_VERSION_MAJOR}_VERSION)
    set(_projt_qt_version "${Qt${QT_VERSION_MAJOR}_VERSION}")
endif()
if(DEFINED _projt_qt_version AND _projt_qt_version VERSION_LESS "${Kite_QT_MINIMUM_VERSION}")
    message(FATAL_ERROR "Qt ${Kite_QT_MINIMUM_VERSION} or newer is required. Detected: ${_projt_qt_version}")
endif()
message(STATUS "Building against Qt ${_projt_qt_version} (major version ${QT_VERSION_MAJOR})")
unset(_projt_qt_version)

# Qt Quick 3D (3D skin preview) is optional, so it is not in Kite_QT_COMPONENTS.
# Kite_ENABLE_SKIN3D can turn it off by hand, never on.
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Quick3D)
option(Kite_ENABLE_SKIN3D "Build the 3D skin preview (needs Qt Quick 3D)" ${Qt${QT_VERSION_MAJOR}Quick3D_FOUND})
if(Kite_ENABLE_SKIN3D AND NOT Qt${QT_VERSION_MAJOR}Quick3D_FOUND)
    message(WARNING "Kite_ENABLE_SKIN3D is ON but Qt Quick3D was not found; building without the 3D skin preview.")
    set(Kite_ENABLE_SKIN3D OFF)
endif()
message(STATUS "3D skin preview (Qt Quick3D): ${Kite_ENABLE_SKIN3D}")

# lrelease compiles translations/kite_*.ts; without it the build ships English only.
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS LinguistTools)
message(STATUS "Bundled translations (Qt LinguistTools): ${Qt${QT_VERSION_MAJOR}LinguistTools_FOUND}")

if(UNIX)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_search_module(SCDOC scdoc)
        if(SCDOC_FOUND)
            pkg_get_variable(SCDOC_SCDOC scdoc scdoc)
            message(STATUS "SCDOC_FOUND=${SCDOC_FOUND}")
            message(STATUS "SCDOC_SCDOC=${SCDOC_SCDOC}")
        endif()
    endif()
endif()

include(QMakeQuery)
QUERY_QMAKE(QT_INSTALL_PLUGINS QT_PLUGINS_DIR)
QUERY_QMAKE(QT_INSTALL_LIBS QT_LIBS_DIR)
QUERY_QMAKE(QT_INSTALL_LIBEXECS QT_LIBEXECS_DIR)
# Qt's own QML modules are not compiled into the launcher; they are deployed from here.
QUERY_QMAKE(QT_INSTALL_QML QT_QML_DIR)

# The libraries are built single-config, but Ninja Multi-Config validates every
# configuration at configure time.
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release Debug "")
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL MinSizeRel Release Debug "")

find_package(cmark REQUIRED)

find_package(tomlplusplus 3.2.0)
if(NOT tomlplusplus_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET GLOBAL
                      tomlplusplus>=3.2.0)
    add_library(tomlplusplus::tomlplusplus ALIAS PkgConfig::tomlplusplus)
endif()

find_package(ZLIB REQUIRED)
find_package(LibArchive)
if(NOT LibArchive_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libarchive REQUIRED IMPORTED_TARGET GLOBAL libarchive)
    add_library(LibArchive::LibArchive ALIAS PkgConfig::libarchive)
elseif(WIN32)
    # Static libarchive calls BCrypt* on Windows and FindLibArchive does not
    # report it, so consumers fail to link with undefined BCrypt* symbols.
    # ref. https://github.com/libarchive/libarchive/issues/3375
    set_property(TARGET LibArchive::LibArchive APPEND PROPERTY INTERFACE_LINK_LIBRARIES bcrypt)
endif()

if(MSVC)
    set(_kite_default_plugin_signatures OFF)
else()
    set(_kite_default_plugin_signatures ON)
endif()
option(Kite_PLUGIN_SIGNATURES
    "Enable GPG-backed plugin signature verification (requires Gpgmepp)"
    ${_kite_default_plugin_signatures})

if(Kite_PLUGIN_SIGNATURES)
    find_package(Gpgmepp REQUIRED)
    if(TARGET Gpgmepp)
        set(Kite_GpgmeppTarget Gpgmepp)
    elseif(TARGET GpgmeppStatic)
        set(Kite_GpgmeppTarget GpgmeppStatic)
    else()
        message(FATAL_ERROR
            "find_package(Gpgmepp) succeeded but exposed neither a Gpgmepp nor "
            "a GpgmeppStatic target, so there is nothing to link the plugin "
            "signature verifier against.\n"
            "Configure with -DKite_PLUGIN_SIGNATURES=OFF to build the stub "
            "verifier instead.")
    endif()
    message(STATUS "Gpgmepp target: ${Kite_GpgmeppTarget}")

    get_target_property(_gpgmepp_loc ${Kite_GpgmeppTarget}
                        IMPORTED_LOCATION_NOCONFIG)
    get_target_property(_gpgmepp_son ${Kite_GpgmeppTarget}
                        IMPORTED_SONAME_NOCONFIG)
    if(_gpgmepp_loc)
        foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            set_property(TARGET ${Kite_GpgmeppTarget} APPEND PROPERTY
                IMPORTED_CONFIGURATIONS ${_cfg})
            set_target_properties(${Kite_GpgmeppTarget} PROPERTIES
                IMPORTED_LOCATION_${_cfg} "${_gpgmepp_loc}")
            if(_gpgmepp_son)
                set_target_properties(${Kite_GpgmeppTarget} PROPERTIES
                    IMPORTED_SONAME_${_cfg} "${_gpgmepp_son}")
            endif()
        endforeach()
    endif()
    unset(_gpgmepp_loc)
    unset(_gpgmepp_son)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

####################################### Branding #######################################

######## Git commit, branch and build timestamp ########

include(GetGitRevisionDescription)
get_git_head_revision(Kite_GIT_REFSPEC Kite_GIT_COMMIT ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)

message(STATUS "Git commit: ${Kite_GIT_COMMIT}")
message(STATUS "Git refspec: ${Kite_GIT_REFSPEC}")

string(TIMESTAMP TODAY "%Y-%m-%d")
set(Kite_BUILD_TIMESTAMP "${TODAY}")

######## Version numbers ########
set(Kite_VERSION_MAJOR    1)
set(Kite_VERSION_MINOR    2)
set(Kite_VERSION_HOTFIX   0)

set(Kite_RELEASE_VERSION_NAME "${Kite_VERSION_MAJOR}.${Kite_VERSION_MINOR}.${Kite_VERSION_HOTFIX}")
set(Kite_RELEASE_TWO_NAME "${Kite_VERSION_MAJOR}.${Kite_VERSION_MINOR}")
set(Kite_VERSION_NAME4 "${Kite_VERSION_MAJOR}.${Kite_VERSION_MINOR}.${Kite_VERSION_HOTFIX}.0")
set(Kite_VERSION_NAME4_COMMA "${Kite_VERSION_MAJOR},${Kite_VERSION_MINOR},${Kite_VERSION_HOTFIX},0")

add_custom_target(version echo "Version: ${Kite_RELEASE_VERSION_NAME}")
add_custom_target(tcversion echo "\\#\\#teamcity[setParameter name=\\'env.KITE_VERSION\\' value=\\'${Kite_RELEASE_VERSION_NAME}\\']")

######## URLs and API keys ########

set(Kite_NEWS_RSS_URL "" CACHE STRING "URL to fetch Kite's news RSS feed from.")
set(Kite_NEWS_EXTRA_FEEDS "" CACHE STRING "Semicolon-separated list of extra RSS feed URLs shown alongside the launcher's own news feed.")

set(Kite_BUILD_ARTIFACT "" CACHE STRING "Name of the CI build this is, matched against release asset names.")

set(Kite_BUILD_PLATFORM "" CACHE STRING "A short string identifying the platform that this build was built for. Only used by the notification system and to display in the about dialog.")

set(Kite_UPDATER_GITHUB_REPO "" CACHE STRING "GitHub repository the updater reads releases from. Defaults to Kite_Git.")

# kite-updater (Windows, Linux) installs a release only when its kite-update.json carries an Ed25519
# signature by this key; the release workflow signs it with the KITE_UPDATE_ED_PRIVATE_KEY secret and
# fails if that secret is not this key's. A fork publishing its own releases sets its own key. Base64
# of the 32-byte public key.
set(Kite_UPDATE_PUBLIC_KEY "6dOlOyj2F8eqxeWJSbnIYY6V1PBak2YHHn02CydQblY=" CACHE STRING "Public Ed25519 key the release manifest must be signed with (base64).")

# A beta build enables pre-releases by default; users can change that. Not
# Kite_VERSION_CHANNEL, which is the git branch name.
set(Kite_UPDATE_CHANNEL "stable" CACHE STRING "Release channel this build subscribes to: stable | beta.")
set_property(CACHE Kite_UPDATE_CHANNEL PROPERTY STRINGS stable beta)

string(TOLOWER "${Kite_UPDATE_CHANNEL}" Kite_UPDATE_CHANNEL_EFFECTIVE)
if(Kite_UPDATE_CHANNEL_EFFECTIVE STREQUAL "")
    # CMakePresets forwards an unset environment variable as an empty string.
    set(Kite_UPDATE_CHANNEL_EFFECTIVE "stable")
elseif(NOT Kite_UPDATE_CHANNEL_EFFECTIVE MATCHES "^(stable|beta)$")
    message(FATAL_ERROR
        "Kite_UPDATE_CHANNEL must be 'stable' or 'beta' (got '${Kite_UPDATE_CHANNEL}').")
endif()
message(STATUS "Update channel: ${Kite_UPDATE_CHANNEL_EFFECTIVE}")

set(Kite_NOTIFICATION_URL "" CACHE STRING "URL for checking for notifications.")

# Keep the trailing slash: the meta and Java download code append paths to it.
set(Kite_META_URL "https://meta.kiteclient.com/" CACHE STRING "URL to fetch Kite's meta files from.")

# Empty turns friends off for the build. Keep the trailing slash.
set(Kite_FRIENDS_URL "https://api.kiteclient.com/" CACHE STRING "URL of Kite's friends server.")

# Empty: Kite never downloads translations. Otherwise a server serving index_v2.json
# and the .qm files it lists.
set(Kite_TRANSLATIONS_URL "" CACHE STRING "URL to download translation updates from (empty: bundled translations only).")

# Empty hides Microsoft sign-in.
set(Kite_MICROSOFT_CLIENT_ID "3e21e33c-8db3-41f9-bbd3-8309b8c41203" CACHE STRING "Azure application (client) ID for Microsoft sign-in")

set(Kite_PASTE_EE_API_KEY "" CACHE STRING "API key you can get from paste.ee when you register an account")

set(Kite_IMGUR_CLIENT_ID "" CACHE STRING "Client ID you can get from Imgur when you register an application")

set(Kite_CURSEFORGE_API_KEY "" CACHE STRING "API key for the CurseForge API (get one at https://console.curseforge.com/)")

set(Kite_BUG_TRACKER_URL "" CACHE STRING "URL for the bug tracker.")

set(Kite_DISCORD_URL "" CACHE STRING "URL for the Discord guild.")

set(Kite_SUBREDDIT_URL "" CACHE STRING "URL for the subreddit.")

######## Kite attributes ########

set(Kite_CommonName "Kite")
set(Kite_AppBinaryName "kite")
set(Kite_AppID "io.github.grxtor.Kite")
set(Kite_Copyright "grxtor")
set(Kite_Copyright_Mac "© 2026 grxtor")
set(Kite_Domain "grxtor.github.io")
set(Kite_Name "${Kite_CommonName}")
set(Kite_DisplayName "${Kite_CommonName}")
set(Kite_UserAgent "${Kite_CommonName}/${Kite_RELEASE_TWO_NAME}")
set(Kite_ConfigFile "kite.cfg")
set(Kite_Git "https://github.com/grxtor/kite")
set(Kite_Patreon_Url "")

if(Kite_UPDATER_GITHUB_REPO STREQUAL "")
    set(Kite_UPDATER_GITHUB_REPO "${Kite_Git}")
endif()
message(STATUS "Updater repository: ${Kite_UPDATER_GITHUB_REPO}")
set(Kite_Authors "Based on MeshMC by Project Tick and MultiMC by the MultiMC Contributors. Licensed under the Apache License, Version 2.0.")

######## Configure placeholders ########

set(Kite_SVGFileName "${Kite_AppID}.svg")
set(Kite_Branding_ICNS "branding/${Kite_AppID}.icns")
set(Kite_Branding_ICO "${Kite_AppID}.ico")

configure_file(branding/${Kite_AppID}.desktop.in ${Kite_AppID}.desktop)
configure_file(branding/${Kite_AppID}.metainfo.xml.in ${Kite_AppID}.metainfo.xml)
configure_file(branding/${Kite_AppBinaryName}.rc.in ${Kite_AppBinaryName}.rc @ONLY)
configure_file(branding/${Kite_AppBinaryName}.qrc.in ${Kite_AppBinaryName}.qrc @ONLY)
configure_file(branding/${Kite_CommonName}.manifest.in ${Kite_AppBinaryName}.manifest @ONLY)
configure_file(branding/${Kite_AppID}.mime.xml ${Kite_AppID}.xml COPYONLY)
configure_file(branding/${Kite_SVGFileName} ${Kite_SVGFileName} COPYONLY)
configure_file(branding/${Kite_AppID}.ico ${Kite_AppID}.ico COPYONLY)
configure_file(branding/${Kite_AppBinaryName}.6.scd.in ${Kite_AppBinaryName}.6.scd @ONLY)
set(Kite_Portable_File "branding/portable.txt")
set(Kite_Branding_MAC_ICON "branding/${Kite_CommonName}.icon")

if(SCDOC_FOUND)
    set(in_scd "${CMAKE_CURRENT_BINARY_DIR}/${Kite_AppBinaryName}.6.scd")
    set(out_man "${CMAKE_CURRENT_BINARY_DIR}/${Kite_AppBinaryName}.6")
    add_custom_command(
        DEPENDS "${in_scd}"
        OUTPUT "${out_man}"
        COMMAND ${SCDOC_SCDOC} < "${in_scd}" > "${out_man}"
    )
    add_custom_target(man ALL DEPENDS ${out_man})
    set(Kite_ManPage "${CMAKE_CURRENT_BINARY_DIR}/${Kite_AppBinaryName}.6")
endif()

if(MSVC)
    set(Kite_MSVC_Redist_NSIS_Section [=[
!ifdef haveNScurl
Section "Visual Studio Runtime"
    Var /GLOBAL vc_redist_exe
    ${If} ${IsNativeARM64}
        StrCpy $vc_redist_exe "vc_redist.arm64.exe"
    ${Else}
        StrCpy $vc_redist_exe "vc_redist.x64.exe"
    ${EndIf}
    DetailPrint 'Downloading Microsoft Visual C++ Redistributable...'
    NScurl::http GET "https://aka.ms/vs/17/release/$vc_redist_exe" "$INSTDIR\vc_redist\$vc_redist_exe" /INSIST /CANCEL /Zone.Identifier /END
    Pop $0
    ${If} $0 == "OK"
        DetailPrint "Download successful"
        ExecWait "$INSTDIR\vc_redist\$vc_redist_exe /install /passive /norestart"
    ${Else}
        DetailPrint "Download failed with error $0"
    ${EndIf}
SectionEnd
!endif
]=])
endif()

configure_file(branding/win_install.nsi.in win_install.nsi @ONLY)

####################################### Install layout #######################################

if(NOT (UNIX AND APPLE))
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${Kite_Portable_File}" DESTINATION "." COMPONENT portable EXCLUDE_FROM_ALL)
endif()

if(UNIX AND APPLE)
    set(BINARY_DEST_DIR "${Kite_Name}.app/Contents/MacOS")
    set(FRAMEWORK_DEST_DIR "${Kite_Name}.app/Contents/Frameworks")
    set(LIBRARY_DEST_DIR "${FRAMEWORK_DEST_DIR}")
    set(PLUGIN_DEST_DIR "${Kite_Name}.app/Contents/MacOS")
    set(RESOURCES_DEST_DIR "${Kite_Name}.app/Contents/Resources")
    set(JARS_DEST_DIR "${Kite_Name}.app/Contents/Resources/jars")
    set(QML_DEST_DIR "${Kite_Name}.app/Contents/Resources/qml")
    # On macOS qt.conf's [Paths] are relative to the bundle's Contents/, not to
    # CMAKE_INSTALL_PREFIX like the install() destinations above.
    set(QT_CONF_PLUGINS_DEST "MacOS")
    set(QT_CONF_QML_DEST "Resources/qml")
    set(BUNDLE_DEST_DIR ".")
    set(MMCO_MODULES_DEST_DIR
        "${Kite_Name}.app/Contents/Resources/mmcmodules")
    set(MMCO_PLUGIN_DATA_DEST_DIR
        "${Kite_Name}.app/Contents/Resources/MMCOPluginData")

    set(MACOSX_BUNDLE_BUNDLE_NAME "${Kite_DisplayName}")
    set(MACOSX_BUNDLE_INFO_STRING "${Kite_DisplayName}: A custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once.")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "${Kite_AppID}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${Kite_RELEASE_VERSION_NAME}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${Kite_RELEASE_VERSION_NAME}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${Kite_RELEASE_VERSION_NAME}")
    set(MACOSX_BUNDLE_ICON_FILE ${Kite_AppID}.icns)
    set(MACOSX_BUNDLE_COPYRIGHT "${Kite_Copyright_Mac}")
    # Lets macOS refuse a build for a newer system instead of the app crashing inside Qt.
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION_KEY
            "<key>LSMinimumSystemVersion</key>\n    <string>${CMAKE_OSX_DEPLOYMENT_TARGET}</string>")
    endif()
    # Sparkle installs an update only when its archive carries an EdDSA signature by this key
    # (SUPublicEDKey); the release workflow signs with the SPARKLE_ED_PRIVATE_KEY secret and fails if
    # that secret is not this key's. Changing it strands every installed copy: Sparkle accepts a new
    # key only from an update signed with the old one.
    set(MACOSX_SPARKLE_UPDATE_PUBLIC_KEY "1xiINdScFsbf7TYDhMqXWSVkZKl3/VWODTwwa6f4Z0o=" CACHE STRING "Public key for Sparkle update feed")
    # Empty: the appcast the release workflow attaches to the latest GitHub release for this build
    # (appcast-<Kite_BUILD_ARTIFACT>.xml). A local build has no artifact name and so no updater.
    set(MACOSX_SPARKLE_UPDATE_FEED_URL "" CACHE STRING "URL for Sparkle update feed")
    if(MACOSX_SPARKLE_UPDATE_FEED_URL STREQUAL "" AND NOT Kite_BUILD_ARTIFACT STREQUAL "")
        set(MACOSX_SPARKLE_UPDATE_FEED_URL
            "${Kite_UPDATER_GITHUB_REPO}/releases/latest/download/appcast-${Kite_BUILD_ARTIFACT}.xml")
    endif()

    # Pinned, not cache variables: a build directory configured before a bump must not keep fetching
    # the old archive. The directory is versioned for the same reason.
    set(MACOSX_SPARKLE_VERSION "2.10.0")
    set(MACOSX_SPARKLE_DOWNLOAD_URL "https://github.com/sparkle-project/Sparkle/releases/download/${MACOSX_SPARKLE_VERSION}/Sparkle-${MACOSX_SPARKLE_VERSION}.tar.xz")
    set(MACOSX_SPARKLE_SHA256 "c2bf58aa8387266ac179357b1415d6f2635f044da8be41042af32425dae6da0c")
    set(MACOSX_SPARKLE_DIR "${CMAKE_BINARY_DIR}/frameworks/Sparkle-${MACOSX_SPARKLE_VERSION}")

    install(FILES ${Kite_Branding_ICNS} DESTINATION ${RESOURCES_DEST_DIR} RENAME ${Kite_AppID}.icns)

    find_program(ACTOOL_EXE actool DOC "Path to the apple asset catalog compiler")
    if(ACTOOL_EXE)
        execute_process(
            COMMAND xcodebuild -version
            OUTPUT_VARIABLE XCODE_VERSION_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        string(REGEX MATCH "Xcode ([0-9]+\.[0-9]+)" XCODE_VERSION_MATCH "${XCODE_VERSION_OUTPUT}")
        if(XCODE_VERSION_MATCH)
            set(XCODE_VERSION ${CMAKE_MATCH_1})
        else()
            set(XCODE_VERSION 0.0)
        endif()

        if(XCODE_VERSION VERSION_GREATER_EQUAL 26.0)
            set(ASSETS_OUT_DIR "${CMAKE_BINARY_DIR}/branding")
            set(GENERATED_ASSETS_CAR "${ASSETS_OUT_DIR}/Assets.car")
            set(ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${Kite_Branding_MAC_ICON}")

            add_custom_command(
                OUTPUT "${GENERATED_ASSETS_CAR}"
                COMMAND ${ACTOOL_EXE} "${ICON_SOURCE}"
                        --compile "${ASSETS_OUT_DIR}"
                        --output-partial-info-plist /dev/null
                        --app-icon ${Kite_Name}
                        --enable-on-demand-resources NO
                        --target-device mac
                        --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
                        --platform macosx
                DEPENDS "${ICON_SOURCE}"
                COMMENT "Compiling asset catalog (${ICON_SOURCE})"
                VERBATIM
            )
            add_custom_target(compile_assets ALL DEPENDS "${GENERATED_ASSETS_CAR}")
            install(FILES "${GENERATED_ASSETS_CAR}" DESTINATION "${RESOURCES_DEST_DIR}")
        else()
            message(WARNING "Xcode ${XCODE_VERSION} is too old. Minimum required version is 26.0. Not compiling liquid glass icons.")
        endif()

    else()
        message(WARNING "actool not found. Cannot compile macOS app icons.\n"
                "Install Xcode command line tools: 'xcode-select --install'")
    endif()


elseif(UNIX)
    include(GNUInstallDirs)

    set(BINARY_DEST_DIR "${CMAKE_INSTALL_BINDIR}")
    set(LIBRARY_DEST_DIR "${CMAKE_INSTALL_LIBDIR}")
    set(JARS_DEST_DIR "share/${Kite_Name}")
    set(MMCO_MODULES_DEST_DIR "share/${Kite_Name}/mmcmodules")
    set(MMCO_PLUGIN_DATA_DEST_DIR "${MMCO_MODULES_DEST_DIR}")
    set(PRIVATE_LIBRARY_DEST_DIR "${CMAKE_INSTALL_LIBDIR}/${Kite_AppBinaryName}")
    SET(Kite_BINARY_RPATH "$ORIGIN/../${PRIVATE_LIBRARY_DEST_DIR}")
    set(KDE_INSTALL_APPDIR      "${CMAKE_INSTALL_DATAROOTDIR}/applications")
    set(KDE_INSTALL_METAINFODIR "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")
    set(KDE_INSTALL_ICONDIR     "${CMAKE_INSTALL_DATAROOTDIR}/icons")
    set(KDE_INSTALL_MIMEDIR     "${CMAKE_INSTALL_DATAROOTDIR}/mime/packages")
    set(KDE_INSTALL_MANDIR      "${CMAKE_INSTALL_MANDIR}")

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Kite_AppID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Kite_AppID}.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/branding/${Kite_AppID}.svg DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/branding/${Kite_AppID}_256.png DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/256x256/apps" RENAME "${Kite_AppID}.png")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Kite_AppID}.xml DESTINATION ${KDE_INSTALL_MIMEDIR})

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/launcher/qtlogging.ini" DESTINATION "share/${Kite_Name}")

    set(PLUGIN_DEST_DIR "plugins")
    set(BUNDLE_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")

    if(Kite_ManPage)
        install(FILES ${Kite_ManPage} DESTINATION "${KDE_INSTALL_MANDIR}/man6")
    endif()

    configure_file(launcher/Kite.in "${CMAKE_CURRENT_BINARY_DIR}/KiteScript" @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/KiteScript" DESTINATION "." RENAME ${Kite_Name} COMPONENT portable EXCLUDE_FROM_ALL)

elseif(WIN32)
    set(BINARY_DEST_DIR ".")
    set(LIBRARY_DEST_DIR ".")
    set(PLUGIN_DEST_DIR ".")
    set(FRAMEWORK_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")
    # windeployqt's default destination for Qt's QML modules.
    set(QML_DEST_DIR "qml")
    # On Windows qt.conf's [Paths] are relative to qt.conf's own directory.
    set(QT_CONF_PLUGINS_DEST "${PLUGIN_DEST_DIR}")
    set(QT_CONF_QML_DEST "${QML_DEST_DIR}")
    set(JARS_DEST_DIR "jars")
    # Flat layout: .mmco files next to kite.exe under <install>/mmcmodules.
    set(MMCO_MODULES_DEST_DIR "${BINARY_DEST_DIR}/mmcmodules")
    # No strict codesign mode, so plugin data can sit beside the .mmco.
    set(MMCO_PLUGIN_DATA_DEST_DIR "${MMCO_MODULES_DEST_DIR}")
else()
    message(FATAL_ERROR "Platform not supported")
endif()

################################ Included Libs ################################

include(ExternalProject)
set_directory_properties(PROPERTIES EP_BASE External)

# libraries/ builds statically, except nbt++ (NBT_BUILD_SHARED).

############################### internal libraries ############################
option(NBT_BUILD_SHARED "Build NBT shared library" ON)
option(NBT_USE_ZLIB "Build NBT library with zlib support" OFF)
option(NBT_BUILD_TESTS "Build NBT library tests" OFF) #FIXME: fix unit tests.

add_subdirectory(${CMAKE_SOURCE_DIR}/libraries/libnbtplusplus libnbtplusplus)

add_subdirectory(libraries/launcher) # java based launcher part for Minecraft
add_subdirectory(libraries/javacheck) # java compatibility checker
add_subdirectory(libraries/xz-embedded) # xz compression
add_subdirectory(libraries/localpeer) # fork of a library from Qt solutions
add_subdirectory(libraries/classparser) # Java class file parser
add_subdirectory(libraries/katabasis) # OAuth2 library
add_subdirectory(libraries/monocypher) # Ed25519 check of the update manifest

# The shared libraries go where the launcher loads them: beside the binary on
# Windows, covered by the RPATH elsewhere. Installed by hand: RUNTIME_DEPENDENCY_SET
# missed them, and their own install() puts them in bin/, away from kite.exe.
set_target_properties(nbt++ PROPERTIES
    VERSION   "${Kite_RELEASE_VERSION_NAME}"
    SOVERSION "${Kite_VERSION_MAJOR}")

if(NOT DEFINED PRIVATE_LIBRARY_DEST_DIR)
    set(PRIVATE_LIBRARY_DEST_DIR "${LIBRARY_DEST_DIR}")
endif()

install(TARGETS nbt++
    RUNTIME DESTINATION "${BINARY_DEST_DIR}"
    LIBRARY DESTINATION "${PRIVATE_LIBRARY_DEST_DIR}")

# The launcher looks for the jars by exact name under JARS_DEST_DIR, not where
# install_jar() puts them.
get_target_property(JAVACHECK_JAR JavaCheck JAR_FILE)
get_target_property(NEWLAUNCH_JAR NewLaunch JAR_FILE)
install(FILES "${JAVACHECK_JAR}" DESTINATION "${JARS_DEST_DIR}" RENAME "JavaCheck.jar")
install(FILES "${NEWLAUNCH_JAR}" DESTINATION "${JARS_DEST_DIR}" RENAME "NewLaunch.jar")

############################### Built Artifacts ###############################

add_subdirectory(buildconfig)

add_subdirectory(launcher/crashreporter)

# Must be last: install() commands run in evaluation order.
add_subdirectory(launcher)

if(Kite_PLUGINS)
    # The SDK must come before the plugins.
    add_subdirectory(launcher/plugin/sdk)

    # After launcher: needs the Kite_logic target.
    add_subdirectory(launcher/plugin/plugins)
endif()
