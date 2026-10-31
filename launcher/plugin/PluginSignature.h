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

#include <QByteArray>
#include <QString>

/* Utilities for the GPG signature trailer appended to .mmco files (layout in MMCOFormat.h) and for
 * deciding whether an SPDX license exempts a module from signing. */

namespace PluginSignature
{

	/* Raw bytes pulled out of a .mmco file: the signed payload (everything before the trailer) and
	 * the detached signature. Both are empty if there is no trailer. */
	struct ExtractedTrailer {
		QByteArray payload;		/* Bytes that were signed. */
		QByteArray signature;	/* ASCII-armored detached signature. */
		bool present = false;	/* True if a trailer was found. */
		bool malformed = false; /* True if trailer magic matched but layout
								 * could not be parsed. */
	};

	/* Reads the trailer from a .mmco file; `present` says whether a signature was appended. I/O
	 * errors give an empty struct; a present but malformed trailer sets malformed=true with empty
	 * arrays so callers can flag the module untrusted. */
	ExtractedTrailer extractTrailer(const QString& filePath);

	/* Verifies the detached signature against the payload with GpgME, using the keyring from
	 * setKeyringPath() (the default GnuPG home otherwise). `detail` receives a human-readable
	 * status or error, `fingerprint` the signing key's (empty on failure). Returns Valid /
	 * Untrusted / BadSignature / Error. */
	PluginSignatureState verify(const QByteArray& payload,
								const QByteArray& signature, QString& detail,
								QString& fingerprint);

	/* Reads the trailer from the file, then verifies it; returns Absent without touching GpgME if
	 * there is none. Results are memoised in a persistent disk cache keyed on (path, size, mtime);
	 * `bypassCache=true` forces a fresh verification (the plugins dialog's "Re-verify"). */
	PluginSignatureState verifyFile(const QString& filePath, QString& detail,
									QString& fingerprint,
									bool bypassCache = false);

	/* Overrides the GnuPG home directory (where the trusted keyring lives); empty resets to the
	 * GpgME default. Call once at startup with the value from the application settings. */
	void setKeyringPath(const QString& path);

	/* Sets where the on-disk verification cache lives and loads any existing cache (no-op if
	 * missing or unreadable); an empty path disables it. Call once at startup, before the first
	 * plugin loads. Thread-safe. */
	void setCachePath(const QString& path);

	/* Flushes the in-memory cache to its path; cheap. Called at the end of
	 * PluginLoader::discoverModules(). */
	void flushCache();

	/* Whether an SPDX license expression refers to an OSI-approved (or FSF-recognised free)
	 * license. Liberal: a compound expression ("Apache-2.0 OR MIT", "GPL-3.0-or-later WITH
	 * Classpath-exception-2.0") counts as OSS if any clause is. An empty license is non-OSS and
	 * requires a signature. */
	bool isOpenSourceLicense(const QString& spdxLicense);

	/* Short human-readable label for the plugins dialog's state column. */
	const char* stateLabel(PluginSignatureState state);

} // namespace PluginSignature
