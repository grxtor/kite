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
 */

#pragma once

#include "plugin/PluginMetadata.h"

#include <QSet>
#include <QStringList>
#include <QVector>

#include <cstdint>

/* Scans the known directories for .mmco files and does the low-level dlopen / symbol resolution;
 * mmco_init() is PluginManager's job. Also runs the trust pre-flight: extracts and verifies any GPG
 * trailer and applies the policy "OSS license = signature optional, otherwise mandatory". A module
 * failing the policy, or named in the disabled set passed to discoverModules(), is returned with
 * disabled=true so the manager skips it without unloading the library (Settings may still
 * need it). */

class PluginLoader
{
  public:
	PluginLoader();
	~PluginLoader();

	/* Scans all search paths and returns metadata for every valid .mmco. `disabledNames` is a
	 * case-insensitive set of module names to mark PluginDisableReason::UserDisabled: the file is
	 * still opened so the dialog can show its metadata, but the caller must not call mmco_init() on
	 * it. */
	QVector<PluginMetadata>
	discoverModules(const QSet<QString>& disabledNames = {}) const;

	/* Opens one .mmco: dlopen, validate magic/ABI, resolve entry points. Success gives loaded ==
	 * true; failure gives loaded == false and a null libraryHandle. */
	PluginMetadata loadModule(const QString& path) const;

	static void unloadModule(PluginMetadata& meta);

	/* Builds the PluginDisableReason and translated detail for a module whose abi_version is
	 * outside [abiMin, abiMax]. Public and separate from loadModule() so the wording can be
	 * unit-tested without dlopen(). `moduleName` may be empty (null or blank
	 * mmco_module_info::name); `fallbackLabel` (typically the file path) identifies the module
	 * then. */
	static PluginDisableReason
	classifyAbiMismatch(const QString& moduleName, const QString& fallbackLabel,
						uint32_t builtForAbi, uint32_t abiMin, uint32_t abiMax,
						QString& outDetail);

	QStringList searchPaths() const;

	/* Prepends an extra search path (e.g. from settings). */
	void addSearchPath(const QString& path);

  private:
	QStringList m_extraPaths;

	static QStringList defaultSearchPaths();
	QVector<PluginMetadata>
	scanDirectory(const QString& dir, const QSet<QString>& disabledNames) const;

	/* Runs the trust pre-flight on `meta`: verifies the GPG trailer (if any) and applies the
	 * license-based signature policy, updating signature_state / disabled. Called from loadModule()
	 * before it returns. */
	static void verifySignatureAndPolicy(PluginMetadata& meta);
};
