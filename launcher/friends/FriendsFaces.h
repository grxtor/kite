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

#pragma once

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <optional>

class QNetworkReply;

/*
 * The friends' heads. The friends server only knows names and UUIDs, so the
 * head of each person comes from Mojang, the source of the skin: the
 * session server's profile (its "textures" property) names the skin, the
 * skin comes from textures.minecraft.net, and the face is cut out of it and
 * kept as a small PNG under <data>/cache/friendfaces/. Nobody else is ever
 * asked about a friend's UUID.
 *
 * ensure() fetches what is missing or older than a day, a few at a time;
 * every failure is silent and tried again the next day (or the next start).
 * changed() says a face arrived. The pure parts are static, so they are
 * tested without a network.
 */
class FriendsFaces : public QObject
{
	Q_OBJECT

  public:
	/// @p dir is where the PNGs are kept.
	explicit FriendsFaces(const QString& dir, QObject* parent = nullptr);
	~FriendsFaces() override;

	/// "file:///.../<uuid>.png?rev=<time>" for a cached face, "" for none.
	/// The rev changes with the file, so a replaced face is loaded again.
	QString faceUrl(const QString& uuid) const;

	/// The PNG of a cached face, "" for none or for something that is not a
	/// UUID with dashes (the mod bridge serves it).
	QString cachedFacePath(const QString& uuid) const;

	/// Queues the faces of @p uuids that are not cached or are stale.
	/// @p sessionBase is the Mojang session server (the FriendsMojangSessionUrl
	/// setting).
	void ensure(const QStringList& uuids, const QString& sessionBase);
	/// Drops the queue and aborts what is running. Their answers are ignored.
	void cancel();
	/// cancel(), and deletes every cached face.
	void clearCache();

	/// <sessionBase>/session/minecraft/profile/<uuid without dashes>.
	static QUrl profileUrl(const QString& sessionBase, const QString& uuid);
	/// The skin's URL from a session server profile: the "textures"
	/// property, base64 of JSON, whose textures.SKIN.url must be https (or
	/// http, which is upgraded) on textures.minecraft.net and the profile's
	/// id must be @p uuid. @p allowLoopback additionally accepts a loopback
	/// host as it is, for a local mock of Mojang; nothing else is accepted.
	static std::optional<QUrl> skinUrlFromProfile(const QByteArray& profile,
												  const QString& uuid,
												  bool allowLoopback);
	/// The head (face with the hat over it, 8x8) of a skin PNG. Null for
	/// anything that is not a 64x64 or 64x32 skin.
	static QImage faceFromSkinPng(const QByteArray& png);
	/// Whether a face last written at @p modified needs fetching again.
	static bool isStale(const QDateTime& modified, const QDateTime& now);
	/// Whether @p host is this machine (localhost, 127.0.0.1, ::1).
	static bool isLoopbackHost(const QString& host);

  signals:
	void changed();

  private:
	using Body = std::function<void(const QByteArray&)>;

	QString pathOf(const QString& uuid) const;
	void pump();
	void fetch(const QString& uuid);
	void finish(const QString& uuid, int epoch);
	/// GET @p url; @p done gets the body of a 200 no larger than @p maxBytes,
	/// else an empty one. A redirect is not followed.
	void get(const QUrl& url, qint64 maxBytes, Body done);

	QString m_dir;
	QString m_sessionBase;
	bool m_allowLoopback = false;
	QStringList m_queue;
	QSet<QString> m_running;
	/// When each uuid was last tried, so a failure waits a day.
	QHash<QString, QDateTime> m_tried;
	QSet<QNetworkReply*> m_replies;
	/// Counts cancel(), so an answer from before it is dropped.
	int m_epoch = 0;
};
