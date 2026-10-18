/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 * 
 * Copyright 2012-2021 MultiMC Contributors
 * Copyright (C) 2026 Project Tick
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.github.grxtor.modern;

import io.github.grxtor.Kite;
import io.github.grxtor.ParamBucket;
import io.github.grxtor.Utils;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Modern launcher implementation for Minecraft 1.0 and above.
 *
 * Unlike OneSixLauncher which loads the game in-process via reflection,
 * ModernLauncher spawns Minecraft as a separate child process using the
 * Java binary specified by Kite. This decouples the JVM version
 * running Kite library from the JVM version required by the game,
 * allowing Minecraft versions that require Java 21+ or newer (e.g., class
 * file version 69.0 / Java 25) to be launched correctly even when the
 * launcher itself runs on an older JVM.
 *
 * Parameters consumed from ParamBucket:
 *   javaPath     - Path to the Java binary for the game process (required)
 *   cp           - Classpath entries, one per entry (required)
 *   mainClass    - Main class to invoke (default: net.minecraft.client.main.Main)
 *   param        - Game arguments, one per entry
 *   jvmArg       - JVM arguments to forward to the game process, one per arg
 *   natives      - Path to the native libraries directory
 *   serverAddress - Server address for direct-connect on launch (optional)
 *   serverPort    - Server port for direct-connect on launch (optional)
 *   worldName     - Singleplayer world save folder to open on launch (optional)
 *   traits        - Version traits, used to pick quick play vs. legacy args
 */
public class ModernLauncher implements Kite
{
    private static boolean isProcessAlive(Process process)
    {
        try
        {
            process.exitValue();
            return false;
        }
        catch (IllegalThreadStateException ignored)
        {
            return true;
        }
    }

    @Override
    public int launch(ParamBucket params)
    {
        try
        {
            return doLaunch(params);
        }
        catch (Exception e)
        {
            Utils.log("ModernLauncher encountered a fatal error: " + e.getMessage(), "Error");
            e.printStackTrace();
            return 1;
        }
    }

    private int doLaunch(ParamBucket params) throws Exception
    {
        // --- Java binary ---
        String javaPath = params.firstSafe("javaPath", "java");
        Utils.log("Java binary: " + javaPath);

        // --- Main class ---
        String mainClass = params.firstSafe("mainClass", "net.minecraft.client.main.Main");
        Utils.log("Main class: " + mainClass);

        // --- Classpath ---
        // "cp"  = regular game libraries
        // "ext" = native-classifier JARs (e.g. lwjgl-3.x-natives-linux.jar) that
        //         LWJGL 3's SharedLibraryLoader needs to find on the classpath.
        //         OneSixLauncher works without these because it runs in-process and
        //         Kite JVM already has them on its own classpath.  For
        //         ModernLauncher we must include them explicitly.
        List<String> cpEntries  = params.allSafe("cp",  Collections.<String>emptyList());
        List<String> extEntries = params.allSafe("ext", Collections.<String>emptyList());
        List<String> allCpEntries = new ArrayList<String>(cpEntries);
        allCpEntries.addAll(extEntries);
        if (allCpEntries.isEmpty())
        {
            Utils.log("No classpath entries provided to ModernLauncher.", "Error");
            return 1;
        }
        String classpath = buildClassPath(allCpEntries);

        // --- Native library path ---
        String natives = params.firstSafe("natives", "");

        // --- JVM arguments (Xmx, Xms, GC flags, platform-specific flags, etc.) ---
        List<String> jvmArgs = params.allSafe("jvmArg", Collections.<String>emptyList());

        // --- Game arguments ---
        // param entries are already pre-expanded by the C++ launcher (auth tokens,
        // game directory, asset index, resolution, etc.)
        List<String> gameArgs = new ArrayList<String>(
            params.allSafe("param", Collections.<String>emptyList())
        );

        // Launch destination: a server or a world, never both. Passed as
        // separate keys and appended here rather than by the C++ side, because
        // processMinecraftArgs skips them when a launch script is used.
        //
        // Which argument the game understands depends on its version:
        // Minecraft 1.20 (23w14a) removed --server/--port and replaced them
        // with quick play. The version's traits say which it is, so ask them
        // instead of guessing -- passing the wrong pair makes the game either
        // ignore the destination or refuse to start.
        List<String> traits = params.allSafe("traits", Collections.<String>emptyList());
        boolean quickPlayMultiplayer = traits.contains("feature:is_quick_play_multiplayer");
        boolean quickPlaySingleplayer = traits.contains("feature:is_quick_play_singleplayer");

        String serverAddress = params.firstSafe("serverAddress", "");
        String serverPort    = params.firstSafe("serverPort", "");
        String worldName     = params.firstSafe("worldName", "");

        if (serverAddress != null && !serverAddress.isEmpty())
        {
            if (quickPlayMultiplayer)
            {
                gameArgs.add("--quickPlayMultiplayer");
                // quickPlayMultiplayer takes one "host:port" argument, so an
                // absent port has to become the default rather than nothing.
                String port = (serverPort == null || serverPort.isEmpty()) ? "25565" : serverPort;
                gameArgs.add(serverAddress + ":" + port);
            }
            else
            {
                gameArgs.add("--server");
                gameArgs.add(serverAddress);
                if (serverPort != null && !serverPort.isEmpty())
                {
                    gameArgs.add("--port");
                    gameArgs.add(serverPort);
                }
            }
        }
        else if (worldName != null && !worldName.isEmpty() && quickPlaySingleplayer)
        {
            // No pre-quick-play way to open a world exists, so unlike the
            // server case there is nothing to fall back to: a version without
            // the trait just launches to the main menu.
            gameArgs.add("--quickPlaySingleplayer");
            gameArgs.add(worldName);
        }

        // --- Build the full command ---
        List<String> command = buildCommand(javaPath, jvmArgs, natives, classpath, mainClass, gameArgs);

        // Log the full command for diagnostics (visible in launcher console)
        StringBuilder cmdLog = new StringBuilder("Spawning game process:\n  CMD: ");
        for (String s : command)
        {
            cmdLog.append(s).append(" ");
        }
        Utils.log(cmdLog.toString().trim());

        ProcessBuilder pb = new ProcessBuilder(command);
        // Let the game process inherit the working directory from us (set by LauncherPartLaunch)
        pb.directory(null);

        // Set JAVA_HOME so the game and any native launchers can locate Java correctly
        File javaFile = new File(javaPath);
        File javaParent = javaFile.getParentFile();
        if (javaParent != null && javaParent.getParentFile() != null)
        {
            pb.environment().put("JAVA_HOME", javaParent.getParent());
        }

        final Process process = pb.start();

        // Pipe stdout and stderr from the child process to our own streams so
        // the C++ launcher's LoggedProcess can capture and display them.
        Thread outPipe = pipeStream(process.getInputStream(), System.out, "stdout");
        Thread errPipe = pipeStream(process.getErrorStream(), System.err, "stderr");
        outPipe.start();
        errPipe.start();

        // Shutdown hook: ensure the game process is terminated if Kite is
        // forcefully killed (e.g., SIGKILL from within Kite UI).
        Runtime.getRuntime().addShutdownHook(new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                if (isProcessAlive(process))
                {
                    process.destroy();
                }
            }
        }, "ModernLauncher-ShutdownHook"));

        // Block until the game process exits
        int exitCode = process.waitFor();

        outPipe.join();
        errPipe.join();

        if (exitCode != 0)
        {
            Utils.log("Game process exited with code " + exitCode, "Error");
        }

        return exitCode;
    }

    private String buildClassPath(List<String> entries)
    {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < entries.size(); i++)
        {
            if (i > 0)
                sb.append(File.pathSeparator);
            sb.append(entries.get(i));
        }
        return sb.toString();
    }

    private List<String> buildCommand(
        String javaPath,
        List<String> jvmArgs,
        String natives,
        String classpath,
        String mainClass,
        List<String> gameArgs)
    {
        List<String> cmd = new ArrayList<String>();

        // Java executable
        cmd.add(javaPath);

        // JVM arguments (memory settings, GC options, platform flags, etc.)
        cmd.addAll(jvmArgs);

        // Native library path - needed for LWJGL and other native dependencies.
        // Pass both the standard JVM property and the LWJGL-specific property:
        //   java.library.path   - used by java.lang.System.loadLibrary()
        //   org.lwjgl.librarypath - checked first by LWJGL 3.3+ before java.library.path
        if (natives != null && !natives.isEmpty())
        {
            cmd.add("-Djava.library.path=" + natives);
            cmd.add("-Dorg.lwjgl.librarypath=" + natives);
        }

        // Classpath
        if (!classpath.isEmpty())
        {
            cmd.add("-cp");
            cmd.add(classpath);
        }

        // Main class
        cmd.add(mainClass);

        // Game arguments
        cmd.addAll(gameArgs);

        return cmd;
    }

    /**
     * Reads bytes from {@code src} and writes them to {@code dst} on a dedicated
     * daemon thread so neither stream blocks the main thread.
     */
    private Thread pipeStream(final InputStream src, final PrintStream dst, final String name)
    {
        Thread t = new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                byte[] buffer = new byte[8192];
                int read;
                try
                {
                    while ((read = src.read(buffer)) != -1)
                    {
                        dst.write(buffer, 0, read);
                        dst.flush();
                    }
                }
                catch (IOException e)
                {
                    // Stream closed - expected when the child process exits
                }
            }
        }, "ModernLauncher-" + name + "-pipe");
        t.setDaemon(true);
        return t;
    }
}
