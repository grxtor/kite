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

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include "QObjectPtr.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"

class AccountTask;

/*
 * QML-facing replacement for the widget AccountListPage: everything that
 * page's toolbar actions did (add Microsoft/offline, remove, refresh, set/
 * clear default), plus the Microsoft sign-in flow itself, as one object a
 * QML Accounts page can bind against and call into.
 *
 * The account list itself is not wrapped -- `accounts` hands QML the
 * AccountList model directly, the same object LAUNCHER->accounts() returns,
 * so a QML ListView binds to it exactly like any other list model here (see
 * AccountList::roleNames()). Everything else here operates on it by row,
 * the same way the widget page operated on the view's current selection.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class AccountsController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QObject* accounts READ accounts CONSTANT)
	Q_PROPERTY(bool hasDefault READ hasDefault NOTIFY defaultChanged)
	/// profileName() of the default account, or empty if there is none.
	Q_PROPERTY(QString defaultName READ defaultName NOTIFY defaultChanged)

  public:
	explicit AccountsController(shared_qobject_ptr<AccountList> accounts,
								QObject* parent = nullptr);

	QObject* accounts() const;
	bool hasDefault() const;
	QString defaultName() const;

	/// Makes row @p row the default account. No-op if @p row is out of range.
	Q_INVOKABLE void setDefault(int row);
	/// Clears the default account, if any.
	Q_INVOKABLE void clearDefault();
	/// Removes row @p row. No-op if @p row is out of range.
	Q_INVOKABLE void remove(int row);
	/// Requests a token refresh for row @p row, ahead of the background
	/// refresh queue -- same as AccountList::requestRefresh(). No-op if
	/// @p row is out of range.
	Q_INVOKABLE void refresh(int row);

	/*!
	 * Adds an offline account named @p username, applying the same rules
	 * the old offline dialog enforced:
	 *  - at least one Microsoft account must already be in the list;
	 *  - the trimmed username must not be empty;
	 *  - it must not collide (case-insensitively) with an existing offline
	 *    account's username.
	 * Returns false, without adding anything, if any rule is violated.
	 */
	Q_INVOKABLE bool addOffline(const QString& username);

	/*!
	 * Starts an interactive Microsoft sign-in and returns a controller for
	 * it, owned by C++ (QQmlEngine::CppOwnership -- see QmlShell::expose()):
	 * the login must outlive the page that started it if the user navigates
	 * away mid-flow. On success the new account is added to the list, and
	 * made the default if it was the first account -- exactly what the
	 * widget dialog's caller did.
	 */
	Q_INVOKABLE QObject* loginMicrosoft();

  signals:
	/// hasDefault()/defaultName() moved.
	void defaultChanged();

  private:
	shared_qobject_ptr<AccountList> m_accounts;
};

/*
 * One Microsoft sign-in attempt, as started by AccountsController::
 * loginMicrosoft().
 *
 * MeshMC's Microsoft login (MSAStep, launcher/minecraft/auth/steps/
 * MSAStep.cpp) is an OAuth2 authorization-code flow, not a device code
 * flow: QOAuth2AuthorizationCodeFlow opens the system browser to a
 * microsoftonline.com login page and a local HTTP server (bound to
 * localhost, chosen by Qt) catches the redirect. There is no user code to
 * display -- only the URL the browser was (or should have been) sent to,
 * which the browser is opened for automatically as soon as it is known;
 * browserUrl and openBrowser() exist for a user who closed that tab and
 * wants it back.
 *
 * running/succeeded/failed/error mirror TaskWatcher's properties (tasks/
 * TaskWatcher.h) but this is not a TaskWatcher: TaskWatcher only watches an
 * already-meaningful task, whereas this also owns the blank account the
 * flow fills in and, on success, files it into the AccountList itself --
 * concerns TaskWatcher deliberately knows nothing about.
 */
class MicrosoftLoginController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString status READ status NOTIFY statusChanged)
	/// The URL the browser was sent to, once known; empty until then.
	Q_PROPERTY(QUrl browserUrl READ browserUrl NOTIFY browserUrlChanged)
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	Q_PROPERTY(bool succeeded READ succeeded NOTIFY succeededChanged)
	Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
	/// The failure reason, if any. Empty while running or on success.
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

	/*!
	 * Gives up on this attempt. There is no server-side cancellation for
	 * an in-flight OAuth2 request (AccountTask/Task never override
	 * canAbort()/abort() to make one possible -- the widget dialog had the
	 * same limitation, and simply let a closed dialog's task run to
	 * completion unobserved); this instead drops this controller's only
	 * references to the pending account and task, matching what happened
	 * when that dialog was destroyed. No-op once already finished.
	 */
	Q_INVOKABLE void cancel();
	/// Re-opens the system browser at browserUrl(). No-op before it is known.
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
