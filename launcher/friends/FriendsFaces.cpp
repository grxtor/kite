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

#include "FriendsFaces.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "BuildConfig.h"
#include "FriendsApi.h"
#include "core/LauncherContext.h"
#include "minecraft/skins/SkinFace.h"

namespace
{
	/* A face is looked at again after this long. */
	constexpr qint64 kFaceMaxAgeSeconds = 24 * 60 * 60;
	/* Mojang is asked about this many friends at once. */
	constexpr int kMaxParallel = 3;
	constexpr int kRequestTimeoutMs = 15000;
	/* A profile is a few hundred bytes plus a signed texture value; a skin
	 * is about 1 KiB. Anything far beyond is not one. */
	constexpr qint64 kMaxProfileBytes = 64 * 1024;
	constexpr qint64 kMaxSkinBytes = 256 * 1024;
	constexpr int kHttpOk = 200;
	/* Skins are 64 wide, 64 high (since 1.8) or 32 (older). */
	constexpr int kSkinWidth = 64;
	constexpr int kSkinHeight = 64;
	constexpr int kLegacySkinHeight = 32;
	const QString kTexturesHost = QStringLiteral("textures.minecraft.net");
	const QString kFaceSuffix = QStringLiteral(".png");
} // namespace

FriendsFaces::FriendsFaces(const QString& dir, QObject* parent)
	: QObject(parent), m_dir(dir)
{
}

FriendsFaces::~FriendsFaces()
{
	cancel();
}

QUrl FriendsFaces::profileUrl(const QString& sessionBase, const QString& uuid)
{
	return FriendsApi::url(sessionBase,
						   QStringLiteral("/session/minecraft/profile/") +
							   FriendsApi::undashed(uuid));
}

bool FriendsFaces::isLoopbackHost(const QString& host)
{
	const QString lower = host.toLower();
	return lower == QLatin1String("localhost") ||
		   lower == QLatin1String("127.0.0.1") ||
		   lower == QLatin1String("::1");
}

std::optional<QUrl> FriendsFaces::skinUrlFromProfile(const QByteArray& profile,
													 const QString& uuid,
													 bool allowLoopback)
{
	const QJsonObject root = QJsonDocument::fromJson(profile).object();
	if (root.value("id").toString().toLower() != FriendsApi::undashed(uuid)) {
		return std::nullopt;
	}
	for (const QJsonValue& property : root.value("properties").toArray()) {
		if (property.toObject().value("name").toString() != "textures") {
			continue;
		}
		const QByteArray textures = QByteArray::fromBase64(
			property.toObject().value("value").toString().toLatin1());
		QUrl url(QJsonDocument::fromJson(textures)
					 .object()
					 .value("textures")
					 .toObject()
					 .value("SKIN")
					 .toObject()
					 .value("url")
					 .toString());
		const QString host = url.host().toLower();
		const bool web = url.scheme() == "http" || url.scheme() == "https";
		if (!url.isValid() || !web || !url.userInfo().isEmpty() ||
			url.path().isEmpty()) {
			return std::nullopt;
		}
		if (host == kTexturesHost && url.port() == -1) {
			/* Mojang still words these as http. */
			url.setScheme("https");
			return url;
		}
		if (allowLoopback && isLoopbackHost(host)) {
			return url;
		}
		return std::nullopt;
	}
	return std::nullopt;
}

QImage FriendsFaces::faceFromSkinPng(const QByteArray& png)
{
	QImage skin;
	if (!skin.loadFromData(png, "PNG") || skin.width() != kSkinWidth ||
		(skin.height() != kSkinHeight && skin.height() != kLegacySkinHeight)) {
		return QImage();
	}
	return SkinFace::fromSkin(skin);
}

bool FriendsFaces::isStale(const QDateTime& modified, const QDateTime& now)
{
	return !modified.isValid() || modified.secsTo(now) >= kFaceMaxAgeSeconds;
}

QString FriendsFaces::pathOf(const QString& uuid) const
{
	return QDir(m_dir).filePath(FriendsApi::undashed(uuid) + kFaceSuffix);
}

QString FriendsFaces::faceUrl(const QString& uuid) const
{
	if (!FriendsApi::isUuid(uuid)) {
		return QString();
	}
	const QFileInfo file(pathOf(uuid));
	if (!file.exists()) {
		return QString();
	}
	return QUrl::fromLocalFile(file.absoluteFilePath()).toString() +
		   QStringLiteral("?rev=") +
		   QString::number(file.lastModified().toMSecsSinceEpoch());
}

void FriendsFaces::ensure(const QStringList& uuids, const QString& sessionBase)
{
	m_sessionBase = sessionBase;
	m_allowLoopback = isLoopbackHost(QUrl(sessionBase).host());
	const QDateTime now = QDateTime::currentDateTimeUtc();
	for (const QString& uuid : uuids) {
		if (!FriendsApi::isUuid(uuid) || m_queue.contains(uuid) ||
			m_running.contains(uuid)) {
			continue;
		}
		const QFileInfo file(pathOf(uuid));
		if (file.exists() && !isStale(file.lastModified(), now)) {
			continue;
		}
		if (m_tried.contains(uuid) && !isStale(m_tried.value(uuid), now)) {
			continue;
		}
		m_queue.append(uuid);
	}
	pump();
}

void FriendsFaces::pump()
{
	while (m_running.size() < kMaxParallel && !m_queue.isEmpty()) {
		const QString uuid = m_queue.takeFirst();
		m_running.insert(uuid);
		fetch(uuid);
	}
}

void FriendsFaces::fetch(const QString& uuid)
{
	const int epoch = m_epoch;
	m_tried.insert(uuid, QDateTime::currentDateTimeUtc());
	get(profileUrl(m_sessionBase, uuid), kMaxProfileBytes,
		[this, uuid, epoch](const QByteArray& profile) {
			if (epoch != m_epoch) {
				return;
			}
			const auto skin =
				skinUrlFromProfile(profile, uuid, m_allowLoopback);
			if (!skin) {
				finish(uuid, epoch);
				return;
			}
			get(*skin, kMaxSkinBytes,
				[this, uuid, epoch](const QByteArray& png) {
					if (epoch != m_epoch) {
						return;
					}
					const QImage face = faceFromSkinPng(png);
					if (!face.isNull() && QDir().mkpath(m_dir) &&
						face.save(pathOf(uuid), "PNG")) {
						emit changed();
					}
					finish(uuid, epoch);
				});
		});
}

void FriendsFaces::finish(const QString& uuid, int epoch)
{
	if (epoch != m_epoch) {
		return;
	}
	m_running.remove(uuid);
	pump();
}

void FriendsFaces::get(const QUrl& url, qint64 maxBytes, Body done)
{
	auto network = LAUNCHER ? LAUNCHER->network() : nullptr;
	if (!network) {
		done(QByteArray());
		return;
	}
	QNetworkRequest request(url);
	request.setTransferTimeout(kRequestTimeoutMs);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
						 QNetworkRequest::ManualRedirectPolicy);
	request.setRawHeader("User-Agent", BuildConfig.USER_AGENT.toUtf8());
	QNetworkReply* reply = network->get(request);
	m_replies.insert(reply);
	connect(reply, &QNetworkReply::downloadProgress, reply,
			[reply, maxBytes](qint64 received, qint64) {
				if (received > maxBytes) {
					reply->abort();
				}
			});
	connect(reply, &QNetworkReply::finished, this,
			[this, reply, done = std::move(done)]() {
				m_replies.remove(reply);
				const bool ok =
					reply->error() == QNetworkReply::NoError &&
					reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
							.toInt() == kHttpOk;
				const QByteArray body = ok ? reply->readAll() : QByteArray();
				reply->deleteLater();
				done(body);
			});
}

void FriendsFaces::cancel()
{
	++m_epoch;
	/* What was cut short did not fail: it may be asked again at once. */
	for (const QString& uuid : m_running) {
		m_tried.remove(uuid);
	}
	m_queue.clear();
	m_running.clear();
	/* abort() ends each with finished(), which finds the epoch moved on. */
	const auto replies = m_replies;
	for (QNetworkReply* reply : replies) {
		reply->abort();
	}
}

void FriendsFaces::clearCache()
{
	cancel();
	m_tried.clear();
	QDir(m_dir).removeRecursively();
}
