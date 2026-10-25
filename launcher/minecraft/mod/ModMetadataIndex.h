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

/*
 * ModMetadataIndex
 *
 * Persistent sidecar index that records how each managed file in a mod-
 * like folder was installed (which remote platform, which project, which
 * version, the SHA-1 we recorded, whether it was pulled in as a transitive
 * dependency, when it was installed).
 *
 * The index lives in `<folder>/.index/`, and the sidecars are packwiz
 * `*.pw.toml` files - the format the rest of this ecosystem reads and
 * writes - so an instance's folder stays usable by packwiz itself and by
 * other launchers. See PackwizSidecar.h for the format. Sidecars written
 * by earlier versions of this launcher (`<filename>.json`) are still read,
 * and are replaced with a TOML one the next time the entry is written.
 *
 * Stale `*.json` entries (whose target file is gone) are pruned on
 * `load()`. Orphaned `*.pw.toml` files are left alone: in a packwiz pack
 * the sidecar is the definition and the file next to it may simply not
 * have been downloaded yet, so deleting it would throw away the pack.
 *
 * The class is thread-safe: all mutations are guarded by an internal mutex
 * so the model thread and the install / dependency-resolver tasks can hit
 * it concurrently.
 */
class ModMetadataIndex
{
  public:
	struct Entry {
		/* Identity */
		QString fileName;  /* Base file name as stored on disk, no .disabled */
		QString platform;  /* "modrinth" | "curseforge" | "local" | ""       */
		QString projectId; /* Platform-specific project ID                   */
		QString versionId; /* Platform-specific version / file ID            */

		/* Descriptive */
		QString name; /* Human-readable mod name at install time         */
		QString slug; /* Platform slug, if known                         */
		QString side; /* "client" | "server" | "both" | "" if unknown    */
		QString downloadUrl;

		/* The version as a person reads it - "1.1.1+1.17" - kept apart
		 * from versionId, which is the platform's own opaque handle.
		 *
		 * They are not interchangeable and the two used to be confused: a
		 * Modrinth download URL spells out the number and carries no id
		 * at all, so a file installed from an mrpack has only this. Held
		 * separately so that it can be shown to the user without ever
		 * being sent somewhere an id is expected. */
		QString versionNumber;

		/* The SHA-1 we recorded for the file, and the digest the sidecar
		 * carries verbatim.
		 *
		 * These are not the same thing. Sidecars name their digest with a
		 * `hash-format`, and other tools store whatever the provider gave
		 * them - Modrinth is asked for SHA-512 first - so `sha1` stays
		 * empty for a foreign entry whose format says something else,
		 * rather than handing a SHA-512 to code that will compare it
		 * against a SHA-1 and conclude every file is corrupt. */
		QString sha1;
		QString hashFormat;
		QString hash;

		qint64 fileSize = 0;

		/* Flags */
		bool isDependency = false; /* installed as a transitive dep?         */

		/* Bookkeeping */
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

	/* Returns the absolute path of the sidecar directory (`<folder>/.index`).
	 * Creates it lazily if it does not yet exist. */
	QString indexDir() const
	{
		return m_indexDir.absolutePath();
	}

	/* Re-read every sidecar file from disk. Stale entries (no underlying
	 * file in `<folder>`) are silently dropped. Safe to call repeatedly. */
	void load();

	/* In-memory accessors. All return-by-value to keep callers off the
	 * mutex. The string keys are file names WITHOUT any `.disabled` suffix. */
	Entry get(const QString& fileName) const;
	bool contains(const QString& fileName) const;
	QList<Entry> all() const;
	/* Project ids of the entries installed from @p platform, one each. */
	QStringList projectIds(const QString& platform) const;

	/* Lookup helpers used by conflict / update analyzers. */
	Entry findByPlatformProject(const QString& platform,
								const QString& projectId) const;
	Entry findByNormalizedName(const QString& normalizedName) const;
	Entry findBySha1(const QString& sha1) const;

	/* Write (or overwrite) the sidecar for `entry.fileName`. Also flushes
	 * the in-memory cache. Returns true on success. */
	bool put(const Entry& entry);

	/* Remove sidecar associated with `fileName`. Returns true if a sidecar
	 * existed and was deleted. */
	bool remove(const QString& fileName);

	/* The absolute path of the sidecar that describes `fileName`, or an
	 * empty string when nothing loaded from this folder does.
	 *
	 * For callers that have to *schedule* a removal rather than perform
	 * one: a modpack update deletes the files it no longer ships only
	 * once the new version is safely in place, and the sidecar of a file
	 * that is going belongs on the same list. Read out here rather than
	 * rebuilt by the caller, because the sidecar's name comes from the
	 * project's slug and cannot be derived from the file name.
	 *
	 * Requires load() to have run. */
	QString sidecarPathFor(const QString& fileName) const;

	/* Move sidecar to follow a renamed file. Used when a mod is toggled
	 * (`foo.jar` <-> `foo.jar.disabled`) or otherwise renamed in place. */
	void rename(const QString& oldFileName, const QString& newFileName);

	/* Normalize a human-readable mod name into a comparison key:
	 * lower-cased, parenthesized suffixes stripped, non-alphanumeric
	 * removed, whitespace collapsed. Mirrors DependencyResolver. */
	static QString normalizeName(const QString& name);

	/* Strip a trailing `.disabled` from a file name, if present. */
	static QString canonicalFileName(const QString& fileName);

  private:
	QString sidecarPath(const QString& sidecarName) const;

	/* Delete `sidecarName` only when it really describes `canonicalName`.
	 *
	 * A sidecar is named after the project, not after the file, so two
	 * versions of one mod sitting in the folder together aim at the same
	 * name. Removing one of them must not take the other's provenance
	 * with it, so the file is read back before it is deleted. */
	bool dropSidecarOwnedBy(const QString& sidecarName,
							const QString& canonicalName) const;

	/* Legacy sidecars from before this folder spoke packwiz. Read-only:
	 * an entry that gets written again lands in a `.pw.toml` and the old
	 * file is removed, so a folder converts itself as it is used. */
	static Entry parseJson(const QByteArray& bytes);

	QDir m_folder;	 /* the mods/resourcepacks/... folder */
	QDir m_indexDir; /* <folder>/.index */
	mutable QMutex m_mutex;
	QHash<QString, Entry> m_entries; /* canonical fileName -> entry */

	/* Canonical fileName -> the sidecar file it was read from or written
	 * to. Needed because the sidecar's name comes from the slug, which
	 * cannot be recovered from the file name it describes. */
	QHash<QString, QString> m_sidecars;
};
