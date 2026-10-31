# Staging plugin registry, entered only when Kite_STAGING_PLUGINS is ON (see ../CMakeLists.txt).
# Plugins still settling (API churn, partial coverage, unfinished UI) live here, out of a default
# build; once stable, move the subdirectory up into plugins/ and list it in the main registry.

# Cross-platform plugins still in staging.
add_subdirectory(ErrorOracle)
add_subdirectory(MercurialVersioning)
