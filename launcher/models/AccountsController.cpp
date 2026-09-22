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

#include "AccountsController.h"

#include <QDesktopServices>

#include "core/LauncherContext.h"
#include "minecraft/auth/AccountTask.h"
#include "tasks/Task.h"

AccountsController::AccountsController(shared_qobject_ptr<AccountList> accounts,
										QObject* parent)
	: QObject(parent), m_accounts(std::move(accounts))
{
	/* Either one moving the default account or the list itself (a
	 * refreshed account's profileName can change what defaultName()
	 * reports) is reason enough to re-read both properties - same
	 * reasoning as QmlShell's own accountChanged() bump. */
	connect(m_accounts.get(), &AccountList::listChanged, this,
			&AccountsController::defaultChanged);
	connect(m_accounts.get(), &AccountList::defaultAccountChanged, this,
			&AccountsController::defaultChanged);
}

QObject* AccountsController::accounts() const
{
	return m_accounts.get();
}

bool AccountsController::hasDefault() const
{
	return m_accounts->defaultAccount().get() != nullptr;
}

QString AccountsController::defaultName() const
{
	auto account = m_accounts->defaultAccount();
	return account ? account->profileName() : QString();
}

void AccountsController::setDefault(int row)
{
	if (row < 0 || row >= m_accounts->count()) {
		return;
	}
	m_accounts->setDefaultAccount(m_accounts->at(row));
}

void AccountsController::clearDefault()
{
	m_accounts->setDefaultAccount(nullptr);
}

void AccountsController::remove(int row)
{
	if (row < 0 || row >= m_accounts->count()) {
		return;
	}
	m_accounts->removeAccount(m_accounts->index(row));
}

void AccountsController::refresh(int row)
{
	if (row < 0 || row >= m_accounts->count()) {
		return;
	}
	auto account = m_accounts->at(row);
	if (!account) {
		return;
	}
	m_accounts->requestRefresh(account->internalId());
}

bool AccountsController::addOffline(const QString& username)
{
	// Same rule as AccountListPage::on_actionAddOffline_triggered(): an
	// offline account is only useful once there is a Microsoft account to
	// actually play under, since offline mode exists for testing/LAN play
	// alongside a real account rather than as a Minecraft account
	// replacement.
	bool hasMSA = false;
	for (int i = 0; i < m_accounts->count(); i++) {
		if (m_accounts->at(i)->isMSA()) {
			hasMSA = true;
			break;
		}
	}
	if (!hasMSA) {
		return false;
	}

	const QString trimmed = username.trimmed();
	if (trimmed.isEmpty()) {
		return false;
	}

	// AccountList::addAccount() already guards against this internally,
	// but checking here lets the caller distinguish "rejected" from
	// "added" instead of silently doing nothing.
	if (m_accounts->findOfflineAccountByUsername(trimmed) != -1) {
		return false;
	}

	m_accounts->addAccount(MinecraftAccount::createOffline(trimmed));
	return true;
}

QObject* AccountsController::loginMicrosoft()
{
	// Parented to this, not left parentless: QML calls this directly on
	// the accountsController property rather than through QmlShell, so
	// there is no expose() call sitting between here and QML to pin an
	// otherwise-parentless return value with CppOwnership. A real parent
	// sidesteps the question entirely - see ModrinthModpackModel::install(),
	// which parents its returned TaskWatcher to itself for the same reason.
	return new MicrosoftLoginController(m_accounts, this);
}

MicrosoftLoginController::MicrosoftLoginController(
	shared_qobject_ptr<AccountList> accounts, QObject* parent)
	: QObject(parent), m_accountList(std::move(accounts))
{
	if (LAUNCHER->msaClientId().isEmpty()) {
		// Mirrors AccountListPage's ui->actionAddMicrosoft->setVisible()
		// guard, which hid the button entirely rather than let it fail;
		// this controller has no visibility to hide, so it starts
		// already failed instead. (The widget page's separate osx64
		// warning dialog is not reproduced here: BUILD_PLATFORM is never
		// actually set to that value by this fork's build, so it never
		// fires today.)
		m_error = tr(
			"Microsoft login is not available: no client id is configured "
			"for this build.");
		m_failed = true;
		return;
	}

	m_running = true;
	m_status = tr("Opening your browser for Microsoft login...");

	m_account = MinecraftAccount::createBlankMSA();
	m_task = m_account->loginMSA();

	connect(m_task.get(), &Task::status, this,
			&MicrosoftLoginController::onStatus);
	connect(m_task.get(), &Task::succeeded, this,
			&MicrosoftLoginController::onSucceeded);
	connect(m_task.get(), &Task::failed, this,
			&MicrosoftLoginController::onFailed);
	connect(m_task.get(), &AccountTask::authorizeWithBrowser, this,
			&MicrosoftLoginController::onAuthorizeWithBrowser);

	m_task->start();
}

MicrosoftLoginController::~MicrosoftLoginController() {}

void MicrosoftLoginController::cancel()
{
	if (!m_running) {
		return;
	}
	// No supported way to actually abort an in-flight OAuth2 request (see
	// the header comment) - dropping our references is what the widget
	// dialog effectively did when closed mid-flow, since nothing else
	// keeps m_account/m_task alive once this does not.
	if (m_task) {
		m_task->disconnect(this);
	}
	m_task.reset();
	m_account.reset();

	m_running = false;
	emit runningChanged();
}

void MicrosoftLoginController::openBrowser()
{
	if (m_browserUrl.isEmpty()) {
		return;
	}
	QDesktopServices::openUrl(m_browserUrl);
}

void MicrosoftLoginController::onStatus(const QString& status)
{
	if (m_status == status) {
		return;
	}
	m_status = status;
	emit statusChanged();
}

void MicrosoftLoginController::onAuthorizeWithBrowser(const QUrl& url)
{
	m_browserUrl = url;
	emit browserUrlChanged();
}

void MicrosoftLoginController::onSucceeded()
{
	// Exactly what AccountListPage::on_actionAddMicrosoft_triggered() did
	// with the widget dialog's result: file the new account in, and make
	// it the default if it's the first one in the list.
	m_accountList->addAccount(m_account);
	if (m_accountList->count() == 1) {
		m_accountList->setDefaultAccount(m_account);
	}

	m_running = false;
	emit runningChanged();
	m_succeeded = true;
	emit succeededChanged();
}

void MicrosoftLoginController::onFailed(const QString& reason)
{
	setFailed(reason);
}

void MicrosoftLoginController::setFailed(const QString& reason)
{
	if (m_running) {
		m_running = false;
		emit runningChanged();
	}
	m_error = reason;
	emit errorChanged();
	m_failed = true;
	emit failedChanged();
}
