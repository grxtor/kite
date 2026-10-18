# Kite In-Tree Plugin Registry (STAGING)
#
# Each subdirectory here builds a .mmco shared module.
# Built modules are placed in ${KITE_PLUGIN_STAGING_DIR} so the
# launcher can discover them at runtime from the app-relative
# mmcmodules/ directory.
#
# This file is only entered when `Kite_STAGING_PLUGINS` is ON
# (see ../CMakeLists.txt). The intent of staging is to surface
# plugins that are still settling — API churn, partial coverage,
# unfinished UI — without exposing them to a default `cmake --build`
# of the launcher. Once a plugin stabilises, move its subdirectory
# back up one level into plugins/ and re-list it in the main
# registry.

#################################### Register plugins here ####################################

# Cross-platform plugins still in staging.
add_subdirectory(ErrorOracle)
add_subdirectory(MercurialVersioning)
