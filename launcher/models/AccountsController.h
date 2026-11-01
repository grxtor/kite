/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QPointer>
#include <QVariantMap>

#include "QObjectPtr.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"

class SkinsController;
class MicrosoftLoginController;

class AccountTask;

/*
 * QML-facing accounts controller: account actions plus the Microsoft sign-in
 * flow. `accounts` hands QML the AccountList model directly.
 */
class AccountsController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QObject* accounts READ accounts CONSTANT)
	/// accountId role of the default account (what rows compare against), or empty.
	Q_PROPERTY(
		QString defaultAccountId READ defaultAccountId NOTIFY defaultChanged)
	/// Bumped when the game's default skins arrive, so pages refetch skinless images.
	Q_PROPERTY(int defaultSkinsRevision READ defaultSkinsRevision NOTIFY
				   defaultSkinsRevisionChanged)
	Q_PROPERTY(bool skinDemoRequested READ skinDemoRequested CONSTANT)
	Q_PROPERTY(QString skinDemoPose READ skinDemoPose CONSTANT)

  public:
	explicit AccountsController(shared_qobject_ptr<AccountList> accounts,
								QObject* parent = nullptr);

	QObject* accounts() const;
	QString defaultAccountId() const;
	int defaultSkinsRevision() const;

	Q_INVOKABLE void setDefault(int row);
	Q_INVOKABLE void clearDefault();
	Q_INVOKABLE void remove(int row);
	/// Runs ahead of the background refresh queue.
	Q_INVOKABLE void refresh(int row);

	/// False if a Microsoft account that owns Minecraft is not in the list, the
	/// trimmed name is empty, or it collides (case-insensitively) with an offline account.
	Q_INVOKABLE bool addOffline(const QString& username);

	/// The controller is owned by C++: the login must outlive the page that started it.
	Q_INVOKABLE QObject* loginMicrosoft();

	/// { "valid", "slim", "hasSkin", "defaultSkin", "currentCapeId",
	/// "currentCapeUrl", "capes": [{ "id", "alias", "url" }] }. "valid" is false
	/// for a bad row, an offline account or an MSA account with no profile data
	/// yet. "hasSkin"/"slim" describe the player's skin, or the default one when
	/// "defaultSkin" is true.
	Q_INVOKABLE QVariantMap accountSkinInfo(int row) const;

	/// True when KITE_QML_ROUTE contains "accounts-demo"; accountSkinInfo(-1)
	/// then returns canned demo data.
	bool skinDemoRequested() const;

	/// Demo editor start view: "cape", "elytra", "default"; anything else is the front.
	QString skinDemoPose() const;

	/// Empty if @p path (local path or file:// URL) is a skin MinecraftServices
	/// accepts, otherwise a user-facing reason.
	Q_INVOKABLE QString validateSkinFile(const QString& path) const;

	/// @p path must pass validateSkinFile(). Returns a TaskWatcher; nullptr if
	/// @p row is out of range or not Microsoft, or @p path cannot be opened.
	Q_INVOKABLE QObject* changeSkin(int row, const QString& path, bool slim);

	/// Returns a TaskWatcher; nullptr for a bad row or a non-Microsoft account.
	Q_INVOKABLE QObject* resetSkin(int row);

	/// An empty @p capeId takes the cape off. Ownership is not checked here; the
	/// server rejects a cape the account does not own. Return as changeSkin().
	Q_INVOKABLE QObject* changeCape(int row, const QString& capeId);

	/// Skin library for row @p row's account; one is kept at a time. Null for a bad row.
	Q_INVOKABLE QObject* skins(int row);

  signals:
	void defaultChanged();
	void defaultSkinsRevisionChanged();

  private:
	shared_qobject_ptr<AccountList> m_accounts;
	// Parented to this; nothing else pins its ownership for QML.
	QPointer<SkinsController> m_skins;
	// The latest sign-in; a new attempt retires the one before it.
	QPointer<MicrosoftLoginController> m_login;
	QString m_skinsAccountId;
	int m_defaultSkinsRevision = 0;
};

/*
 * One Microsoft sign-in attempt (OAuth2 authorization-code flow in the system
 * browser; there is no device code). browserUrl and openBrowser() are for a
 * user who closed the tab.
 */
class MicrosoftLoginController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString status READ status NOTIFY statusChanged)
	Q_PROPERTY(QUrl browserUrl READ browserUrl NOTIFY browserUrlChanged)
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	Q_PROPERTY(bool succeeded READ succeeded NOTIFY succeededChanged)
	Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit MicrosoftLoginController(shared_qobject_ptr<AccountList> accounts,
									  QObject* parent = nullptr);
	~MicrosoftLoginController() override;

	QString status() const
	{
		return m_status;
	}
	QUrl browserUrl() const
	{
		return m_browserUrl;
	}
	bool running() const
	{
		return m_running;
	}
	bool succeeded() const
	{
		return m_succeeded;
	}
	bool failed() const
	{
		return m_failed;
	}
	QString error() const
	{
		return m_error;
	}

	/// Drops the pending account and task; an in-flight OAuth2 request cannot be
	/// cancelled. No-op once finished.
	Q_INVOKABLE void cancel();
	Q_INVOKABLE void openBrowser();

  signals:
	void statusChanged();
	void browserUrlChanged();
	void runningChanged();
	void succeededChanged();
	void failedChanged();
	void errorChanged();

  private slots:
	void onStatus(const QString& status);
	void onAuthorizeWithBrowser(const QUrl& url);
	void onSucceeded();
	void onFailed(const QString& reason);

  private:
	void setFailed(const QString& reason);

	shared_qobject_ptr<AccountList> m_accountList;
	MinecraftAccountPtr m_account;
	shared_qobject_ptr<AccountTask> m_task;

	QString m_status;
	QUrl m_browserUrl;
	bool m_running = false;
	bool m_succeeded = false;
	bool m_failed = false;
	QString m_error;
};
