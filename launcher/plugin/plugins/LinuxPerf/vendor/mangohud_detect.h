/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: Project Tick
 * SPDX-FileContributor: grxtor
 *
 * mangohud_detect.h: availability helpers for MangoHud.
 *
 * MangoHud (https://github.com/flightlessmango/MangoHud) has no C API header for launchers.
 * Integration is a wrapper command (`mangohud <executable> [args...]`, which hooks OpenGL / Vulkan
 * through LD_PRELOAD) plus environment variables set before the child starts:
 *   MANGOHUD=1               enable the overlay (Vulkan; implied by the wrapper)
 *   MANGOHUD_DLSYM=1         enable dlsym hooking (needed for some OpenGL games; default since 1.3)
 *   MANGOHUD_CONFIG=<csv>    comma-separated config key=value pairs
 *   MANGOHUD_CONFIGFILE=<p>  path to a config file (useful for Java apps, whose process is "java")
 *
 * For Minecraft / LWJGL, prepend "mangohud" as a wrapper command. The LinuxPerf plugin uses this
 * header to disable the MangoHud checkbox when the binary is not on PATH and to detect the Flatpak
 * variant.
 */

#pragma once

#ifdef __cplusplus
#include <cstdlib>

namespace mangohud_detect
{

	/* Whether `mangohud` is findable on PATH. Implemented in LinuxPerfPlugin.cpp via
	 * QStandardPaths::findExecutable; this placeholder always returns false, never call it
	 * directly. */
	inline bool is_available_on_path()
	{
		return false; /* placeholder — never call this directly */
	}

	/* Whether running inside a Flatpak sandbox (/.flatpak-info exists). The MangoHud layer is then
	 * org.freedesktop.Platform.VulkanLayer.MangoHud, pre-loaded by the Flatpak runtime; no wrapper
	 * is needed. */
	inline bool is_flatpak()
	{
#if defined(__linux__)
		struct stat st;
		return (stat("/.flatpak-info", &st) == 0);
#else
		return false;
#endif
	}

	/* Environment variables the LinuxPerf plugin injects when MangoHud is enabled via the wrapper
	 * path (single canonical source). */
	static constexpr const char* ENV_MANGOHUD = "MANGOHUD";
	static constexpr const char* ENV_MANGOHUD_DLSYM = "MANGOHUD_DLSYM";

	static constexpr const char* ENV_VALUE_ON = "1";
	static constexpr const char* ENV_VALUE_OFF = "0";

} /* namespace mangohud_detect */

#endif /* __cplusplus */
