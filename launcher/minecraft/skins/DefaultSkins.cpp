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

#include "DefaultSkins.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QUrl>

#include "core/LauncherContext.h"
#include "minecraft/skins/SkinEntry.h"

namespace
{
	const char* const kTextureBase = "https://textures.minecraft.net/texture/";

	/* The skins of the MHF_Steve and MHF_Alex accounts -- the addresses
	 * Parsers.cpp substitutes for a player without a skin (fallbackUrl()). */
	const char* const kClassicHash =
		"1a4af718455d4aab528e7a61f86fa25e6a369d1768dcb13f7df319a713eb810b";
	const char* const kSlimHash =
		"83cee5ca6afcdb171285aa00e8049c297b2dbeba0efb8ff970a5677a1b644032";

	/* The gallery: the game's nine default skins (1.19.3 and later), each
	 * as Mojang serves it for classic (wide) and slim arms. Steve and Alex
	 * here are the 1.19.3 textures, not the older MHF ones above. Checked
	 * pixel for pixel against the client jar's
	 * textures/entity/player/{wide,slim}/<name>.png. */
	struct GalleryEntry {
		const char* name;
		bool slim;
		const char* classicHash;
		const char* slimHash;
	};
	const GalleryEntry kGallery[] = {
		{ "Steve", false,
		  "31f477eb1a7beee631c2ca64d06f8f68fa93a3386d04452ab27f43acdf1b60cb",
		  "d5c4ee5ce20aed9e33e866c66caa37178606234b3721084bf01d13320fb2eb3f" },
		{ "Alex", true,
		  "1abc803022d8300ab7578b189294cce39622d9a404cdc00d3feacfdf45be6981",
		  "46acd06e8483b176e8ea39fc12fe105eb3a2a4970f5100057e9d84d4b60bdfa7" },
		{ "Ari", false,
		  "4c05ab9e07b3505dc3ec11370c3bdce5570ad2fb2b562e9b9dd9cf271f81aa44",
		  "6ac6ca262d67bcfb3dbc924ba8215a18195497c780058a5749de674217721892" },
		{ "Efe", true,
		  "daf3d88ccb38f11f74814e92053d92f7728ddb1a7955652a60e30cb27ae6659f",
		  "fece7017b1bb13926d1158864b283b8b930271f80a90482f174cca6a17e88236" },
		{ "Kai", false,
		  "e5cdc3243b2153ab28a159861be643a4fc1e3c17d291cdd3e57a7f370ad676f3",
		  "226c617fde5b1ba569aa08bd2cb6fd84c93337532a872b3eb7bf66bdd5b395f8" },
		{ "Makena", true,
		  "dc0fcfaf2aa040a83dc0de4e56058d1bbb2ea40157501f3e7d15dc245e493095",
		  "7cb3ba52ddd5cc82c0b050c3f920f87da36add80165846f479079663805433db" },
		{ "Noor", true,
		  "90e75cd429ba6331cd210b9bd19399527ee3bab467b5a9f61cb8a27b177f6789",
		  "6c160fbd16adbc4bff2409e70180d911002aebcfa811eb6ec3d1040761aea6dd" },
		{ "Sunny", false,
		  "a3bd16079f764cd541e072e888fe43885e711f98658323db0f9a6045da91ee7a",
		  "b66bc80f002b10371e2fa23de6f230dd5e2f3affc2e15786f65bc9be4c6eb71a" },
		{ "Zuri", false,
		  "f5dddb41dcafef616e959c2817808e0be741c89ffbfed39134a13e75b811863d",
		  "eee522611005acf256dbd152e992c60c0bb7978cb0f3127807700e478ad97664" },
	};

	QString textureUrl(const char* hash)
	{
		return QLatin1String(kTextureBase) + QLatin1String(hash);
	}

	constexpr int kUuidHexDigits = 32;
	constexpr int kHalfHexDigits = 16;
	constexpr int kHexBase = 16;
	constexpr int kHalfBits = 32;
	constexpr int kTextureSize = 64;
} // namespace

DefaultSkins* DefaultSkins::instance()
{
	/* Parented to the application, so it goes away with it rather than at
	 * static destruction, after the network stack it may still hold a reply
	 * from. */
	static QPointer<DefaultSkins> s_instance;
	if (!s_instance) {
		s_instance = new DefaultSkins(QCoreApplication::instance());
	}
	return s_instance;
}

DefaultSkins::DefaultSkins(QObject* parent) : QObject(parent) {}

bool DefaultSkins::isSlimFor(const QString& uuid)
{
	QString hex = uuid;
	hex.remove(QLatin1Char('-'));
	if (hex.size() != kUuidHexDigits) {
		return false;
	}
	bool okHigh = false;
	bool okLow = false;
	const quint64 high = hex.left(kHalfHexDigits).toULongLong(&okHigh, kHexBase);
	const quint64 low = hex.right(kHalfHexDigits).toULongLong(&okLow, kHexBase);
	if (!okHigh || !okLow) {
		return false;
	}
	/* Java's UUID.hashCode(): xor the two halves, then the halves of that. */
	const quint64 folded = high ^ low;
	const quint32 hashCode = static_cast<quint32>(folded >> kHalfBits) ^
							 static_cast<quint32>(folded);
	return hashCode % 2 == 1;
}

bool DefaultSkins::isSlim(const MinecraftAccountPtr& account)
{
	if (!account) {
		return false;
	}
	const QString& variant = account->accountData()->minecraftProfile.skin.variant;
	if (!variant.isEmpty()) {
		return variant.compare(QLatin1String("SLIM"), Qt::CaseInsensitive) == 0;
	}
	return isSlimFor(account->gameUuid());
}

QImage DefaultSkins::texture(bool slim) const
{
	return slim ? m_slim : m_classic;
}

void DefaultSkins::setTexture(bool slim, const QImage& image)
{
	(slim ? m_slim : m_classic) = image;
	emit loaded();
}

void DefaultSkins::ensureLoaded()
{
	/* The cache and the network both belong to a running launcher: the
	 * data folder is its working directory. */
	if (!LauncherContext::instance()) {
		return;
	}
	for (const bool slim : { false, true }) {
		const bool fetching = slim ? m_fetchingSlim : m_fetchingClassic;
		if (!texture(slim).isNull() || fetching || loadCached(slim)) {
			continue;
		}
		fetch(slim);
	}
}

QString DefaultSkins::cachePath(bool slim)
{
	return QDir(QStringLiteral("cache/skins"))
		.absoluteFilePath(slim ? QStringLiteral("default-slim.png")
							   : QStringLiteral("default-classic.png"));
}

bool DefaultSkins::loadCached(bool slim)
{
	QImage image;
	if (!image.load(cachePath(slim), "PNG")) {
		return false;
	}
	const QImage texture = SkinEntry::normalisedTexture(image);
	if (texture.width() != kTextureSize || texture.height() != kTextureSize) {
		return false;
	}
	/* Set without loaded(): this runs inside a caller that is about to read
	 * texture() anyway (an image request, accountSkinInfo()), and nothing
	 * else can have drawn without it yet. */
	(slim ? m_slim : m_classic) = texture;
	return true;
}

void DefaultSkins::fetch(bool slim)
{
	auto* context = LauncherContext::instance();
	if (!context || !context->network()) {
		return;
	}
	(slim ? m_fetchingSlim : m_fetchingClassic) = true;
	QNetworkReply* reply = context->network()->get(
		QNetworkRequest(QUrl(fallbackUrl(slim))));
	connect(reply, &QNetworkReply::finished, this,
			[this, reply, slim] { onFinished(reply, slim); });
}

void DefaultSkins::onFinished(QNetworkReply* reply, bool slim)
{
	reply->deleteLater();
	/* Cleared either way: a failed fetch is tried again the next time
	 * someone asks (ensureLoaded()), e.g. once the network is back. */
	(slim ? m_fetchingSlim : m_fetchingClassic) = false;
	if (reply->error() != QNetworkReply::NoError) {
		qWarning() << "Could not fetch the default skin:"
				   << reply->errorString();
		return;
	}
	QImage image;
	if (!image.loadFromData(reply->readAll(), "PNG")) {
		qWarning() << "The default skin is not a PNG";
		return;
	}
	const QImage texture = SkinEntry::normalisedTexture(image);
	if (texture.width() != kTextureSize || texture.height() != kTextureSize) {
		return;
	}
	/* Kept for the next run, offline or not; failing to save only means
	 * fetching again next time. The addresses are content hashes, so the
	 * copy never goes stale. */
	const QString path = cachePath(slim);
	if (!QDir().mkpath(QFileInfo(path).absolutePath()) ||
		!texture.save(path, "PNG")) {
		qWarning() << "Could not keep the default skin at" << path;
	}
	setTexture(slim, texture);
}

QString DefaultSkins::fallbackUrl(bool slim)
{
	return textureUrl(slim ? kSlimHash : kClassicHash);
}

QList<DefaultSkins::Character> DefaultSkins::characters()
{
	QList<Character> list;
	for (const GalleryEntry& entry : kGallery) {
		list.append({ QLatin1String(entry.name), entry.slim });
	}
	return list;
}

QString DefaultSkins::galleryUrl(const QString& name, bool slim)
{
	for (const GalleryEntry& entry : kGallery) {
		if (name == QLatin1String(entry.name)) {
			return textureUrl(slim ? entry.slimHash : entry.classicHash);
		}
	}
	return QString();
}

QString DefaultSkins::galleryPath(const QString& name, bool slim)
{
	/* Named by the texture's hash, as on textures.minecraft.net: a file
	 * here is that exact texture, whatever the table above maps to it. */
	const QString hash = QUrl(galleryUrl(name, slim)).fileName();
	return hash.isEmpty() ? QString()
						  : QDir(QStringLiteral("cache/skins/default"))
								.absoluteFilePath(hash + QStringLiteral(".png"));
}

bool DefaultSkins::galleryLoading() const
{
	return !m_galleryFetching.isEmpty();
}

void DefaultSkins::fetchGallery()
{
	auto* context = LauncherContext::instance();
	if (!context || !context->network()) {
		return;
	}
	bool started = false;
	for (const Character& character : characters()) {
		for (const bool slim : { false, true }) {
			const QString path = galleryPath(character.name, slim);
			if (QFileInfo::exists(path) || m_galleryFetching.contains(path)) {
				continue;
			}
			m_galleryFetching.insert(path);
			started = true;
			QNetworkReply* reply = context->network()->get(
				QNetworkRequest(QUrl(galleryUrl(character.name, slim))));
			connect(reply, &QNetworkReply::finished, this,
					[this, reply, path] { onGalleryFinished(reply, path); });
		}
	}
	if (started) {
		emit galleryChanged();
	}
}

void DefaultSkins::onGalleryFinished(QNetworkReply* reply, const QString& path)
{
	reply->deleteLater();
	/* Cleared either way: a failed download is tried again the next time
	 * the gallery is asked for (the skin editor opening again). */
	m_galleryFetching.remove(path);
	const QByteArray bytes = reply->error() == QNetworkReply::NoError
								 ? reply->readAll()
								 : QByteArray();
	QImage image;
	if (!image.loadFromData(bytes, "PNG") || image.width() != kTextureSize ||
		image.height() != kTextureSize) {
		qWarning() << "Could not fetch the default skin" << reply->url()
				   << reply->errorString();
		emit galleryChanged();
		return;
	}
	/* The file as Mojang serves it, since it is also what gets uploaded
	 * when the player wears it; written whole or not at all, so a file that
	 * exists is always a finished one. */
	QSaveFile file(path);
	if (!QDir().mkpath(QFileInfo(path).absolutePath()) ||
		!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
		!file.commit()) {
		qWarning() << "Could not keep the default skin at" << path;
	}
	emit galleryChanged();
}
