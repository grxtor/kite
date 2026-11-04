cmake_minimum_required(VERSION 3.25)
project(monocypher VERSION 4.0.3 LANGUAGES C)

# Monocypher 4.0.3 with its optional Ed25519 (SHA-512) module, unmodified. kite-updater uses only
# crypto_ed25519_check(), to verify the signature on a release's kite-update.json.
add_library(monocypher STATIC
    src/monocypher.c
    src/monocypher-ed25519.c
    include/monocypher.h
    include/monocypher-ed25519.h
)
target_include_directories(monocypher PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_property(TARGET monocypher PROPERTY C_STANDARD 99)
set_property(TARGET monocypher PROPERTY POSITION_INDEPENDENT_CODE ON)
