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

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <optional>

#include "FriendsApi.h"
#include "ShareCodes.h"
#include "ShareManifest.h"

class FriendsController;
namespace Net
{
	class JsonPost;
}

/*
 * Share codes for QML (QmlShell.shareCodes): "Share as code" on an instance
 * (prepare(), then share()), your codes (refreshCodes(), deleteCode()), and a
 * new instance from someone's code (lookUp(), then createInstance()). Talks
 * to the friends server with FriendsController's session, so Friends must be
 * on. Every invokable that asks the server ends with actionFinished().
 */
class ShareCodesController : public QObject
{
	Q_OBJECT

	/// Friends is on: codes need its sign-in.
	Q_PROPERTY(bool available READ available NOTIFY availableChanged)
	/// Your live codes, newest first (ShareCodes::toMap(), without counts).
	Q_PROPERTY(QVariantList codes READ codes NOTIFY codesChanged)
	/// False until refreshCodes() has an answer.
	Q_PROPERTY(bool codesLoaded READ codesLoaded NOTIFY codesChanged)

  public:
	explicit ShareCodesController(FriendsController* friends,
								  QObject* parent = nullptr);
	~ShareCodesController() override;

	bool available() const;
	QVariantList codes() const;
	bool codesLoaded() const;

	/// Works out what a code of instance @p id would carry and emits
	/// prepared(): ShareManifest::toMap(), with `error` set when it cannot
	/// be shared. Asks Modrinth about files their sidecar does not name.
	Q_INVOKABLE void prepare(const QString& instanceId);
	/// Makes a code of what prepare() worked out; emits shared() on success.
	Q_INVOKABLE void share(const QString& instanceId);
	Q_INVOKABLE void refreshCodes();
	Q_INVOKABLE void deleteCode(const QString& code);

	/// "KITE-7F3K-9QXM" for anything ShareCodes::normalize() takes, else "".
	Q_INVOKABLE QString display(const QString& raw) const;
	/// Fetches a code and emits lookedUp() with ShareCodes::toMap().
	Q_INVOKABLE void lookUp(const QString& raw);
	/// A new instance from the code lookUp() found; a TaskWatcher, or null
	/// (with actionFinished()) when it cannot start.
	Q_INVOKABLE QObject* createInstance(const QString& name, const QString& group,
										const QString& iconKey);

  signals:
	void availableChanged();
	void codesChanged();
	void prepared(QString instanceId, QVariantMap preview);
	void shared(QString instanceId, QString code);
	void lookedUp(QVariantMap preview);
	void actionFinished(QString action, bool ok, QString message);

  private:
	void finishAction(const QString& action, bool ok, const QString& message);
	void emitPrepared(const QString& instanceId);
	QString errorText(const FriendsApi::Reply& reply, bool lookingUp) const;
	QString loaderUid(const QString& loader) const;

	FriendsController* m_friends;
	QHash<QString, ShareManifest::Built> m_prepared;
	std::unique_ptr<Net::JsonPost> m_hashLookup;
	std::optional<ShareCodes::Code> m_lookedUp;
	QVariantList m_codes;
	bool m_codesLoaded = false;
	/// ShareCodeInstanceTask::missing() of the last instance made.
	QStringList m_lastMissing;
};
