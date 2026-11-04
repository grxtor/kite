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

#include "PackLayout.h"

#include <QDir>
#include <QSet>
#include <QString>
#include <QStringList>

#include "FileSystem.h"
#include "MMCZip.h"

namespace
{

	// Zip entry names are not normalised by the spec: writers emit backslashes
	// and leading "./" or "/" at random. Fold that away so the checks below can
	// use plain "a/b/c" strings.
	QStringList normalizedEntries(const QString& zipPath)
	{
		QStringList out;
		const QStringList raw = MMCZip::listEntries(zipPath);
		out.reserve(raw.size());
		for (QString entry : raw) {
			entry.replace(QLatin1Char('\\'), QLatin1Char('/'));
			while (entry.startsWith(QLatin1String("./"))) {
				entry.remove(0, 2);
			}
			while (entry.startsWith(QLatin1Char('/'))) {
				entry.remove(0, 1);
			}
			if (entry.isEmpty()) {
				continue;
			}
			out.append(entry);
		}
		return out;
	}

	// The first path segment of an entry, or empty at the archive root.
	// Directory entries keep their trailing slash, so "foo/" yields "foo" like
	// "foo/bar".
	QString topLevelFolder(const QString& entry)
	{
		const int slash = entry.indexOf(QLatin1Char('/'));
		if (slash <= 0) {
			return QString();
		}
		return entry.left(slash);
	}

	bool anyEntryUnder(const QStringList& entries, const QString& prefix)
	{
		for (const QString& entry : entries) {
			if (entry.startsWith(prefix, Qt::CaseInsensitive) &&
				entry.size() > prefix.size()) {
				return true;
			}
		}
		return false;
	}

	bool hasSubDirectory(const QFileInfo& root, const QString& name)
	{
		return QFileInfo(FS::PathCombine(root.filePath(), name)).isDir();
	}

	bool hasFile(const QFileInfo& root, const QString& name)
	{
		return QFileInfo(FS::PathCombine(root.filePath(), name)).isFile();
	}

	// Distinct top-level folders in the archive, for the "wrapped in one
	// folder" case.
	QSet<QString> topLevelFolders(const QStringList& entries)
	{
		QSet<QString> folders;
		for (const QString& entry : entries) {
			const QString top = topLevelFolder(entry);
			if (!top.isEmpty()) {
				folders.insert(top);
			}
		}
		return folders;
	}

} // namespace

namespace PackLayout
{

	bool isShaderPack(const QFileInfo& file)
	{
		if (!file.exists()) {
			return false;
		}

		if (file.isDir()) {
			if (hasSubDirectory(file, QStringLiteral("shaders"))) {
				return true;
			}
			// Wrapped in a single folder, as produced by "download as zip"
			// on a Git forge and then extracted.
			const QDir dir(file.filePath());
			const QStringList children =
				dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
			for (const QString& child : children) {
				if (QFileInfo(FS::PathCombine(dir.path(), child,
											  QStringLiteral("shaders")))
						.isDir()) {
					return true;
				}
			}
			return false;
		}

		const QStringList entries = normalizedEntries(file.filePath());
		if (entries.isEmpty()) {
			return false;
		}

		for (const QString& entry : entries) {
			if (entry.startsWith(QStringLiteral("shaders/"),
								 Qt::CaseInsensitive) ||
				entry.contains(QStringLiteral("/shaders/"),
							   Qt::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	}

	namespace
	{
		// A `pack.mcmeta` beside a `dirName` directory ("data", "assets") at
		// the root or one level down: what data and resource packs share.
		bool hasMcmetaWith(const QFileInfo& file, const QString& dirName)
		{
			if (!file.exists()) {
				return false;
			}

			if (file.isDir()) {
				if (hasFile(file, QStringLiteral("pack.mcmeta")) &&
					hasSubDirectory(file, dirName)) {
					return true;
				}
				const QDir dir(file.filePath());
				const QStringList children =
					dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
				for (const QString& child : children) {
					const QFileInfo nested(FS::PathCombine(dir.path(), child));
					if (hasFile(nested, QStringLiteral("pack.mcmeta")) &&
						hasSubDirectory(nested, dirName)) {
						return true;
					}
				}
				return false;
			}

			const QStringList entries = normalizedEntries(file.filePath());
			if (entries.isEmpty()) {
				return false;
			}

			const QSet<QString> entrySet(entries.begin(), entries.end());
			if (entrySet.contains(QStringLiteral("pack.mcmeta")) &&
				anyEntryUnder(entries, dirName + QLatin1Char('/'))) {
				return true;
			}

			for (const QString& top : topLevelFolders(entries)) {
				if (entrySet.contains(top + QStringLiteral("/pack.mcmeta")) &&
					anyEntryUnder(entries, top + QLatin1Char('/') + dirName +
											   QLatin1Char('/'))) {
					return true;
				}
			}
			return false;
		}
	} // namespace

	bool isDataPack(const QFileInfo& file)
	{
		return hasMcmetaWith(file, QStringLiteral("data"));
	}

	bool isResourcePack(const QFileInfo& file)
	{
		return hasMcmetaWith(file, QStringLiteral("assets"));
	}

	QStringList archiveEntries(const QString& zipPath)
	{
		return normalizedEntries(zipPath);
	}

} // namespace PackLayout
