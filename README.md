Kite
====

Kite is a Minecraft launcher that keeps every installation of the game in its
own instance, with a QML interface for modpacks, mods, worlds and accounts.

Kite is a fork of [MeshMC](https://github.com/Project-Tick/MeshMC) by Project
Tick, which continues [MultiMC](https://github.com/MultiMC/Launcher). It is an
independent project, not affiliated with or endorsed by Project Tick, MultiMC,
Mojang or Microsoft.

## Building

```sh
git submodule update --init
cmake --preset macos -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos
cmake --build build --config Debug
```

The Java launcher parts still target Java 7, which JDK 20 and newer refuse to
compile. Build with JDK 17 or older, for example by adding
`-DJava_JAVAC_EXECUTABLE=<jdk17>/bin/javac` to the configure line.

Microsoft sign-in uses Kite's own Azure app, whose client ID is built in. API
keys are not part of the source tree. Without them the matching feature stays
hidden: CurseForge and screenshot uploads (Imgur). Log uploads (paste.ee) need
a key too: paste.ee rejects its old anonymous `public` key, so without one the
Upload button and the crash reporter's upload fail. Either way the log is sent
only when the user clicks Upload (or agrees in the crash reporter). Keep your
own keys in a `CMakeUserPresets.json`, which git ignores:

```json
{
  "version": 8,
  "configurePresets": [
    {
      "name": "macos-local",
      "inherits": "macos",
      "cacheVariables": {
        "Kite_CURSEFORGE_API_KEY": "<CurseForge API key>",
        "Kite_PASTE_EE_API_KEY": "<paste.ee application key>"
      }
    }
  ]
}
```

## License

Kite is licensed under the Apache License, Version 2.0. `COPYING.md` lists the
licenses of the code and assets it includes, and `NOTICE` says what this fork
changed.
