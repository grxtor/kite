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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

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
	/* A QML FileDialog hands out a "file://" url; validateSkinFile()/
	 * changeSkin() also accept a plain path, so either can be passed
	 * straight through from QML without the caller converting it first.
	 * Same conversion InstanceDetails::install() uses for the same reason. */
	QString toLocalPath(const QString& fileUrlOrPath)
	{
		const QUrl url(fileUrlOrPath);
		return url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
	}

	/* The launcher's skins folder (the SkinsDir setting), absolute.
	 * Relative to the data folder, which is the working directory, like
	 * every other folder setting; "skins" where there are no settings. */
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

	/* The demo's capes: textures from "capes/" in the skins folder, where
	 * the widget skin dialog kept its cape cache (qml-preview-tools puts
	 * two there). A missing file leaves the cape without a picture, as a
	 * real cape whose texture has not loaded. */
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
	/* Either one moving the default account or the list itself (a
	 * refreshed account's profileName can change what defaultName()
	 * reports) is reason enough to re-read both properties - same
	 * reasoning as QmlShell's own accountChanged() bump. */
	connect(m_accounts.get(), &AccountList::listChanged, this,
			&AccountsController::defaultChanged);
	connect(m_accounts.get(), &AccountList::defaultAccountChanged, this,
			&AccountsController::defaultChanged);
	/* A skinless account is drawn in the game's default skin once that
	 * arrives (see accountSkinInfo()); the page refetches on this. */
	connect(DefaultSkins::instance(), &DefaultSkins::loaded, this, [this] {
		++m_defaultSkinsRevision;
		emit defaultSkinsRevisionChanged();
	});
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

QString AccountsController::defaultAccountId() const
{
	/* The same value AccountList's AccountIdRole hands each row. */
	auto account = m_accounts->defaultAccount();
	if (!account) {
		return QString();
	}
	return account->profileId().isEmpty() ? account->internalId()
										  : account->profileId();
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

bool AccountsController::skinDemoRequested() const
{
	return qEnvironmentVariable("MESHMC_QML_ROUTE")
		.contains(QStringLiteral("accounts-demo"));
}

QString AccountsController::skinDemoPose() const
{
	const QStringList steps =
		qEnvironmentVariable("MESHMC_QML_ROUTE").split(QLatin1Char(';'));
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
	/* Whether there is a skin to draw at all -- offline accounts, and any
	 * account before its profile has been fetched, have none, and a
	 * preview then shows the neutral figure instead. */
	info[QStringLiteral("hasSkin")] = false;
	info[QStringLiteral("defaultSkin")] = false;
	info[QStringLiteral("currentCapeId")] = QString();
	info[QStringLiteral("currentCapeUrl")] = QString();
	info[QStringLiteral("capes")] = QVariantList();

	/* Row -1 is the qml-preview-tools demo sentinel (see
	 * skinDemoRequested()/AccountsPage.qml): the preview's account file
	 * only ever has offline accounts, so no real row's isMSA role would
	 * let the page reach this editor otherwise. Canned data, not a faked
	 * account file, and only handed back when actually asked for -- a
	 * plain out-of-range -1 (what every other row here already gets)
	 * still reads as invalid the rest of the time. */
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

	/* What the player looks like, for every account: its own skin, or the
	 * game's default for it -- the same choice AccountFaceProvider makes
	 * when it draws the skin. */
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

	/* Once per id: account files saved before Parsers::
	 * parseMinecraftProfile() cleared its list kept every refresh's capes,
	 * so one cape could be stored many times over. */
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
	SkinEntry entry(toLocalPath(path));
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

	QFile file(toLocalPath(path));
	if (!file.open(QIODevice::ReadOnly)) {
		return nullptr;
	}
	const QByteArray texture = file.readAll();
	file.close();

	/* Same SkinUpload + refresh sequence SkinManageDialog::accept() runs,
	 * minus the cape change: this uploads a picked file directly rather
	 * than editing a local skin-library entry that already carries a cape
	 * choice, so there is nothing here to carry over. */
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
	/* The qml-preview-tools demo row (see skinDemoRequested()): the
	 * library on its own, with no account to say which skin is worn. */
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
	/* The game's default skins, for the editor's own section: whichever
	 * are not in the cache yet, tried again on every open (say, once the
	 * network is back). */
	DefaultSkins::instance()->fetchGallery();
	return m_skins;
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
