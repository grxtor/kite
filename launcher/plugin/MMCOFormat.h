/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
 * MMCO module format.
 *
 * .mmco files are shared libraries (.so/.dylib/.dll) exporting a standard ABI via C linkage. Every
 * module must export `mmco_module_info` (MMCOModuleInfo); the loader validates the magic and ABI
 * version before calling anything else. Build one by compiling against the Kite Plugin SDK and
 * linking a shared library with the .mmco extension.
 *
 * Signing: a module may carry a detached GPG signature appended as a fixed-layout trailer:
 *
 *   ┌──────────────────────────────────────────────────────────┐
 *   │  <original ELF/PE/Mach-O bytes>                          │
 *   ├──────────────────────────────────────────────────────────┤
 *   │  <ASCII-armored detached GPG signature, N bytes>         │
 *   │  uint64_t signature_size  (little-endian, = N)           │
 *   │  uint32_t trailer_magic   (= MMCO_TRAILER_MAGIC)         │
 *   └──────────────────────────────────────────────────────────┘
 *
 * The signed payload is exactly the file bytes before the trailer. A signature is optional when the
 * SPDX license declared in mmco_module_info is OSI-approved and mandatory otherwise. The launcher
 * trusts keys in its keyring (setting "plugin.signing.keyring_path", default
 * <config_dir>/keyring.gpg).
 */

#pragma once

#include <cstdint>

#define MMCO_MAGIC 0x4D4D434F
#define MMCO_VERSION "10.0.0"
#define MMCO_ABI_VERSION 5
#define MMCO_EXTENSION ".mmco"

/* Identifies the signature trailer at the end of a .mmco file. Little-endian on disk; ASCII "MMCS". */
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

/* Optional dependency on another .mmco module, declared in MMCOModuleInfo::dependencies. */
struct MMCODependency {
	const char* name;
	/* Minimum version (semver-ish); nullptr or empty means any version. */
	const char* min_version;
	/* Non-zero: optional, the dependent module still loads if this one is missing. */
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

	/* Qt resource prefix where the plugin's icon set is mounted, or nullptr if it ships no icons.
	 * Plugins compile a .qrc into the .mmco; the logical name (e.g. "myplugin") resolves to
	 * ":/plugins/<name>/<icon>.png" through MMCOContext::ui_plugin_icon(). */
	const char* icon_set_resource;

	/* Dependency table: nullptr if dependency_count is 0, else dependency_count entries with static
	 * storage duration. */
	const MMCODependency* dependencies;
	uint32_t dependency_count;

	/* OpenPGP key id the module was signed with, nullptr or empty if unsigned. Informational: the
	 * launcher uses it to find the public key before verifying the trailer. */
	const char* signing_key_id;
};

#define MMCO_FLAG_NONE 0x00000000

/* Symbol visibility for .mmco shared libraries */
#if defined(_WIN32) || defined(__CYGWIN__)
#define MMCO_EXPORT __declspec(dllexport)
#else
#define MMCO_EXPORT __attribute__((visibility("default")))
#endif

/* Every .mmco module must export these with C linkage (mmco_init() returns 0 on success):
 *   extern "C" MMCOModuleInfo mmco_module_info;
 *   extern "C" int  mmco_init(struct MMCOContext* ctx);
 *   extern "C" void mmco_unload(void); */
