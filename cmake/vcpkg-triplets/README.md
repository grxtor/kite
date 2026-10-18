# vcpkg overlay triplets

These files override (or, in one case, add to) the triplets that ship with the
`cmake/vcpkg` submodule. `vcpkg-configuration.json` points at this directory
through `overlay-triplets`, so a plain triplet name like `x64-linux` resolves
to the file here instead of `cmake/vcpkg/triplets/x64-linux.cmake`.

Three things are asserted for every platform:

- **`VCPKG_LIBRARY_LINKAGE static`** — dependencies are linked into the
  launcher binary, so a Kite build has no dependency DLLs/dylibs to ship or
  to find at runtime.
- **`VCPKG_CRT_LINKAGE dynamic`** — the C runtime itself stays shared. On MSVC
  this has to match the top-level `CMAKE_MSVC_RUNTIME_LIBRARY`
  (`MultiThreadedDLL`, pinned there for the Rust subsystem); mixing CRTs is a
  link error at best.
- **`VCPKG_BUILD_TYPE release`** — only the release half of each dependency is
  built. Nothing links debug builds of cmark or libarchive, so the debug half
  is pure build time. Note this applies to the *dependencies*, not to Kite:
  a Debug build of the launcher still links release dependencies.

## Why override the default triplet names instead of using `-release` ones?

Upstream already ships `x64-linux-release`, `x64-windows-release` and friends,
which would give the same result. But the triplet name has to be selected from
outside CMake (`VCPKG_TARGET_TRIPLET`, or `--triplet`), and there is no CMake
preset macro for the host architecture — so every caller would have to spell
out `x64-…` / `arm64-…` by hand and keep the two spellings in sync. Overriding
the plain names keeps `x64-linux` meaning "the way Kite builds x64 Linux",
whoever asks for it.

`arm64-mingw-static-release.cmake` is the exception: upstream has
`community/x64-mingw-static-release.cmake` and
`community/x86-mingw-static-release.cmake`, but no arm64 equivalent, so that
one is added rather than overridden.
