/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "SafeFiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

#include "java/download/JavaRuntime.h"

namespace LauncherImport
{
	bool readCapped(const QString& path, qint64 maxBytes, QByteArray* out,
					bool* tooLarge)
	{
		if (tooLarge) {
			*tooLarge = false;
		}
		const QFileInfo info(path);
		if (!info.isFile()) {
			return false;
		}
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return false;
		}
		/* The size is asked of the open file and the read is capped too: the
		 * file can grow between the two. */
		if (file.size() > maxBytes) {
			if (tooLarge) {
				*tooLarge = true;
			}
			return false;
		}
		*out = file.read(maxBytes + 1);
		if (out->size() > maxBytes) {
			out->clear();
			if (tooLarge) {
				*tooLarge = true;
			}
			return false;
		}
		return true;
	}

	bool readJsonObject(const QString& path, qint64 maxBytes, QJsonObject* out,
						QString* error)
	{
		QByteArray bytes;
		bool tooLarge = false;
		if (!readCapped(path, maxBytes, &bytes, &tooLarge)) {
			if (error) {
				*error = tooLarge
							 ? QCoreApplication::translate(
								   "LauncherImport",
								   "Its details file is too large to read.")
							 : QCoreApplication::translate(
								   "LauncherImport",
								   "Its details file is missing.");
			}
			return false;
		}
		QJsonParseError parseError;
		const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
		if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
			if (error) {
				*error = QCoreApplication::translate(
					"LauncherImport", "Its details file is damaged.");
			}
			return false;
		}
		*out = doc.object();
		return true;
	}

	bool isLink(const QFileInfo& info)
	{
		return info.isSymbolicLink() || info.isJunction() || info.isAlias();
	}

	bool resolvesInside(const QString& root, const QString& path)
	{
		const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
		const QString canonicalPath = QFileInfo(path).canonicalFilePath();
		if (canonicalRoot.isEmpty() || canonicalPath.isEmpty()) {
			return false;
		}
		return canonicalPath == canonicalRoot ||
			   JavaDownload::isInside(canonicalRoot, canonicalPath);
	}

	bool isUsableDir(const QString& root, const QString& path)
	{
		const QFileInfo info(path);
		if (isLink(info)) {
			return resolvesInside(root, path) && QFileInfo(info.canonicalFilePath()).isDir();
		}
		return info.isDir();
	}

	QString usableIconFile(const QString& root, const QString& path)
	{
		static const QStringList pictures = {
			QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
			QStringLiteral("gif"), QStringLiteral("svg")};
		if (path.isEmpty()) {
			return {};
		}
		const QFileInfo info(path);
		if (!info.isFile() || info.size() > kMaxIconBytes ||
			!pictures.contains(info.suffix().toLower()) ||
			!resolvesInside(root, path)) {
			return {};
		}
		return info.canonicalFilePath();
	}

	bool isSafeName(const QString& name)
	{
		return JavaDownload::isSafeSegment(name);
	}

	QString joinSafe(const QString& root, const QString& relative)
	{
		return JavaDownload::joinInside(root, relative);
	}
} // namespace LauncherImport
