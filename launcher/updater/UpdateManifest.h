/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
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

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

/*! kite-update.json: every file of one release with its size and SHA-256, signed as a whole by the
 * release workflow (kite-update.json.sig holds a base64 Ed25519 signature over the file's exact
 * bytes). kite-updater installs a file only when a manifest whose signature verifies against the key
 * built into it (BuildConfig.UPDATE_PUBLIC_KEY) lists that file, and the download hashes to the listed
 * SHA-256. The tag inside stops one release's manifest from vouching for another release's files.
 *
 * Written by packaging/update-feed/kite_update_feed.py; keep the two in step. */
namespace UpdateManifest
{
	constexpr auto kManifestName = "kite-update.json";
	constexpr auto kSignatureName = "kite-update.json.sig";

	//! Far above any real manifest (a few KiB); a download larger than this is not one.
	constexpr qint64 kMaxBytes = 1 << 20;

	struct Asset {
		QString name;
		qint64 size = 0;
		QString sha256; //!< Lowercase hex, 64 characters.
	};

	struct Manifest {
		QString version;
		QString tag;
		QList<Asset> assets;

		//! The entry for the release file called \a name, if the manifest lists one.
		std::optional<Asset> asset(const QString& name) const;
	};

	/*! Whether \a signature (the .sig file's contents, base64) is a valid Ed25519 signature by
	 * \a publicKey (base64 of 32 bytes) over \a manifest. */
	bool verifySignature(const QByteArray& manifest, const QByteArray& signature,
						 const QString& publicKey, QString* error);

	/*! Verifies, then parses: nothing in \a manifest is looked at before its signature checks out.
	 * Fails, with \a error saying why, when the key or signature is malformed or does not verify,
	 * the JSON is not a manifest of a known schema, an entry is malformed or listed twice, or the
	 * manifest belongs to a release other than \a expectedTag. */
	std::optional<Manifest> load(const QByteArray& manifest,
								 const QByteArray& signature,
								 const QString& publicKey,
								 const QString& expectedTag, QString* error);

} // namespace UpdateManifest
