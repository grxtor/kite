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

API keys are not part of the source tree. Without them the matching feature
stays hidden: Microsoft sign-in, CurseForge, log uploads (paste.ee) and
screenshot uploads (Imgur). Keep your own keys in a `CMakeUserPresets.json`,
which git ignores:

```json
{
  "version": 8,
  "configurePresets": [
    {
      "name": "macos-local",
      "inherits": "macos",
      "cacheVariables": {
        "Kite_MICROSOFT_CLIENT_ID": "<Azure application ID>",
        "Kite_CURSEFORGE_API_KEY": "<CurseForge API key>"
      }
    }
  ]
}
```

## License

Kite is licensed under the Apache License, Version 2.0. `COPYING.md` lists the
licenses of the code and assets it includes, and `NOTICE` says what this fork
changed.
