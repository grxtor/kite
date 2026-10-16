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

#include "ModIconCache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QSaveFile>

namespace
{
	const QLatin1String kDisabledSuffix(".disabled");
	// Hex SHA-1.
	constexpr int kKeyLength = 40;
} // namespace

QString ModIconCache::directory()
{
	return QDir(QStringLiteral("cache/LocalModIcons")).absolutePath();
}

QString ModIconCache::keyFor(const QFileInfo& file)
{
	QString path = file.absoluteFilePath();
	if (path.endsWith(kDisabledSuffix)) {
		path.chop(kDisabledSuffix.size());
	}
	QCryptographicHash hash(QCryptographicHash::Sha1);
	hash.addData(path.toUtf8());
	hash.addData(QByteArrayLiteral("\n"));
	hash.addData(
		QByteArray::number(file.lastModified().toMSecsSinceEpoch()));
	return QString::fromLatin1(hash.result().toHex());
}

bool ModIconCache::isValidKey(const QString& key)
{
	if (key.size() != kKeyLength) {
		return false;
	}
	for (const QChar c : key) {
		const bool digit = c >= QLatin1Char('0') && c <= QLatin1Char('9');
		const bool hexLetter = c >= QLatin1Char('a') && c <= QLatin1Char('f');
		if (!digit && !hexLetter) {
			return false;
		}
	}
	return true;
}

QString ModIconCache::pathFor(const QString& cacheDir, const QString& key)
{
	return QDir(cacheDir).filePath(key + QStringLiteral(".png"));
}

bool ModIconCache::contains(const QString& cacheDir, const QString& key)
{
	return isValidKey(key) && QFileInfo::exists(pathFor(cacheDir, key));
}

bool ModIconCache::store(const QString& cacheDir, const QString& key,
						 const QByteArray& encoded)
{
	if (!isValidKey(key) || encoded.isEmpty() ||
		encoded.size() > kMaxEncodedBytes) {
		return false;
	}
	QImage image = QImage::fromData(encoded);
	if (image.isNull()) {
		return false;
	}
	if (image.width() > kMaxExtent || image.height() > kMaxExtent) {
		image = image.scaled(kMaxExtent, kMaxExtent, Qt::KeepAspectRatio,
							 Qt::SmoothTransformation);
	}
	if (!QDir().mkpath(cacheDir)) {
		return false;
	}
	QSaveFile out(pathFor(cacheDir, key));
	if (!out.open(QIODevice::WriteOnly)) {
		return false;
	}
	if (!image.save(&out, "PNG")) {
		out.cancelWriting();
		return false;
	}
	return out.commit();
}
