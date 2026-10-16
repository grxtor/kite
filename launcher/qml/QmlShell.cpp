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

#include "qml/QmlShell.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QFileInfo>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#include "BaseInstance.h"
#include "minecraft/MinecraftInstance.h"
#include "FileSystem.h"
#include "InstanceCopyTask.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "java/JavaInstallList.h"
#include "models/AboutInfo.h"
#include "models/AccountsController.h"
#include "models/NewsModel.h"
#include "models/IdSelectionModel.h"
#include "models/InstanceDetails.h"
#include "models/InstanceExportController.h"
#include "models/InstanceShortcutController.h"
#include "models/JavaController.h"
#include "models/JavaDownloadController.h"
#include "models/FolderSettingsController.h"
#include "models/UpdateSettingsController.h"
#include "models/PlayTimeSummary.h"
#include "models/LibraryController.h"
#include "models/LauncherLogs.h"
#include "BuildConfig.h"
#include "models/InstanceFilterModel.h"
#include "models/NewInstanceController.h"
#include "models/RecentWorldsModel.h"
#include "models/SettingsAdapter.h"
#include "modplatform/modrinth/ModrinthModpackModel.h"
#include "modplatform/flame/FlameModpackModel.h"
#include "modplatform/atlauncher/AtlModpackModel.h"
#include "modplatform/modpacksch/FtbModpackModel.h"
#include "modplatform/technic/TechnicModpackModel.h"
#include "models/AtlOptionalModsModel.h"
#include "core/UiHost.h"
#include "tasks/Task.h"
#include "tasks/TaskWatcher.h"
#include "translations/TranslationsModel.h"
#include "Sys.h"
#include "DesktopServices.h"
#include "settings/SettingsObject.h"
#include <QDir>
#include "qml/AccountFaceProvider.h"
#include "qml/InstanceIconProvider.h"
#include "qml/ModIconProvider.h"
#include "qml/QmlUiHost.h"
#include "qml/ScreenshotThumbnailProvider.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/mod/ModIconCache.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace
{
	/* Spelled out rather than loadFromModule(), which is Qt 6.5+; the floor is
	 * 6.4. The module sets RESOURCE_PREFIX "/qt/qml" so this path is stable. */
	const QUrl kRootUrl(QStringLiteral("qrc:/qt/qml/MeshMC/Main.qml"));

	/* Function-local static rather than a plain namespace-scope QmlShell
	 * member: this is a process-wide seam Application installs once, not
	 * per-instance state, and a Meyer's singleton sidesteps static
	 * initialisation order between translation units. */
	QmlShell::PluginSurfaceFactory& pluginSurfaceFactory()
	{
		static QmlShell::PluginSurfaceFactory factory;
		return factory;
	}
} // namespace

QString sanitizedInstanceName(const QString& name)
{
	QString sanitized = name;
	// Same as NoReturnTextEdit's commit path (InstanceDelegate.cpp): a
	// pasted multi-line name becomes one line rather than being rejected.
	sanitized.replace(QLatin1Char('\n'), QLatin1Char(' '));
	return sanitized.trimmed();
}

bool languageSetupStepNeeded(const QString& language)
{
	return language.isEmpty();
}

bool javaSetupStepNeeded(bool hostnameChanged, bool javaPathResolves)
{
	return hostnameChanged || !javaPathResolves;
}

QmlShell::QmlShell(QObject* parent) : QObject(parent)
{
	/* Sidebar account summary. Both signals exist on AccountList already;
	 * either one moving the default account or the list itself is reason
	 * enough to re-read all three properties, so both are wired to the
	 * same accountChanged() rather than tracked separately. */
	auto accounts = LAUNCHER->accounts();
	/* Any change may be a new skin under the same account id, and QML
	 * caches images by url: the revision in the face url makes it refetch. */
	const auto bump = [this] {
		++m_accountRevision;
		emit accountChanged();
	};
	connect(accounts.get(), &AccountList::listChanged, this, bump);
	connect(accounts.get(), &AccountList::defaultAccountChanged, this, bump);

	// Once per run, like Application::createSetupWizard() used to compute
	// this once before deciding whether to show the widget wizard.
	recomputeSetupSteps();
}

QmlShell::~QmlShell() = default;

QObject* QmlShell::expose(QObject* object)
{
	if (object) {
		QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
	}
	return object;
}

void QmlShell::setPluginSurfaceFactory(PluginSurfaceFactory factory)
{
	pluginSurfaceFactory() = std::move(factory);
}

QObject* QmlShell::pluginSurfaces(int anchor, const QString& anchorContext)
{
	const auto key = std::make_pair(anchor, anchorContext);
	auto it = m_pluginSurfaceModels.find(key);
	if (it != m_pluginSurfaceModels.end()) {
		return expose(it->second.get());
	}

	auto& factory = pluginSurfaceFactory();
	if (!factory) {
		return nullptr;
	}
	QObject* model = factory(anchor, anchorContext);
	if (!model) {
		return nullptr;
	}
	m_pluginSurfaceModels.emplace(key, std::unique_ptr<QObject>(model));
	return expose(model);
}

QString QmlShell::accountName() const
{
	auto account = LAUNCHER->accounts()->defaultAccount();
	return account ? account->profileName() : QString();
}

QString QmlShell::accountFace() const
{
	auto account = LAUNCHER->accounts()->defaultAccount();
	if (!account) {
		return QString();
	}
	// Offline accounts have no profile id; the provider accepts either.
	const QString id = account->profileId().isEmpty() ? account->internalId()
													   : account->profileId();
	return QStringLiteral("image://accountface/%1?rev=%2")
		.arg(id)
		.arg(m_accountRevision);
}

QString QmlShell::accountKind() const
{
	auto account = LAUNCHER->accounts()->defaultAccount();
	if (!account) {
		return QString();
	}
	return account->isMSA() ? QStringLiteral("Microsoft")
							: QStringLiteral("Offline");
}

int QmlShell::accountCount() const
{
	return LAUNCHER->accounts()->count();
}

void QmlShell::launchInstance(const QString& id)
{
	emit launchRequested(id);
}

void QmlShell::launchInstanceMode(const QString& id, const QString& mode)
{
	emit launchModeRequested(id, mode);
}

void QmlShell::launchInstanceAs(const QString& id, const QString& accountId)
{
	emit launchAsRequested(id, accountId);
}

void QmlShell::setProfilersProvider(ProfilersProvider provider)
{
	m_profilersProvider = std::move(provider);
}

void QmlShell::setPluginListFactory(PluginListFactory factory)
{
	m_pluginListFactory = std::move(factory);
}

QObject* QmlShell::pluginList()
{
	if (!m_pluginList && m_pluginListFactory) {
		m_pluginList.reset(m_pluginListFactory());
	}
	return expose(m_pluginList.get());
}

QVariantMap QmlShell::launchOptions(const QString& id) const
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return {};
	}

	// Same check as the widget launch menu's Launch Demo entry.
	auto minecraft = std::dynamic_pointer_cast<MinecraftInstance>(instance);
	const bool supportsDemo = minecraft && minecraft->supportsDemo();

	QVariantList accounts;
	auto accountList = LAUNCHER->accounts();
	const auto defaultAccount = accountList->defaultAccount();
	for (int i = 0; i < accountList->count(); ++i) {
		const auto account = accountList->at(i);
		if (!account) {
			continue;
		}
		accounts.append(QVariantMap{
			// Same id accountFace() uses: offline accounts have no
			// profile id.
			{ QStringLiteral("id"), account->profileId().isEmpty()
										? account->internalId()
										: account->profileId() },
			{ QStringLiteral("name"), account->profileName() },
			{ QStringLiteral("kind"), account->isMSA()
										  ? QStringLiteral("Microsoft")
										  : QStringLiteral("Offline") },
			{ QStringLiteral("isDefault"), account == defaultAccount },
		});
	}

	return QVariantMap{
		{ QStringLiteral("supportsDemo"), supportsDemo },
		{ QStringLiteral("profiler"), instance->profilerKey() },
		{ QStringLiteral("profilers"), m_profilersProvider
											? m_profilersProvider()
											: QVariantList() },
		{ QStringLiteral("accounts"), accounts },
	};
}

void QmlShell::setInstanceProfiler(const QString& id,
								   const QString& profilerKey)
{
	if (auto instance = LAUNCHER->instances()->getInstanceById(id)) {
		instance->setProfilerKey(profilerKey);
	}
}

void QmlShell::killInstance(const QString& id)
{
	emit killRequested(id);
}

void QmlShell::openInstanceFolder(const QString& id)
{
	emit folderRequested(id);
}

void QmlShell::joinServer(const QString& id, const QString& address)
{
	emit joinServerRequested(id, address);
}

void QmlShell::openPath(const QString& path)
{
	DesktopServices::openDirectory(QDir(path).absolutePath(), true);
}

void QmlShell::showInstanceLogRequested(const QString& id)
{
	emit openInstanceLog(id);
}

void QmlShell::requestImport(const QString& source)
{
	if (source.isEmpty()) {
		return;
	}
	if (!m_window) {
		m_pendingImports.append(source);
		return;
	}
	emit importRequested(source);
}

void QmlShell::requestNavigate(const QString& target)
{
	emit navigateRequested(target);
}

bool QmlShell::renameInstance(const QString& id, const QString& name)
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return false;
	}

	const QString sanitized = sanitizedInstanceName(name);
	if (sanitized.isEmpty()) {
		return false;
	}

	// FIXME: if no change, do not set. setting involves saving a file.
	// (Same shortcut InstanceList::setData() takes; BaseInstance::setName()
	// does not bother checking on its own.)
	if (instance->name() != sanitized) {
		instance->setName(sanitized);
	}
	return true;
}

void QmlShell::setInstanceGroup(const QString& id, const QString& group)
{
	LAUNCHER->instances()->setInstanceGroup(id, group);
}

void QmlShell::setInstanceIcon(const QString& id, const QString& iconKey)
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return;
	}
	instance->setIconKey(iconKey);
}

void QmlShell::debugMarkInstanceCrashed(const QString& id)
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return;
	}
	instance->setCrashed(true);
}

void QmlShell::debugShowRequest(const QString& kind)
{
	if (!m_uiHost) {
		return;
	}
	if (kind == QLatin1String("pickAccount")) {
		// Queued: pickAccount() blocks until answered, and the QML call
		// that got here should return first.
		QTimer::singleShot(0, this, [this]() {
			if (!m_uiHost) {
				return;
			}
			QList<MinecraftAccountPtr> list;
			auto accounts = LAUNCHER->accounts();
			for (int i = 0; i < accounts->count(); ++i) {
				list.append(accounts->at(i));
			}
			m_uiHost->pickAccount(
				tr("Choose an account"),
				tr("No account is set as the default. Pick the one to play "
				   "with."),
				list);
		});
	} else if (kind == QLatin1String("modal")) {
		// A plugin's one-off prompt, as a typical plugin builds it.
		QTimer::singleShot(0, this, [this]() {
			if (!m_uiHost) {
				return;
			}
			const QJsonObject document{
				{ "type", "column" },
				{ "children",
				  QJsonArray{
					  QJsonObject{ { "type", "text" },
								   { "props",
									 QJsonObject{ { "text",
													"Share this world with your friends' server?" } } } },
					  QJsonObject{ { "type", "text_field" },
								   { "id", "address" },
								   { "props", QJsonObject{ { "label", "Server address" },
														   { "value", "play.example.net" } } } },
					  QJsonObject{ { "type", "toggle" },
								   { "id", "remember" },
								   { "props", QJsonObject{ { "label", "Remember this server" } } } },
					  QJsonObject{ { "type", "row" },
								   { "children",
									 QJsonArray{ QJsonObject{ { "type", "button" },
															  { "id", "share" },
															  { "props", QJsonObject{ { "label", "Share" } } } } } } },
				  } },
			};
			m_uiHost->runPluginModal(QStringLiteral("World Share"), document);
		});
	} else if (kind == QLatin1String("optionalMods")) {
		// An ATLauncher pack's optional mods, as AtlModpackModel asks.
		QTimer::singleShot(0, this, [this]() {
			if (!m_uiHost) {
				return;
			}
			auto mod = [](const QString& name, const QString& description,
						  bool selected) {
				ATLauncher::VersionMod m;
				m.name = name;
				m.description = description;
				m.optional = true;
				m.selected = selected;
				m.recommended = selected;
				m.hidden = false;
				m.library = false;
				m.effectively_hidden = false;
				m.client = true;
				return m;
			};
			AtlOptionalModsModel mods(
				{ mod(QStringLiteral("Optifine"), QStringLiteral("Better frame rates and shader support."), true),
				  mod(QStringLiteral("JourneyMap"), QStringLiteral("A minimap and a full world map."), true),
				  mod(QStringLiteral("Inventory Tweaks"), QStringLiteral("Sorts your inventory with one click."), false) });
			m_uiHost->chooseOptionalMods(QStringLiteral("Sky Factory"), mods);
		});
	} else if (kind == QLatin1String("busySkip")) {
		/* Kept alive by its own skip callback, which ends it; QmlUiHost
		 * drops the callback (and so the indicator) at the latest when it
		 * is destroyed itself. */
		auto holder =
			std::make_shared<std::unique_ptr<UiHost::BusyIndicator>>();
		*holder = m_uiHost->showBusy(tr("Refreshing account…"),
									 tr("Play offline"),
									 [holder]() { holder->reset(); });
	}
}

bool QmlShell::importIcon(const QString& fileUrlOrPath)
{
	const QUrl url(fileUrlOrPath);
	const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;

	const QFileInfo info(path);
	if (!info.isReadable() || !info.isFile()) {
		return false;
	}

	auto icons = LAUNCHER->icons();
	icons->installIcons({ path });

	/* Same key IconList itself derives for a dropped/installed file (see
	 * IconList::directoryChanged()) - deterministic from the file name, so
	 * this does not need to wait for the watcher to actually pick the copy
	 * up before telling the caller what to expect. installIcons() silently
	 * no-ops on a rejected extension or a same-key collision, same as
	 * IconPickerDialog's own "Add Icon" button, so this is optimistic
	 * rather than a confirmation the icon list now has it. */
	emit iconImported(info.baseName());
	return true;
}

QObject* QmlShell::duplicateInstance(const QString& id, const QString& newName,
									 const QString& group)
{
	auto original = LAUNCHER->instances()->getInstanceById(id);
	if (!original) {
		return nullptr;
	}

	const QString name = sanitizedInstanceName(newName);
	if (name.isEmpty()) {
		return nullptr;
	}

	// Same defaults CopyInstanceDialog's checkboxes start with, and the
	// same icon it starts the icon button showing (the original's).
	auto* copyTask = new InstanceCopyTask(original, /* copySaves */ true,
										  /* keepPlaytime */ true);
	copyTask->setName(name);
	copyTask->setGroup(group);
	copyTask->setIcon(original->iconKey());

	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(copyTask);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(name);
	wrapped->start();
	return expose(watcher);
}

bool QmlShell::deleteInstance(const QString& id)
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return false;
	}
	// Trashing a folder Java still has open fails on Windows, and the
	// fallback below would then start deleting a running game's files one
	// by one -- same guard MainWindow::on_actionDeleteInstance_triggered()
	// has before it ever gets to the confirmation dialog.
	if (instance->isRunning()) {
		return false;
	}

	auto instances = LAUNCHER->instances();
	if (!instances->trashInstance(id)) {
		instances->deleteInstance(id);
	}

	/* The widget grid persists its selection across restarts under this
	 * key; clearing it here too means it never restarts pointed at an
	 * instance that no longer exists, whichever UI is in use next time. */
	LAUNCHER->settings()->set("SelectedInstance", QString());
	return true;
}

bool QmlShell::isInstanceRunning(const QString& id) const
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	return instance && instance->isRunning();
}

QStringList QmlShell::groups() const
{
	return m_instances ? m_instances->groups() : QStringList();
}

QObject* QmlShell::iconsModel() const
{
	return expose(LAUNCHER->icons().get());
}

QString QmlShell::iconsDir() const
{
	return LAUNCHER->icons()->getDirectory();
}

QStringList QmlShell::setupSteps() const
{
	return m_setupSteps;
}

void QmlShell::recomputeSetupSteps()
{
	auto settings = LAUNCHER->settings();

	// Same hostname check as Application::createSetupWizard(): a machine
	// change may mean the recorded JavaPath no longer applies, so this is
	// re-armed by recording the new hostname once it is seen.
	const QString currentHostName = QHostInfo::localHostName();
	const QString oldHostName = settings->get("LastHostname").toString();
	const bool hostnameChanged = currentHostName != oldHostName;
	if (hostnameChanged) {
		settings->set("LastHostname", currentHostName);
	}
	const QString javaPath = settings->get("JavaPath").toString();
	const bool javaPathResolves = !FS::ResolveExecutable(javaPath).isNull();

	QStringList steps;
	if (languageSetupStepNeeded(settings->get("Language").toString())) {
		steps << QStringLiteral("language");
	}
	if (javaSetupStepNeeded(hostnameChanged, javaPathResolves)) {
		steps << QStringLiteral("java");
	}

	if (steps == m_setupSteps) {
		return;
	}
	m_setupSteps = steps;
	qDebug() << "QML shell: setup steps needed:" << m_setupSteps;
	emit setupStepsChanged();
}

void QmlShell::finishSetupStep(const QString& id)
{
	Q_UNUSED(id);
	recomputeSetupSteps();
}

QObject* QmlShell::languages() const
{
	return expose(LAUNCHER->translations().get());
}

void QmlShell::selectLanguage(const QString& key)
{
	auto translations = LAUNCHER->translations();
	translations->selectLanguage(key);
	translations->updateLanguage(key);
	// selectedLanguage() rather than echoing back @p key: selectLanguage()
	// falls back to the default language for an unrecognised key, and this
	// should persist whatever it actually settled on, the way
	// LanguageWizardPage::validatePage() persists the tree view's current
	// selection rather than trusting an arbitrary string.
	LAUNCHER->settings()->set("Language", translations->selectedLanguage());
	if (m_engine) {
		m_engine->retranslate();
	}
}

QObject* QmlShell::javaInstalls() const
{
	return expose(LAUNCHER->javalist().get());
}

bool QmlShell::javaDetecting() const
{
	return m_javaDetecting;
}

void QmlShell::detectJava()
{
	auto task = LAUNCHER->javalist()->getLoadTask();
	if (!task) {
		return;
	}
	if (!m_javaDetecting) {
		m_javaDetecting = true;
		emit javaDetectingChanged();
	}
	connect(task.get(), &Task::finished, this, [this]() {
		m_javaDetecting = false;
		emit javaDetectingChanged();
	});
	if (!task->isRunning()) {
		task->start();
	}
}

void QmlShell::useJava(const QString& path)
{
	LAUNCHER->settings()->set("JavaPath", path);
}

QObject* QmlShell::about() const
{
	return expose(m_about.get());
}

QObject* QmlShell::news() const
{
	return expose(m_news.get());
}

bool QmlShell::updaterAvailable() const
{
	return m_updaterAvailable;
}

void QmlShell::setUpdaterAvailable(bool available)
{
	if (m_updaterAvailable == available) {
		return;
	}
	m_updaterAvailable = available;
	emit updaterAvailableChanged();
}

void QmlShell::checkForUpdates()
{
	emit checkForUpdatesRequested();
}

QObject* QmlShell::settings() const
{
	return expose(m_settings.get());
}

void QmlShell::applyProxySettings()
{
	// Same five settings, read the same way, as
	// Application::initSubsystems()'s own proxy setup and the widget
	// ProxyPage::applySettings() -- only the destination differs
	// (LauncherContext rather than calling updateProxySettings() directly,
	// since QmlShell cannot see Application from MeshMC_qml).
	auto settings = LAUNCHER->settings();
	const QString proxyTypeStr = settings->get("ProxyType").toString();
	const QString addr = settings->get("ProxyAddr").toString();
	const int port = settings->get("ProxyPort").value<qint16>();
	const QString user = settings->get("ProxyUser").toString();
	const QString pass = settings->get("ProxyPass").toString();
	LAUNCHER->updateProxySettings(proxyTypeStr, addr, port, user, pass);
}

QObject* QmlShell::uiHost() const
{
	return expose(m_uiHost.get());
}

UiHost* QmlShell::uiHostInterface() const
{
	// Not ready until some QML item has called setPresenterReady(true) --
	// see QmlUiHost's class comment. Application::uiHost() falls back to
	// the widget host while this is null, so a call reached before then
	// (an automatic startup update check finding no updater binary, say)
	// gets a real dialog instead of hanging on a request nothing shows.
	if (m_uiHost && m_uiHost->presenterReady()) {
		return m_uiHost.get();
	}
	return nullptr;
}

int QmlShell::systemMemoryMiB() const
{
	return static_cast<int>(Sys::getSystemRam() / Sys::mebibyte);
}

QObject* QmlShell::modpackModel() const
{
	return expose(m_modpacks.get());
}

QObject* QmlShell::curseForgeModpackModel() const
{
	return expose(m_curseForgeModpacks.get());
}

QObject* QmlShell::ftbModpackModel() const
{
	return expose(m_ftbModpacks.get());
}

QObject* QmlShell::atlModpackModel() const
{
	return expose(m_atlModpacks.get());
}

QObject* QmlShell::technicModpackModel() const
{
	return expose(m_technicModpacks.get());
}

QObject* QmlShell::installModpack(const QString& projectId,
								  const QString& versionId,
								  const QString& instanceName,
								  const QString& group)
{
	if (!m_modpacks) {
		return nullptr;
	}
	return expose(
		m_modpacks->install(projectId, versionId, instanceName, group));
}

QObject* QmlShell::recentModel() const
{
	return expose(m_recent.get());
}

QObject* QmlShell::heroModel() const
{
	return expose(m_hero.get());
}

QObject* QmlShell::instancePageModel() const
{
	return expose(m_instancePage.get());
}

QObject* QmlShell::recentWorlds() const
{
	return expose(m_recentWorlds.get());
}

QObject* QmlShell::accountsController() const
{
	return expose(m_accountsController.get());
}

QObject* QmlShell::newInstance() const
{
	/* Made on first use, not in load(): the controller starts loading the
	 * Minecraft version list, and opening the launcher should not fetch
	 * metadata nobody asked for. */
	if (!m_newInstance && m_engine) {
		m_newInstance = std::make_unique<NewInstanceController>();
	}
	// The version list proxies it hands out need the same CppOwnership
	// pinning as the controller itself, or the engine will try to delete
	// them out from under it the first time QML touches one.
	if (m_newInstance) {
		expose(m_newInstance->minecraftVersions());
		expose(m_newInstance->loaderVersions());
	}
	return expose(m_newInstance.get());
}

QObject* QmlShell::javaController() const
{
	if (!m_javaController) {
		m_javaController = std::make_unique<JavaController>();
	}
	return expose(m_javaController.get());
}

QObject* QmlShell::javaDownload() const
{
	if (!m_javaDownload) {
		m_javaDownload = std::make_unique<JavaDownloadController>();
	}
	return expose(m_javaDownload.get());
}

QObject* QmlShell::folderSettings() const
{
	if (!m_folderSettings) {
		m_folderSettings = std::make_unique<FolderSettingsController>();
	}
	return expose(m_folderSettings.get());
}

UpdateSettingsController* QmlShell::updateSettingsController() const
{
	if (!m_updateSettings) {
		m_updateSettings = std::make_unique<UpdateSettingsController>();
	}
	return m_updateSettings.get();
}

QObject* QmlShell::updateSettings() const
{
	return expose(updateSettingsController());
}

QObject* QmlShell::playTime() const
{
	/* First read once the root window loads, by which time the instance
	 * list exists; PlayTimeSummary recounts whenever it changes. */
	if (!m_playTime) {
		m_playTime = std::make_unique<PlayTimeSummary>();
	}
	return expose(m_playTime.get());
}

QObject* QmlShell::library() const
{
	if (!m_library && m_engine) {
		m_library = std::make_unique<LibraryController>(LAUNCHER->instances(),
														LAUNCHER->icons());
	}
	if (m_library) {
		// The icons model the duplicate dialog's picker reads: CppOwnership,
		// like iconsModel().
		expose(m_library->iconsModel());
	}
	return expose(m_library.get());
}

QObject* QmlShell::launcherLogs() const
{
	/* The working directory is the data folder, where the launcher writes
	 * <MESHMC_NAME>-<n>.log -- the same folder the widget logs dialog
	 * lists. */
	if (!m_launcherLogs) {
		m_launcherLogs = std::make_unique<LauncherLogs>(
			QDir::currentPath(), BuildConfig.MESHMC_NAME);
	}
	return expose(m_launcherLogs.get());
}

QObject* QmlShell::exportController(const QString& id)
{
	if (!m_exportController || m_exportController->instanceId() != id) {
		auto instance = LAUNCHER->instances()->getInstanceById(id);
		if (!instance) {
			return nullptr;
		}
		// Replacing one whose export still runs is safe: its destructor
		// aborts the task and keeps it alive until it has wound down.
		m_exportController = std::make_unique<InstanceExportController>(instance);
		expose(m_exportController->files());
	}
	return expose(m_exportController.get());
}

QObject* QmlShell::shortcutController(const QString& id)
{
	if (!m_shortcutController || m_shortcutController->instanceId() != id) {
		auto instance = LAUNCHER->instances()->getInstanceById(id);
		if (!instance) {
			return nullptr;
		}
		m_shortcutController =
			std::make_unique<InstanceShortcutController>(instance);
	}
	return expose(m_shortcutController.get());
}

QObject* QmlShell::sectionModel(const QString& group)
{
	if (!m_instances) {
		return nullptr;
	}
	auto& section = m_sections[group];
	if (!section) {
		section = std::make_unique<InstanceFilterModel>();
		section->setExactGroup(true);
		section->setGroup(group);
		section->setSourceModel(m_instances.get());
	}
	return expose(section.get());
}

QObject* QmlShell::instanceDetails(const QString& id)
{
	if (m_instanceDetails && m_instanceDetails->instanceId() == id) {
		return expose(m_instanceDetails.get());
	}

	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return nullptr;
	}

	// Replaces (and destroys, via unique_ptr assignment) whichever detail
	// page was open before - only one is kept at a time.
	m_instanceDetails = std::make_unique<InstanceDetails>(instance);

	// Every QObject* the bridge hands to QML needs the same CppOwnership
	// pinning as everything else exposed here, or the engine will try to
	// delete a model the instance still owns.
	expose(m_instanceDetails->settings());
	expose(m_instanceDetails->mods());
	expose(m_instanceDetails->resourcePacks());
	expose(m_instanceDetails->shaderPacks());
	expose(m_instanceDetails->texturePacks());
	expose(m_instanceDetails->worlds());
	expose(m_instanceDetails->log());
	expose(m_instanceDetails->otherLogs());
	expose(m_instanceDetails->components());
	expose(m_instanceDetails->screenshots());
	expose(m_instanceDetails->gameOptions());
	// contentBrowser()'s own `results` is reachable straight off the
	// pinned browser below without a separate expose() here: every
	// ContentProviderModel it hands out is parented to it (see
	// ContentBrowser::ensureModel()), and a parented QObject already keeps
	// its C++ ownership once QML touches it, pin or no pin.
	expose(m_instanceDetails->contentBrowser());
	// Same reasoning for loaderInstaller()'s own `versions` proxy - see
	// LoaderInstaller's constructor comment.
	expose(m_instanceDetails->loaderInstaller());

	return expose(m_instanceDetails.get());
}

QVariantMap QmlShell::rootProperties()
{
	QVariantMap props;
	props.insert(QStringLiteral("instanceModel"),
				 QVariant::fromValue(expose(m_instances.get())));
	props.insert(QStringLiteral("selection"),
				 QVariant::fromValue(expose(m_selection.get())));
	props.insert(QStringLiteral("shell"), QVariant::fromValue(expose(this)));
	/* MESHMC_QML_ROUTE opens a given screen at startup -- see
	 * Main.qml's applyDevRoute(). With MESHMC_QML_SNAPSHOT it lets a
	 * review script picture any page without editing the QML. */
	props.insert(QStringLiteral("devRoute"),
				 qEnvironmentVariable("MESHMC_QML_ROUTE"));
	/* Whether launcher/qml/Cat (the roaming cat companion) was built at
	 * all -- a compile-time fact, not a setting, so it is a plain bool
	 * handed over once here rather than a Q_PROPERTY. Main.qml's catLoader
	 * and SettingsPage's Cat group both gate on this: MESHMC_HAS_CAT is
	 * only ever defined when launcher/qml/CMakeLists.txt actually linked
	 * MeshMC_qml_cat in (see its MeshMC_ENABLE_CAT guard), which itself
	 * only happens when Qt Quick3D was found. */
#ifdef MESHMC_HAS_CAT
	props.insert(QStringLiteral("catAvailable"), true);
#else
	props.insert(QStringLiteral("catAvailable"), false);
#endif
	return props;
}

bool QmlShell::show(bool minimized)
{
	if (!load()) {
		return false;
	}
	if (m_shown) {
		m_window->showNormal();
		m_window->raise();
		m_window->requestActivate();
		return true;
	}
	m_shown = true;

	if (minimized)
		m_window->showMinimized();
	else
		m_window->show();

	scheduleSnapshotIfRequested();
	return true;
}

bool QmlShell::load()
{
	if (m_window) {
		return true;
	}

	/* The grid shows the core's instance list through a filtering, naturally
	 * sorted proxy -- the same ordering the widget grid used -- and keeps its
	 * selection by instance id, since rows move under the proxy. */
	m_instances = std::make_unique<InstanceFilterModel>();
	m_instances->setSourceModel(LAUNCHER->instances().get());
	// groups() forwards to m_instances->groups(); its own signal already
	// fires exactly when that list actually moves (see refreshDerived()).
	connect(m_instances.get(), &InstanceFilterModel::groupsChanged, this,
			&QmlShell::groupsChanged);
	m_recent = std::make_unique<InstanceFilterModel>();
	m_recent->setRecentFirst(true);
	m_recent->setSourceModel(LAUNCHER->instances().get());
	m_hero = std::make_unique<InstanceFilterModel>();
	/* Nothing matches until QML names an instance, so there is no flash of
	 * every instance as a hero. Ids are folder names; none contains '/'. */
	m_hero->setInstanceId(QStringLiteral("/"));
	m_hero->setSourceModel(LAUNCHER->instances().get());
	m_instancePage = std::make_unique<InstanceFilterModel>();
	m_instancePage->setInstanceId(QStringLiteral("/"));
	m_instancePage->setSourceModel(LAUNCHER->instances().get());
	m_recentWorlds =
		std::make_unique<RecentWorldsModel>(LAUNCHER->instances().get());
	m_selection = std::make_unique<IdSelectionModel>();
	m_settings = std::make_unique<SettingsAdapter>(LAUNCHER->settings());
	m_uiHost = std::make_unique<QmlUiHost>();
	m_modpacks = std::make_unique<ModrinthModpackModel>();
	m_curseForgeModpacks = std::make_unique<FlameModpackModel>();
	m_ftbModpacks = std::make_unique<FtbModpackModel>();
	m_atlModpacks = std::make_unique<AtlModpackModel>();
	m_technicModpacks = std::make_unique<TechnicModpackModel>();
	/* An ATLauncher pack's optional mods are asked through UiHost (the QML
	 * "optionalMods" question); without an answer the pack's defaults
	 * install. */
	m_atlModpacks->setOptionalModsHandler(
		[](const QString& name,
		   AtlOptionalModsModel& mods) -> std::optional<QVector<QString>> {
			UiHost* host = LAUNCHER->uiHost();
			if (host && host->chooseOptionalMods(name, mods)) {
				return mods.selectedNames();
			}
			return std::nullopt;
		});
	m_accountsController =
		std::make_unique<AccountsController>(LAUNCHER->accounts());
	m_about = std::make_unique<AboutInfo>();
	if (auto* checker = LAUNCHER->newsChecker()) {
		m_news = std::make_unique<NewsModel>(checker);
	}

	/* The sort order reads InstSortMode on every comparison, but a proxy
	 * only compares when told to: re-sort when the setting moves. */
	connect(LAUNCHER->settings().get(), &SettingsObject::SettingChanged, this,
			[this](const Setting& setting, const QVariant&) {
				if (setting.id() != QLatin1String("InstSortMode")) {
					return;
				}
				m_instances->invalidate();
				for (auto& section : m_sections) {
					section.second->invalidate();
				}
			});

	/* Must be chosen before the first engine exists: Qt Quick Controls binds
	 * its style when QtQuick.Controls is first imported, and cannot switch
	 * afterwards. The style falls back to Basic for anything it does not
	 * define itself. */
	QQuickStyle::setStyle(QStringLiteral("MeshMC.Style"));

	m_engine = std::make_unique<QQmlApplicationEngine>();
	m_engine->addImportPath(QStringLiteral("qrc:/qt/qml"));

	/* The engine takes ownership of image providers. Registered before load()
	 * so the first frame already has icons rather than broken images. */
	m_engine->addImageProvider(QStringLiteral("instanceicon"),
							   new InstanceIconProvider(LAUNCHER->icons()));
	m_engine->addImageProvider(QStringLiteral("accountface"),
							   new AccountFaceProvider());
	m_engine->addImageProvider(QStringLiteral("screenshot"),
							   new ScreenshotThumbnailProvider());
	// The same directory every ModFolderModel's parse tasks write to.
	m_engine->addImageProvider(
		QStringLiteral("modicon"),
		new ModIconProvider(ModIconCache::directory()));
	m_engine->setInitialProperties(rootProperties());
	m_engine->load(kRootUrl);

	const auto roots = m_engine->rootObjects();
	m_window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
	if (!m_window) {
		qCritical() << "QML shell: the root object at" << kRootUrl
					<< "failed to load or is not a window";
		m_engine.reset();
		return false;
	}

	/* Tells whether the window is really closing, not merely hidden --
	 * see eventFilter() and closed()'s doc comment. */
	m_window->installEventFilter(this);

	/* Imports asked for before there was a window to ask in -- see
	 * requestImport(). Main.qml's handler exists now: the engine's load()
	 * above created it along with the rest of the root window. */
	const QStringList pendingImports = std::exchange(m_pendingImports, {});
	for (const QString& source : pendingImports) {
		emit importRequested(source);
	}
	return true;
}

void QmlShell::scheduleSnapshotIfRequested()
{
	/* MESHMC_QML_SNAPSHOT=<file.png> renders the real window, with the real
	 * models, into an image and exits. Combined with QT_QPA_PLATFORM=offscreen
	 * it never touches a display, so it can run on a CI runner and be diffed:
	 * this is what visual regression checks of the QML UI are built on.
	 *
	 * The delay lets layouts settle and asynchronously loaded images arrive;
	 * grabWindow() then forces a render of whatever is current. */
	const QString path = qEnvironmentVariable("MESHMC_QML_SNAPSHOT");
	if (path.isEmpty())
		return;

	/* Long enough for the first frame; MESHMC_QML_SNAPSHOT_DELAY (ms) waits
	 * longer, for pages that show network results. */
	bool delayOk = false;
	const int delay =
		qEnvironmentVariableIntValue("MESHMC_QML_SNAPSHOT_DELAY", &delayOk);
	QTimer::singleShot(delayOk ? delay : 750, this, [this, path]() {
		const QImage image = m_window->grabWindow();
		const bool saved = !image.isNull() && image.save(path);
		if (saved)
			qInfo() << "QML shell: snapshot written to" << path << image.size();
		else
			qCritical() << "QML shell: could not write snapshot to" << path;
		QCoreApplication::exit(saved ? 0 : 1);
	});
}

QWindow* QmlShell::window() const
{
	return m_window;
}

bool QmlShell::eventFilter(QObject* watched, QEvent* event)
{
	/* PluginManager installs its own close filter on this same window
	 * once a plugin registers one (main_window_install_close_filter(),
	 * see PluginManager::ensureCloseFilterInstalled()) -- always after
	 * this one, since that only happens once a plugin's
	 * MMCO_HOOK_UI_MAIN_READY handler runs, which is dispatched only
	 * after load() (and this installEventFilter() call) has already
	 * returned. Qt calls the most-recently-installed filter first, so a
	 * plugin veto (ce->ignore(), then stopping the event by returning
	 * true) never reaches here -- the same way a vetoed close never
	 * reaches MainWindow::closeEvent() on the widget path. Seeing the
	 * event here therefore means nothing vetoed it: the window is
	 * really closing, not merely being hidden (main_window_hide(), or a
	 * veto's own hide(), both go straight to QWindow::hide() and raise
	 * no QEvent::Close at all). */
	if (watched == m_window && event->type() == QEvent::Close)
		emit closed();
	return QObject::eventFilter(watched, event);
}
