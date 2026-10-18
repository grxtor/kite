# See https://github.com/microsoft/vcpkg/discussions/19454
# NOTE: Try to keep in sync with default arm64-osx definition
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES "arm64;x86_64")

execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE _kite_host_machine
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(_kite_host_machine MATCHES "^(arm64|aarch64)$")
    set(_kite_darwin_triplet "aarch64-apple-darwin")
else()
    set(_kite_darwin_triplet "x86_64-apple-darwin")
endif()
set(VCPKG_MAKE_BUILD_TRIPLET
    "--host=${_kite_darwin_triplet};--build=${_kite_darwin_triplet}")
unset(_kite_host_machine)
unset(_kite_darwin_triplet)
