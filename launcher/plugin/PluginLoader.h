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
 */

#pragma once

#include "plugin/PluginMetadata.h"

#include <QSet>
#include <QStringList>
#include <QVector>

#include <cstdint>

/*
 * PluginLoader — scans known directories for .mmco module files and
 * performs the low-level dlopen / symbol resolution.
 *
 * It does NOT call mmco_init(); that is the PluginManager's job.
 *
 * In addition to loading, the loader runs the *trust pre-flight*: it
 * extracts and verifies any GPG signature trailer, then applies the
 * "OSS license → signature optional / otherwise mandatory" policy. A
 * module that fails the policy is returned with `disabled=true` set so
 * that the manager skips it without unloading the shared library (the
 * library may still be needed by other tooling, e.g. the about dialog).
 *
 * Modules whose name appears in the disabled-set passed to discoverModules()
 * are likewise returned with disabled=true.
 */

class PluginLoader
{
  public:
	PluginLoader();
	~PluginLoader();

	/*
	 * Scan all configured search paths and return metadata for every
	 * valid .mmco module found.
	 *
	 * `disabledNames` is a case-insensitive set of module names that
	 * should be marked PluginDisableReason::UserDisabled. The .mmco file
	 * is still opened (so the metadata block can be displayed in the
	 * plugins dialog), but the module's `disabled` flag is set and the
	 * caller must not call mmco_init() on it.
	 */
	QVector<PluginMetadata>
	discoverModules(const QSet<QString>& disabledNames = {}) const;

	/*
	 * Open a single .mmco file: dlopen, validate magic/ABI, resolve
	 * entry points. On success the returned PluginMetadata has
	 * loaded == true. On failure loaded == false and libraryHandle
	 * is nullptr.
	 */
	PluginMetadata loadModule(const QString& path) const;

	/*
	 * Close a previously loaded module.
	 */
	static void unloadModule(PluginMetadata& meta);

	/*
	 * Build the PluginDisableReason and human-readable, translated detail
	 * for a module whose declared abi_version falls outside
	 * [abiMin, abiMax]. Only called by loadModule() once it has already
	 * decided the range check failed, but kept as its own static
	 * function -- and public -- so the reason/message formatting can be
	 * unit-tested without dlopen()'ing a real .mmco file: the wording
	 * depends only on these plain values, never on anything read from
	 * the module beyond the name loadModule() already captured.
	 *
	 * `moduleName` is the module's declared name, or empty if
	 * mmco_module_info::name was null/blank; `fallbackLabel` (typically
	 * the .mmco file path) is used in the message in that case so the
	 * user still gets something to identify the module by.
	 */
	static PluginDisableReason
	classifyAbiMismatch(const QString& moduleName, const QString& fallbackLabel,
						uint32_t builtForAbi, uint32_t abiMin, uint32_t abiMax,
						QString& outDetail);

	/*
	 * Return the ordered list of directories that will be scanned.
	 */
	QStringList searchPaths() const;

	/*
	 * Prepend extra search paths (e.g. from settings).
	 */
	void addSearchPath(const QString& path);

  private:
	QStringList m_extraPaths;

	static QStringList defaultSearchPaths();
	QVector<PluginMetadata>
	scanDirectory(const QString& dir, const QSet<QString>& disabledNames) const;

	/* Run the trust pre-flight on `meta`: verify the GPG trailer (if any)
	 * and apply the license-based signature policy. Updates the
	 * signature_state / disabled fields on `meta`. Called from
	 * loadModule() before that function returns. */
	static void verifySignatureAndPolicy(PluginMetadata& meta);
};
