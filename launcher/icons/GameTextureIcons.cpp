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

#include "GameTextureIcons.h"
#include "GameTextureRecipes.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QVersionNumber>

#include <archive.h>
#include <archive_entry.h>

#include <memory>

#include "ArchiveOpen.h"
#include "FileSystem.h"
#include "Logging.h"

namespace
{
const QString kTextureRoot = QStringLiteral("assets/minecraft/textures/");

/* Written into a version's cache folder once every image is in it. The
 * number is the revision of GameTextureRecipes: bump it when they change,
 * and caches made from the old ones are redone instead of trusted. */
const QString kCacheMarker = QStringLiteral(".complete-2");

struct ArchiveReadDeleter {
	void operator()(struct archive* archive) const
	{
		archive_read_free(archive);
	}
};

QByteArray readEntry(struct archive* archive)
{
	QByteArray data;
	char buffer[8192];
	la_ssize_t read;
	while ((read = archive_read_data(archive, buffer, sizeof(buffer))) > 0) {
		data.append(buffer, read);
	}
	return read < 0 ? QByteArray() : data;
}

/* Every key's image from the jar at @p jarPath into @p images, in one
 * pass over it. False when the jar could not be read to its end -- one
 * still being written, or a damaged one -- which leaves @p images empty. */
bool readJar(const QString& jarPath, QHash<QString, QImage>* images)
{
	QSet<QString> wanted;
	for (const QString& path : GameTextureRecipes::texturePaths()) {
		wanted.insert(kTextureRoot + path);
	}

	std::unique_ptr<struct archive, ArchiveReadDeleter> archive(
		archive_read_new());
	archive_read_support_format_zip(archive.get());
	if (MMCArchive::openForReading(archive.get(), jarPath, 10240) !=
		ARCHIVE_OK) {
		qCWarning(iconsLog) << "Could not open the client jar" << jarPath
							<< archive_error_string(archive.get());
		return false;
	}

	QHash<QString, QImage> textures;
	struct archive_entry* entry;
	int result;
	while ((result = archive_read_next_header(archive.get(), &entry)) ==
		   ARCHIVE_OK) {
		const char* name = archive_entry_pathname(entry);
		const QString path = name ? QString::fromUtf8(name) : QString();
		if (!wanted.contains(path)) {
			continue;
		}
		QImage texture;
		if (texture.loadFromData(readEntry(archive.get()), "PNG")) {
			textures.insert(path.mid(kTextureRoot.size()),
							texture.convertToFormat(QImage::Format_ARGB32));
		}
	}
	if (result != ARCHIVE_EOF) {
		qCWarning(iconsLog) << "Could not read the client jar" << jarPath
							<< archive_error_string(archive.get());
		return false;
	}

	*images = GameTextureRecipes::compose(textures);
	return true;
}

/* Scales its image to whatever size is asked for, nearest-neighbour, so
 * every texture pixel lands on a block of whole device pixels instead of
 * being smoothed into its neighbours the way QIcon's own engine would. */
class PixelArtIconEngine : public QIconEngine
{
  public:
	explicit PixelArtIconEngine(const QImage& image) : m_image(image) {}

	void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode,
			   QIcon::State state) override
	{
		const qreal ratio = painter->device()->devicePixelRatio();
		QPixmap pixmap =
			this->pixmap((QSizeF(rect.size()) * ratio).toSize(), mode, state);
		pixmap.setDevicePixelRatio(ratio);
		const QSizeF drawn = QSizeF(pixmap.size()) / ratio;
		painter->drawPixmap(QRectF(rect).center() -
								QPointF(drawn.width(), drawn.height()) / 2,
							pixmap);
	}

	QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override
	{
		return m_image.size().scaled(size, Qt::KeepAspectRatio);
	}

	QPixmap pixmap(const QSize& size, QIcon::Mode, QIcon::State) override
	{
		if (m_image.isNull() || size.isEmpty()) {
			return QPixmap();
		}
		return QPixmap::fromImage(
			m_image.scaled(size, Qt::KeepAspectRatio, Qt::FastTransformation));
	}

	QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode,
						 QIcon::State state, qreal scale) override
	{
		QPixmap pixmap =
			this->pixmap((QSizeF(size) * scale).toSize(), mode, state);
		pixmap.setDevicePixelRatio(scale);
		return pixmap;
	}

	QList<QSize> availableSizes(QIcon::Mode, QIcon::State) override
	{
		return {m_image.size()};
	}

	bool isNull() override
	{
		return m_image.isNull();
	}

	QString key() const override
	{
		return QStringLiteral("KitePixelArt");
	}

	QIconEngine* clone() const override
	{
		return new PixelArtIconEngine(m_image);
	}

  private:
	QImage m_image;
};
} // namespace

QString GameTextureIcons::findClientJar(const QString& librariesDir,
										QString* version)
{
	// A release is plain numbers ("1.21.4", "26.3"); anything else --
	// "24w14a", "1.21-pre1", "26.4-snapshot-4" -- is a snapshot.
	static const QRegularExpression release(QStringLiteral("^\\d+(\\.\\d+)+$"));
	const QDir versions(
		FS::PathCombine(librariesDir, QStringLiteral("com/mojang/minecraft")));

	QString newestJar;
	QString newestName;
	QVersionNumber newest;
	for (const QString& name :
		 versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (!release.match(name).hasMatch()) {
			continue;
		}
		const QString jar = versions.filePath(
			QStringLiteral("%1/minecraft-%1-client.jar").arg(name));
		const QVersionNumber number = QVersionNumber::fromString(name);
		if (!QFileInfo(jar).isFile() ||
			(!newestJar.isEmpty() && number <= newest)) {
			continue;
		}
		newestJar = jar;
		newestName = name;
		newest = number;
	}
	if (version) {
		*version = newestName;
	}
	return newestJar;
}

QHash<QString, QImage> GameTextureIcons::readFromJar(const QString& jarPath)
{
	QHash<QString, QImage> images;
	readJar(jarPath, &images);
	return images;
}

QHash<QString, QImage> GameTextureIcons::load(const QString& jarPath,
											  const QString& version,
											  const QString& cacheDir)
{
	if (jarPath.isEmpty()) {
		return {};
	}

	const QDir cache(FS::PathCombine(cacheDir, version));
	const QString marker = cache.filePath(kCacheMarker);
	QHash<QString, QImage> images;
	if (QFileInfo::exists(marker)) {
		for (const QString& key : GameTextureRecipes::keys()) {
			const QImage image(cache.filePath(key + QStringLiteral(".png")));
			if (!image.isNull()) {
				images.insert(key,
							  image.convertToFormat(QImage::Format_ARGB32));
			}
		}
		return images;
	}

	// A jar that gave nothing -- unreadable, or cut short mid-download in a
	// way the reader cannot tell -- is not cached as having nothing, so the
	// next look tries it again.
	QElapsedTimer timer;
	timer.start();
	if (!readJar(jarPath, &images) || images.isEmpty()) {
		return images;
	}
	qCDebug(iconsLog) << "Read" << images.size() << "game textures from"
					  << jarPath << "in" << timer.elapsed() << "ms";
	if (!FS::ensureFolderPathExists(cache.absolutePath())) {
		return images;
	}
	bool saved = true;
	for (auto it = images.cbegin(); it != images.cend(); ++it) {
		const QString file = cache.filePath(it.key() + QStringLiteral(".png"));
		saved = it.value().save(file, "PNG") && saved;
	}
	QFile done(marker);
	if (saved && !done.open(QIODevice::WriteOnly)) {
		qCWarning(iconsLog) << "Could not finish the game texture cache"
							<< cache.absolutePath();
	}
	return images;
}

QIcon GameTextureIcons::pixelIcon(const QImage& image)
{
	return QIcon(new PixelArtIconEngine(image));
}
