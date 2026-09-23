/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
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
 *
 *
 * MMCO Module Format
 *
 * .mmco files are shared libraries (.so/.dylib/.dll) that export a standard
 * ABI via C linkage. They are analogous to Linux kernel modules (.ko) but
 * operate at the launcher layer.
 *
 * Every .mmco module MUST export a symbol named `mmco_module_info` of type
 * MMCOModuleInfo. The loader validates the magic and ABI version before
 * calling any further entry points.
 *
 * The compiler toolchain produces .mmco by compiling C++ sources against
 * the MeshMC Plugin SDK and linking as a shared library with the .mmco
 * extension.
 *
 *
 * ── Signing ─────────────────────────────────────────────────────────
 *
 * Modules may carry a detached GPG signature appended to the end of the
 * .mmco file as a fixed-layout trailer:
 *
 *   ┌──────────────────────────────────────────────────────────┐
 *   │  <original ELF/PE/Mach-O bytes>                          │
 *   ├──────────────────────────────────────────────────────────┤
 *   │  <ASCII-armored detached GPG signature, N bytes>         │
 *   │  uint64_t signature_size  (little-endian, = N)           │
 *   │  uint32_t trailer_magic   (= MMCO_TRAILER_MAGIC)         │
 *   └──────────────────────────────────────────────────────────┘
 *
 * The signed payload is exactly the file bytes BEFORE the trailer —
 * i.e. the original shared-library content as produced by the linker.
 *
 * Whether a signature is required depends on the SPDX license declared
 * in mmco_module_info:
 *   - OSI-approved open-source licenses  → signature optional
 *   - any other license / no license     → signature mandatory
 *
 * The launcher trusts signatures made by keys present in its keyring
 * (configurable via the global app setting "plugin.signing.keyring_path";
 * defaults to <config_dir>/keyring.gpg).
 */

#pragma once

#include <cstdint>

#define MMCO_MAGIC 0x4D4D434F
#define MMCO_VERSION "10.0.0"
#define MMCO_ABI_VERSION 5
#define MMCO_EXTENSION ".mmco"

/* Magic value that identifies the GPG signature trailer at the end of a
 * .mmco file. Little-endian on disk. ASCII: "MMCS" (MeshMC Module Signature).
 */
#define MMCO_TRAILER_MAGIC 0x53434D4D
#define MMCO_FLAG_NONE 0x00000000
#define MMCO_VERNUM                                                            \
	0x0A000000L /* MMNNRRSM: major minor revision status modified */
#define MMCO_VER_MAJOR 10
#define MMCO_VER_MINOR 0
#define MMCO_VER_REVISION 0
#define MMCO_VER_STATUS 0 /* 0=devel, 1-E=beta, F=Release (DEPRECATED) */
#define MMCO_VER_STATUSH                                                       \
	0x0 /* Hex values: 0=devel, 1-9=beta, A-E=Release Candidate, F=Release */
#define MMCO_VER_MODIFIED 0 /* non-zero if modified externally from mmco */

/* Optional dependency on another .mmco module, declared in
 * MMCOModuleInfo::dependencies. */
struct MMCODependency {
	const char* name;
	/* Minimum acceptable version string (semver-ish). May be nullptr or
	 * empty to mean "any version". */
	const char* min_version;
	/* If non-zero, the dependency is optional: the dependent module will
	 * still load even if this dependency is missing. */
	uint32_t optional;
};

struct MMCOModuleInfo {
	uint32_t magic;			 /* Must be MMCO_MAGIC */
	uint32_t abi_version;	 /* Must match MMCO_ABI_VERSION */
	const char* name;		 /* Human-readable module name */
	const char* version;	 /* Module version string */
	const char* author;		 /* Author / maintainer */
	const char* description; /* Short description */
	const char* license;	 /* SPDX license identifier */
	uint32_t flags;			 /* Reserved for future use, set to 0 */
	const char* code_link;	 /* Optional: URL to source code repository */

	/* Qt resource prefix where the plugin's icon set is mounted, or
	 * nullptr if the plugin ships no icons.
	 *
	 * Plugins bundle icons by compiling a .qrc into the .mmco shared
	 * library. The launcher exposes them via MMCOContext::ui_plugin_icon().
	 * The string is treated as a logical name (e.g. "myplugin") and the
	 * launcher resolves it to ":/plugins/<name>/<icon>.png" at runtime. */
	const char* icon_set_resource;

	/* Dependency table.
	 *
	 * `dependencies` may be nullptr if `dependency_count` is 0; otherwise
	 * it points to `dependency_count` MMCODependency entries with static
	 * storage duration (i.e. the module owns them for its full lifetime). */
	const MMCODependency* dependencies;
	uint32_t dependency_count;

	/* Identifier of the OpenPGP key the module was signed with, or
	 * nullptr / empty for unsigned modules.
	 *
	 * Informational only — the actual signature bytes live in the
	 * file-trailer described at the top of this header. The launcher
	 * uses this hint to look up the corresponding public key in the
	 * trusted keyring before verifying the trailer. */
	const char* signing_key_id;
};

/* Module flags (reserved, extend as needed) */
#define MMCO_FLAG_NONE 0x00000000

/* Symbol visibility for .mmco shared libraries */
#if defined(_WIN32) || defined(__CYGWIN__)
#define MMCO_EXPORT __declspec(dllexport)
#else
#define MMCO_EXPORT __attribute__((visibility("default")))
#endif

/*
 * Every .mmco module must export these three symbols with C linkage:
 *
 *   extern "C" MMCOModuleInfo mmco_module_info;
 *   extern "C" int  mmco_init(struct MMCOContext* ctx);
 *   extern "C" void mmco_unload(void);
 *
 * mmco_init() returns 0 on success, non-zero on failure.
 */
