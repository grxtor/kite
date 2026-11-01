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

#include "AccountsController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

#include "FileSystem.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountTask.h"
#include "minecraft/services/CapeChange.h"
#include "minecraft/services/SkinDelete.h"
#include "minecraft/services/SkinUpload.h"
#include "minecraft/skins/DefaultSkins.h"
#include "minecraft/skins/SkinEntry.h"
#include "models/SkinsController.h"
#include "settings/SettingsObject.h"
#include "tasks/SequentialTask.h"
#include "tasks/Task.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* The SkinsDir setting, absolute (relative to the data folder, the working
	 * directory); "skins" where there are no settings. */
	QString skinsDirectory()
	{
		auto* context = LauncherContext::instance();
		const QString setting =
			context && context->settings()
				? context->settings()->get(QStringLiteral("SkinsDir")).toString()
				: QString();
		return QDir(setting.isEmpty() ? QStringLiteral("skins") : setting)
			.absolutePath();
	}

	/* Demo capes: textures from "capes/" in the skins folder. A missing file
	 * leaves the cape without a picture. */
	QVariantMap demoCape(const QString& id, const QString& alias)
	{
		const QString file =
			QDir(skinsDirectory()).filePath(QStringLiteral("capes/%1.png").arg(id));
		QVariantMap cape;
		cape[QStringLiteral("id")] = id;
		cape[QStringLiteral("alias")] = alias;
		cape[QStringLiteral("url")] = QFileInfo::exists(file)
										  ? QUrl::fromLocalFile(file).toString()
										  : QString();
		return cape;
	}
} // namespace

AccountsController::AccountsController(shared_qobject_ptr<AccountList> accounts,
										QObject* parent)
	: QObject(parent), m_accounts(std::move(accounts))
{
	/* Either the default account moving or the list itself changing re-reads
	 * the default. */
	connect(m_accounts.get(), &AccountList::listChanged, this,
			&AccountsController::defaultChanged);
	connect(m_accounts.get(), &AccountList::defaultAccountChanged, this,
			&AccountsController::defaultChanged);
	// A skinless account is drawn in the default skin once it arrives; the page refetches.
	connect(DefaultSkins::instance(), &DefaultSkins::loaded, this, [this] {
		++m_defaultSkinsRevision;
		emit defaultSkinsRevisionChanged();
	});
}

QObject* AccountsController::accounts() const
{
	return m_accounts.get();
}

QString AccountsController::defaultAccountId() const
{
	auto account = m_accounts->defaultAccount();
	if (!account) {
		return QString();
	}
	return account->accountId();
}

int AccountsController::defaultSkinsRevision() const
{
	return m_defaultSkinsRevision;
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
	// An offline account needs a Microsoft account that owns Minecraft first.
	if (!m_accounts->anyMicrosoftAccountOwnsMinecraft()) {
		return false;
	}

	const QString trimmed = username.trimmed();
	if (trimmed.isEmpty()) {
		return false;
	}

	// AccountList::addAccount() guards this too; checking here lets the caller tell "rejected" from "added".
	if (m_accounts->findOfflineAccountByUsername(trimmed) != -1) {
		return false;
	}

	m_accounts->addAccount(MinecraftAccount::createOffline(trimmed));
	return true;
}

QObject* AccountsController::loginMicrosoft()
{
	// Parented to this: QML calls this directly, with no expose() to pin a parentless return value.
	if (m_login) {
		m_login->cancel();
		m_login->deleteLater();
	}
	m_login = new MicrosoftLoginController(m_accounts, this);
	return m_login;
}

bool AccountsController::skinDemoRequested() const
{
	return qEnvironmentVariable("KITE_QML_ROUTE")
		.contains(QStringLiteral("accounts-demo"));
}

QString AccountsController::skinDemoPose() const
{
	const QStringList steps =
		qEnvironmentVariable("KITE_QML_ROUTE").split(QLatin1Char(';'));
	const QString key = QStringLiteral("accounts-demo=");
	for (const QString& step : steps) {
		if (step.trimmed().startsWith(key)) {
			return step.trimmed().mid(key.length());
		}
	}
	return QString();
}

QVariantMap AccountsController::accountSkinInfo(int row) const
{
	QVariantMap info;
	info[QStringLiteral("valid")] = false;
	info[QStringLiteral("slim")] = false;
	/* False for offline accounts and before the profile is fetched. */
	info[QStringLiteral("hasSkin")] = false;
	info[QStringLiteral("defaultSkin")] = false;
	info[QStringLiteral("currentCapeId")] = QString();
	info[QStringLiteral("currentCapeUrl")] = QString();
	info[QStringLiteral("capes")] = QVariantList();

	/* Row -1 is the qml-preview-tools demo sentinel: canned data, only when
	 * skinDemoRequested(). */
	if (row == -1 && skinDemoRequested()) {
		info[QStringLiteral("valid")] = true;
		const QVariantMap ember =
			demoCape(QStringLiteral("demo-ember"), QStringLiteral("Ember"));
		info[QStringLiteral("currentCapeId")] = ember.value(QStringLiteral("id"));
		info[QStringLiteral("currentCapeUrl")] = ember.value(QStringLiteral("url"));
		info[QStringLiteral("capes")] = QVariantList{
			ember, demoCape(QStringLiteral("demo-vault"), QStringLiteral("Vault"))
		};
		return info;
	}

	if (row < 0 || row >= m_accounts->count()) {
		return info;
	}
	auto account = m_accounts->at(row);
	if (!account || !account->accountData()) {
		return info;
	}

	/* Its own skin, or the game's default for it (as AccountFaceProvider draws). */
	const MinecraftProfile& profile = account->accountData()->minecraftProfile;
	const bool slim = DefaultSkins::isSlim(account);
	info[QStringLiteral("slim")] = slim;
	if (!profile.skin.data.isEmpty()) {
		info[QStringLiteral("hasSkin")] = true;
	} else {
		DefaultSkins* defaults = DefaultSkins::instance();
		defaults->ensureLoaded();
		info[QStringLiteral("hasSkin")] = !defaults->texture(slim).isNull();
		info[QStringLiteral("defaultSkin")] = true;
	}

	if (!account->isMSA()) {
		return info;
	}
	info[QStringLiteral("valid")] = true;
	info[QStringLiteral("currentCapeId")] = profile.currentCape;

	/* Once per id: older account files kept every refresh's capes, so one cape
	 * could be stored many times. */
	QVariantList capes;
	QSet<QString> seen;
	for (const Cape& cape : profile.capes) {
		if (seen.contains(cape.id)) {
			continue;
		}
		seen.insert(cape.id);
		QVariantMap c;
		c[QStringLiteral("id")] = cape.id;
		c[QStringLiteral("alias")] = cape.alias;
		c[QStringLiteral("url")] = cape.url;
		capes.append(c);
		if (cape.id == profile.currentCape) {
			info[QStringLiteral("currentCapeUrl")] = cape.url;
		}
	}
	info[QStringLiteral("capes")] = capes;
	return info;
}

QString AccountsController::validateSkinFile(const QString& path) const
{
	SkinEntry entry(FS::pathFromUrlOrPath(path));
	if (!entry.isUsable()) {
		return tr("Skin images must be 64x64 or 64x32 pixel PNG files.");
	}
	return QString();
}

QObject* AccountsController::changeSkin(int row, const QString& path, bool slim)
{
	if (row < 0 || row >= m_accounts->count()) {
		return nullptr;
	}
	auto account = m_accounts->at(row);
	if (!account || !account->isMSA()) {
		return nullptr;
	}

	QFile file(FS::pathFromUrlOrPath(path));
	if (!file.open(QIODevice::ReadOnly)) {
		return nullptr;
	}
	const QByteArray texture = file.readAll();
	file.close();

	auto* job = new SequentialTask(nullptr, tr("Change skin"));
	job->addTask(Task::Ptr(new SkinUpload(
		nullptr, account->accessToken(), texture,
		slim ? SkinUpload::ALEX : SkinUpload::STEVE)));
	job->addTask(account->refresh());

	auto* watcher = new TaskWatcher(Task::Ptr(job), this);
	watcher->setTitle(tr("Change skin"));
	job->start();
	return watcher;
}

QObject* AccountsController::resetSkin(int row)
{
	if (row < 0 || row >= m_accounts->count()) {
		return nullptr;
	}
	auto account = m_accounts->at(row);
	if (!account || !account->isMSA()) {
		return nullptr;
	}

	auto* job = new SequentialTask(nullptr, tr("Reset skin"));
	job->addTask(Task::Ptr(new SkinDelete(nullptr, account->accessToken())));
	job->addTask(account->refresh());

	auto* watcher = new TaskWatcher(Task::Ptr(job), this);
	watcher->setTitle(tr("Reset skin"));
	job->start();
	return watcher;
}

QObject* AccountsController::changeCape(int row, const QString& capeId)
{
	if (row < 0 || row >= m_accounts->count()) {
		return nullptr;
	}
	auto account = m_accounts->at(row);
	if (!account || !account->isMSA()) {
		return nullptr;
	}

	auto* job = new SequentialTask(nullptr, tr("Change cape"));
	job->addTask(
		Task::Ptr(new CapeChange(nullptr, account->accessToken(), capeId)));
	job->addTask(account->refresh());

	auto* watcher = new TaskWatcher(Task::Ptr(job), this);
	watcher->setTitle(tr("Change cape"));
	job->start();
	return watcher;
}

QObject* AccountsController::skins(int row)
{
	/* The qml-preview-tools demo row: the library alone, no account. */
	const bool demo = row == -1 && skinDemoRequested();
	if (!demo && (row < 0 || row >= m_accounts->count())) {
		return nullptr;
	}
	auto account = demo ? MinecraftAccountPtr() : m_accounts->at(row);
	if (!demo && !account) {
		return nullptr;
	}
	const QString accountId =
		account ? account->internalId() : QStringLiteral("demo");
	if (!m_skins || m_skinsAccountId != accountId) {
		if (m_skins) {
			m_skins->deleteLater();
		}
		m_skins = new SkinsController(account, skinsDirectory(), this);
		m_skinsAccountId = accountId;
	}
	/* Fetch the default skins not cached yet; tried again on every open. */
	DefaultSkins::instance()->fetchGallery();
	return m_skins;
}

MicrosoftLoginController::MicrosoftLoginController(
	shared_qobject_ptr<AccountList> accounts, QObject* parent)
	: QObject(parent), m_accountList(std::move(accounts))
{
	if (LAUNCHER->msaClientId().isEmpty()) {
		// Without a client id the sign-in cannot work; start already failed.
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
	// An in-flight OAuth2 request cannot be aborted; drop the references instead.
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
	// File the account in; the first one becomes the default.
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
