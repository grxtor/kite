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
#include <QUrl>

#include "core/LauncherContext.h"
#include "minecraft/skins/SkinEntry.h"

namespace
{
	/* The skins of the MHF_Steve and MHF_Alex accounts -- the same two
	 * addresses Parsers.cpp substitutes for a player without a skin. */
	const char* const kClassicUrl =
		"https://textures.minecraft.net/texture/"
		"1a4af718455d4aab528e7a61f86fa25e6a369d1768dcb13f7df319a713eb810b";
	const char* const kSlimUrl =
		"https://textures.minecraft.net/texture/"
		"83cee5ca6afcdb171285aa00e8049c297b2dbeba0efb8ff970a5677a1b644032";

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
		QNetworkRequest(QUrl(QLatin1String(slim ? kSlimUrl : kClassicUrl))));
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
