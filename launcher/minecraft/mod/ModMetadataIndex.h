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

#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

// Persistent sidecar index recording how each managed file in a mods-like
// folder was installed (platform, project, version, SHA-1, dependency flag,
// install time).
//
// It lives in `<folder>/.index/` as packwiz `*.pw.toml` files (see
// PackwizSidecar.h), so the folder stays usable by packwiz and other
// launchers. Older `<filename>.json` sidecars are still read and replaced by
// TOML on the next write. Stale `*.json` entries (target file gone) are pruned
// on load(); orphaned `*.pw.toml` files are kept, since in a packwiz pack the
// sidecar is the definition and the file may not be downloaded yet.
//
// Thread-safe: an internal mutex guards all mutations, so the model thread and
// install / dependency-resolver tasks can use it concurrently.
class ModMetadataIndex
{
  public:
	struct Entry {
		QString fileName;  /* Base file name as stored on disk, no .disabled */
		QString platform;  /* "modrinth" | "curseforge" | "local" | ""       */
		QString projectId; /* Platform-specific project ID                   */
		QString versionId; /* Platform-specific version / file ID            */

		QString name; /* Human-readable mod name at install time         */
		QString slug; /* Platform slug, if known                         */
		QString side; /* "client" | "server" | "both" | "" if unknown    */
		QString downloadUrl;

		// The version as a person reads it ("1.1.1+1.17"), apart from
		// versionId, the platform's opaque handle. A file installed from an
		// mrpack has only this (the download URL carries no id); never send it
		// where an id is expected.
		QString versionNumber;

		// sha1 is the SHA-1 we recorded; hashFormat/hash are the sidecar's
		// digest verbatim. Foreign sidecars may carry SHA-512 (Modrinth is
		// asked for it first), so sha1 stays empty for them rather than hold a
		// digest of another kind.
		QString sha1;
		QString hashFormat;
		QString hash;

		qint64 fileSize = 0;

		bool isDependency = false; /* installed as a transitive dep?         */

		QDateTime installedAt;

		bool isValid() const
		{
			return !fileName.isEmpty();
		}
		bool hasPlatformOrigin() const
		{
			return !platform.isEmpty() && !projectId.isEmpty();
		}
	};

	explicit ModMetadataIndex(const QDir& folder);

	// The absolute path of the sidecar directory (`<folder>/.index`).
	QString indexDir() const
	{
		return m_indexDir.absolutePath();
	}

	// Re-reads every sidecar from disk; entries with no file in `<folder>` are
	// dropped. Safe to call repeatedly.
	void load();

	// Accessors return by value to keep callers off the mutex. Keys are file
	// names without any `.disabled` suffix.
	Entry get(const QString& fileName) const;
	bool contains(const QString& fileName) const;
	QList<Entry> all() const;
	// Project ids of the entries installed from @p platform, one each.
	QStringList projectIds(const QString& platform) const;

	Entry findByPlatformProject(const QString& platform,
								const QString& projectId) const;
	Entry findByNormalizedName(const QString& normalizedName) const;
	Entry findBySha1(const QString& sha1) const;

	// Writes (or overwrites) the sidecar for `entry.fileName` and the cache.
	// Returns true on success.
	bool put(const Entry& entry);

	// Removes the sidecar of `fileName`. Returns true if one existed and was
	// deleted.
	bool remove(const QString& fileName);

	// The absolute path of the sidecar that describes `fileName`, or empty. For
	// callers that schedule a removal (a modpack update deletes dropped files
	// once the new version is in place); the name comes from the slug, so it
	// cannot be derived from the file name. Requires load() to have run.
	QString sidecarPathFor(const QString& fileName) const;

	// Moves the sidecar to follow a renamed file (e.g. `foo.jar` <->
	// `foo.jar.disabled`).
	void rename(const QString& oldFileName, const QString& newFileName);

	// Normalizes a mod name into a comparison key (lower-cased, parenthesized
	// suffixes stripped, non-alphanumeric removed, whitespace collapsed).
	// Mirrors DependencyResolver.
	static QString normalizeName(const QString& name);

	static QString canonicalFileName(const QString& fileName);

  private:
	QString sidecarPath(const QString& sidecarName) const;

	// Deletes `sidecarName` only if it describes `canonicalName`: the sidecar
	// is named after the project, so two versions of one mod share a name, and
	// removing one must not take the other's provenance.
	bool dropSidecarOwnedBy(const QString& sidecarName,
							const QString& canonicalName) const;

	// Legacy `.json` sidecars; read-only. Writing the entry again lands in a
	// `.pw.toml` and removes the old file.
	static Entry parseJson(const QByteArray& bytes);

	QDir m_folder;	 /* the mods/resourcepacks/... folder */
	QDir m_indexDir; /* <folder>/.index */
	mutable QMutex m_mutex;
	QHash<QString, Entry> m_entries; /* canonical fileName -> entry */

	// Canonical fileName -> the sidecar file it came from; the sidecar's name
	// comes from the slug, which the file name does not give.
	QHash<QString, QString> m_sidecars;
};
