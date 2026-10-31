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

#include "PackContents.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include "FileSystem.h"

namespace PackContents
{
	namespace
	{
		/* Bumped only when the meaning of what is on disk changes. Readers
		 * refuse unknown versions: the list drives deletions. */
		const int currentFormatVersion = 1;

		const char* const listFileName = "pack-contents.json";
		const char* const keyFormatVersion = "formatVersion";
		const char* const keyPaths = "paths";

		/* Length of ".disabled", the suffix both the launcher and pack formats use
		 * for a mod kept on disk but not loaded. */
		const int disabledSuffixLength = 9;

		QString disabledSuffix()
		{
			return QStringLiteral(".disabled");
		}

		/* The other name the same file could have, or empty. */
		QString counterpartOf(const QString& path)
		{
			if (!path.endsWith(disabledSuffix(), Qt::CaseInsensitive)) {
				return path + disabledSuffix();
			}

			const QString stripped =
				path.left(path.size() - disabledSuffixLength);
			/* A bare ".disabled" is not a renamed file, and stripping it would name
			 * the containing *folder*; this list feeds a delete loop. */
			if (stripped.isEmpty() || stripped.endsWith(QLatin1Char('/'))) {
				return {};
			}
			return stripped;
		}

		QStringList normalizeAll(const QStringList& paths)
		{
			QStringList out;
			out.reserve(paths.size());
			for (const QString& path : paths) {
				const QString clean = normalizePath(path);
				if (clean.isEmpty()) {
					continue;
				}
				out.append(clean);
			}
			out.removeDuplicates();
			out.sort();
			return out;
		}
	} // namespace

	QString listPath(const QString& instanceRoot)
	{
		return FS::PathCombine(instanceRoot, QLatin1String(listFileName));
	}

	QString normalizePath(const QString& path)
	{
		if (path.isEmpty()) {
			return {};
		}

		/* An instance directory travels between machines, so the stored form is
		 * always forward slashes. */
		QString clean =
			QString(path).replace(QLatin1Char('\\'), QLatin1Char('/'));
		clean = QDir::cleanPath(clean);

		if (clean.isEmpty() || clean == QLatin1String(".")) {
			return {};
		}
		if (QDir::isAbsolutePath(clean) || clean.startsWith(QLatin1Char('/'))) {
			/* An absolute path would point outside the instance and still reach a
			 * delete loop. */
			return {};
		}

		/* cleanPath resolves "a/../b", so a ".." left over climbs above the game
		 * directory. */
		const QStringList segments =
			clean.split(QLatin1Char('/'), Qt::SkipEmptyParts);
		if (segments.contains(QLatin1String(".."))) {
			return {};
		}

		return segments.join(QLatin1Char('/'));
	}

	bool write(const QString& instanceRoot,
			   const QStringList& gameRelativePaths)
	{
		const QString path = listPath(instanceRoot);

		QJsonArray paths;
		for (const QString& entry : normalizeAll(gameRelativePaths)) {
			paths.append(entry);
		}

		QJsonObject root;
		root.insert(QLatin1String(keyFormatVersion), currentFormatVersion);
		root.insert(QLatin1String(keyPaths), paths);

		const QByteArray payload =
			QJsonDocument(root).toJson(QJsonDocument::Indented);

		/* QSaveFile: a truncated list would make the next update treat
		 * everything missing from it as the user's own file. */
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qWarning() << "PackContents: cannot open" << path << "for writing";
			return false;
		}
		if (file.write(payload) != payload.size()) {
			file.cancelWriting();
			qWarning() << "PackContents: short write to" << path;
			return false;
		}
		if (!file.commit()) {
			qWarning() << "PackContents: failed to commit" << path;
			return false;
		}
		return true;
	}

	bool read(const QString& instanceRoot, QStringList& out)
	{
		const QString path = listPath(instanceRoot);

		QFile file(path);
		if (!file.exists()) {
			/* Installed before this was recorded, or by something else: unknown, not
			 * an error. */
			return false;
		}
		if (!file.open(QIODevice::ReadOnly)) {
			qWarning() << "PackContents: cannot read" << path;
			return false;
		}
		const QByteArray bytes = file.readAll();
		file.close();

		QJsonParseError error{};
		const QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
		if (error.error != QJsonParseError::NoError || !doc.isObject()) {
			qWarning() << "PackContents: malformed list at" << path;
			return false;
		}

		const QJsonObject root = doc.object();
		const int version =
			root.value(QLatin1String(keyFormatVersion)).toInt(0);
		if (version != currentFormatVersion) {
			qWarning() << "PackContents: unsupported format version" << version
					   << "at" << path;
			return false;
		}

		const QJsonValue pathsValue = root.value(QLatin1String(keyPaths));
		if (!pathsValue.isArray()) {
			qWarning() << "PackContents: list at" << path
					   << "carries no paths array";
			return false;
		}

		QStringList paths;
		for (const QJsonValue& entry : pathsValue.toArray()) {
			if (!entry.isString()) {
				continue;
			}
			paths.append(entry.toString());
		}

		out = normalizeAll(paths);
		return true;
	}

	QStringList staleEntries(const QStringList& oldPaths,
							 const QStringList& newPaths)
	{
		const QStringList oldClean = normalizeAll(oldPaths);
		const QStringList newClean = normalizeAll(newPaths);

		QSet<QString> shipped(newClean.begin(), newClean.end());

		QStringList stale;
		for (const QString& path : oldClean) {
			if (shipped.contains(path)) {
				continue;
			}
			stale.append(path);

			const QString counterpart = counterpartOf(path);
			if (!counterpart.isEmpty() && !shipped.contains(counterpart)) {
				stale.append(counterpart);
			}
		}

		stale.removeDuplicates();
		stale.sort();
		return stale;
	}
} // namespace PackContents
