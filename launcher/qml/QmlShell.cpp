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
#include <QImage>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceCopyTask.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "java/JavaInstallList.h"
#include "models/AccountsController.h"
#include "models/IdSelectionModel.h"
#include "models/InstanceDetails.h"
#include "models/InstanceFilterModel.h"
#include "models/NewInstanceController.h"
#include "models/SettingsAdapter.h"
#include "models/ContentBrowser.h"
#include "modplatform/modrinth/ModrinthModpackModel.h"
#include "tasks/Task.h"
#include "tasks/TaskWatcher.h"
#include "translations/TranslationsModel.h"
#include "Sys.h"
#include "DesktopServices.h"
#include "settings/SettingsObject.h"
#include <QDir>
#include "qml/AccountFaceProvider.h"
#include "qml/InstanceIconProvider.h"
#include "qml/QmlUiHost.h"
#include "qml/ScreenshotThumbnailProvider.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"
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

void QmlShell::killInstance(const QString& id)
{
	emit killRequested(id);
}

void QmlShell::editInstance(const QString& id)
{
	emit editRequested(id);
}

void QmlShell::openInstanceFolder(const QString& id)
{
	emit folderRequested(id);
}

void QmlShell::createInstance()
{
	emit createInstanceRequested();
}

void QmlShell::openSettings(const QString& page)
{
	emit settingsRequested(page);
}

void QmlShell::openPath(const QString& path)
{
	DesktopServices::openDirectory(QDir(path).absolutePath(), true);
}

void QmlShell::manageAccounts()
{
	emit accountsRequested();
}

void QmlShell::showInstanceLogRequested(const QString& id)
{
	emit openInstanceLog(id);
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

QObject* QmlShell::installContent(int row, const QString& versionId)
{
	auto* browser = m_instanceDetails
						? qobject_cast<ContentBrowser*>(
							  m_instanceDetails->contentBrowser())
						: nullptr;
	if (!browser) {
		return nullptr;
	}
	return expose(browser->install(row, versionId));
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

QObject* QmlShell::accountsController() const
{
	return expose(m_accountsController.get());
}

QObject* QmlShell::newInstance() const
{
	/* Made on first use, not in show(): the controller starts loading the
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
	expose(m_instanceDetails->components());
	expose(m_instanceDetails->screenshots());
	// contentBrowser()'s own `results` is reachable straight off the
	// pinned browser below without a separate expose() here: every
	// ContentProviderModel it hands out is parented to it (see
	// ContentBrowser::ensureModel()), and a parented QObject already keeps
	// its C++ ownership once QML touches it, pin or no pin.
	expose(m_instanceDetails->contentBrowser());

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
	return props;
}

bool QmlShell::show(bool minimized)
{
	if (m_window) {
		m_window->showNormal();
		m_window->raise();
		m_window->requestActivate();
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
	m_selection = std::make_unique<IdSelectionModel>();
	m_settings = std::make_unique<SettingsAdapter>(LAUNCHER->settings());
	m_uiHost = std::make_unique<QmlUiHost>();
	m_modpacks = std::make_unique<ModrinthModpackModel>();
	m_accountsController =
		std::make_unique<AccountsController>(LAUNCHER->accounts());

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

	if (minimized)
		m_window->showMinimized();
	else
		m_window->show();

	scheduleSnapshotIfRequested();
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
	 * after show() (and this installEventFilter() call) has already
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
