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

#include "qml/QmlShell.h"

#include <algorithm>

#include <QCoreApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QDebug>
#include <QEvent>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPointer>
#include <QImage>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#ifdef Q_OS_MACOS
#include <QGuiApplication>
#include <QQmlProperty>

#include "qml/MacWindowChrome.h"
#endif

#include "BaseInstance.h"
#include "minecraft/GraphicsBackend.h"
#include "minecraft/MinecraftInstance.h"
#include "FileSystem.h"
#include "InstanceCopyTask.h"
#include "InstanceList.h"
#include "icons/AutoIcons.h"
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
#include "models/LibraryController.h"
#include "models/LauncherLogs.h"
#include "models/CatPackList.h"
#include "BuildConfig.h"
#include "models/InstanceFilterModel.h"
#include "models/NewInstanceController.h"
#include "minecraft/PerformanceInstanceTask.h"
#include "minecraft/PerformanceSetupRules.h"
#include "minecraft/PerformanceSetupTask.h"
#include "minecraft/mod/ModFolderModel.h"
#include "modplatform/BundleCatalog.h"
#include "friends/FriendsController.h"
#include "servers/FeaturedServers.h"
#include "servers/ServerPinger.h"
#include "modbridge/ModBridge.h"
#include "settings/Setting.h"
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
#include "minecraft/skins/DefaultSkins.h"

namespace
{
	// loadFromModule() needs Qt 6.5. The module sets RESOURCE_PREFIX "/qt/qml".
	const QUrl kRootUrl(QStringLiteral("qrc:/qt/qml/Kite/Main.qml"));

	// Where PerformanceSetupTask lists the mods it found no build of for the new version.
	const QString kUnmatchedSetting = QStringLiteral("KitePerformanceUnmatched");

	// Function-local static: sidesteps static initialisation order.
	QmlShell::PluginSurfaceFactory& pluginSurfaceFactory()
	{
		static QmlShell::PluginSurfaceFactory factory;
		return factory;
	}
} // namespace

QString sanitizedInstanceName(const QString& name)
{
	QString sanitized = name;
	sanitized.replace(QLatin1Char('\n'), QLatin1Char(' '));
	return sanitized.trimmed();
}

bool languageSetupStepNeeded(const QString& language)
{
	return language.isEmpty();
}

bool javaSetupStepNeeded(bool hostnameChanged, bool javaPathResolves,
						 bool autoDownload)
{
	return hostnameChanged || (!javaPathResolves && !autoDownload);
}

QmlShell::QmlShell(QObject* parent) : QObject(parent)
{
	auto accounts = LAUNCHER->accounts();
	// QML caches images by url; the revision in the face url forces a refetch.
	const auto bump = [this] {
		++m_accountRevision;
		emit accountChanged();
	};
	connect(accounts.get(), &AccountList::listChanged, this, bump);
	connect(accounts.get(), &AccountList::defaultAccountChanged, this, bump);
	// An account without a skin shows the default skin once it has downloaded.
	connect(DefaultSkins::instance(), &DefaultSkins::loaded, this, bump);

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
	return QStringLiteral("image://accountface/%1?rev=%2")
		.arg(account->accountId())
		.arg(m_accountRevision);
}

QString QmlShell::accountKind() const
{
	auto account = LAUNCHER->accounts()->defaultAccount();
	if (!account) {
		return QString();
	}
	return account->kindLabel();
}

int QmlShell::accountCount() const
{
	return LAUNCHER->accounts()->count();
}

void QmlShell::launchInstance(const QString& id)
{
	watchPerformance(id);
	emit launchRequested(id);
}

void QmlShell::launchInstanceMode(const QString& id, const QString& mode)
{
	watchPerformance(id);
	emit launchModeRequested(id, mode);
}

void QmlShell::launchInstanceAs(const QString& id, const QString& accountId)
{
	watchPerformance(id);
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

// Needs a mod loader and a game version the performance mods exist for.
static bool
performanceAvailable(const std::shared_ptr<MinecraftInstance>& instance)
{
	auto profile = instance->getPackProfile();
	return PerformanceSetup::hasLoader(profile->primaryModLoader()) &&
		   PerformanceSetup::supportsVersion(
			   profile->getComponentVersion(QStringLiteral("net.minecraft")));
}

QVariantMap QmlShell::launchOptions(const QString& id) const
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return {};
	}

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
			{ QStringLiteral("id"), account->accountId() },
			{ QStringLiteral("name"), account->profileName() },
			{ QStringLiteral("kind"), account->kindLabel() },
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
		{ QStringLiteral("performance"), minecraft && performanceEnabled(id) },
		{ QStringLiteral("performanceAvailable"),
		  minecraft && performanceAvailable(minecraft) },
	};
}

void QmlShell::setInstanceProfiler(const QString& id,
								   const QString& profilerKey)
{
	if (auto instance = LAUNCHER->instances()->getInstanceById(id)) {
		instance->setProfilerKey(profilerKey);
	}
}

static std::shared_ptr<MinecraftInstance>
minecraftInstanceById(const QString& id)
{
	return std::dynamic_pointer_cast<MinecraftInstance>(
		LAUNCHER->instances()->getInstanceById(id));
}

bool QmlShell::performanceEnabled(const QString& id) const
{
	auto instance = minecraftInstanceById(id);
	return instance && instance->settings()->get("KitePerformance").toBool();
}

void QmlShell::setPerformanceEnabled(const QString& id, bool on)
{
	if (auto instance = minecraftInstanceById(id)) {
		instance->settings()->set("KitePerformance", on);
		emit performanceChanged(id);
	}
}

QStringList QmlShell::performanceExcluded(const QString& id) const
{
	auto instance = minecraftInstanceById(id);
	return instance ? PerformanceSetup::parseProjects(
						  instance->settings()
							  ->get("KitePerformanceExcluded")
							  .toString())
					: QStringList();
}

void QmlShell::setPerformanceExcluded(const QString& id,
									  const QStringList& projects)
{
	if (auto instance = minecraftInstanceById(id)) {
		instance->settings()->set("KitePerformanceExcluded",
								  PerformanceSetup::joinProjects(projects));
		emit performanceChanged(id);
	}
}

QStringList QmlShell::performanceIncluded(const QString& id) const
{
	auto instance = minecraftInstanceById(id);
	return instance ? PerformanceSetup::parseProjects(
						  instance->settings()
							  ->get("KitePerformanceIncluded")
							  .toString())
					: QStringList();
}

void QmlShell::setPerformanceIncluded(const QString& id,
									  const QStringList& projects)
{
	if (auto instance = minecraftInstanceById(id)) {
		instance->settings()->set("KitePerformanceIncluded",
								  PerformanceSetup::joinProjects(projects));
		emit performanceChanged(id);
	}
}

QStringList QmlShell::performanceUnmatched(const QString& id) const
{
	auto instance = minecraftInstanceById(id);
	auto folder = instance ? instance->loaderModList() : nullptr;
	if (!folder) {
		return {};
	}
	return PerformanceSetup::installedNames(
		*folder->metadataIndex(),
		PerformanceSetup::parseProjects(
			instance->settings()->get(kUnmatchedSetting).toString()));
}

void QmlShell::watchPerformance(const QString& id)
{
	auto instance = minecraftInstanceById(id);
	if (!instance || m_watchedPerformance.contains(id)) {
		return;
	}
	m_watchedPerformance.insert(id);
	/* The setup runs inside a launch, which this shell does not own, so the
	 * setting is how it tells. Every launch re-reads the instance's settings
	 * and that announces each of them again, unchanged: only a value that
	 * differs from the last one seen is news. */
	connect(instance->settings().get(), &SettingsObject::SettingChanged, this,
			[this, id, seen = instance->settings()
								  ->get(kUnmatchedSetting)
								  .toString()](const Setting& setting,
											   const QVariant& value) mutable {
				if (setting.id() != kUnmatchedSetting ||
					value.toString() == seen) {
					return;
				}
				seen = value.toString();
				const QStringList names = performanceUnmatched(id);
				if (!names.isEmpty()) {
					emit performanceUnmatchedFound(id, names);
				}
			});
}

bool QmlShell::performanceSupported(const QString& mcVersion) const
{
	return PerformanceSetup::supportsVersion(mcVersion);
}

static bool
hasGraphicsBackend(const std::shared_ptr<MinecraftInstance>& instance)
{
	return GraphicsBackend::supportsVersion(
		instance->getPackProfile()->getComponentVersion(
			QStringLiteral("net.minecraft")));
}

QString QmlShell::graphicsBackend(const QString& id) const
{
	auto instance = minecraftInstanceById(id);
	if (!instance || !hasGraphicsBackend(instance)) {
		return QString();
	}
	return GraphicsBackend::read(instance->gameRoot());
}

bool QmlShell::setGraphicsBackend(const QString& id, const QString& backend)
{
	auto instance = minecraftInstanceById(id);
	if (!instance || !hasGraphicsBackend(instance) || instance->isRunning()) {
		return false;
	}
	return GraphicsBackend::write(instance->gameRoot(), backend);
}

QObject* QmlShell::performanceSetup(const QString& id) const
{
	return m_performanceSetups.value(id).data();
}

QObject* QmlShell::speedUp(const QString& id, const QStringList& extras)
{
	if (auto* running = performanceSetup(id)) {
		return running;
	}

	auto instance = minecraftInstanceById(id);
	if (!instance || instance->isRunning() ||
		!PerformanceSetup::hasLoader(instance->getPackProfile()->primaryModLoader())) {
		return nullptr;
	}
	watchPerformance(id);
	instance->settings()->set("KitePerformance", true);
	emit performanceChanged(id);

	auto* task = new PerformanceSetupTask(instance, extras);
	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Kite Performance"));
	m_performanceSetups.insert(id, watcher);
	connect(watcher, &TaskWatcher::finished, this,
			[this, id]() { m_performanceSetups.remove(id); });
	task->start();
	return expose(watcher);
}

QObject* QmlShell::createPerformanceInstance(const QString& sourceId)
{
	auto source = minecraftInstanceById(sourceId);
	const QString mcVersion =
		source ? source->getPackProfile()->getComponentVersion(
					 QStringLiteral("net.minecraft"))
			   : QString();
	if (!source || !PerformanceSetup::supportsVersion(mcVersion)) {
		return nullptr;
	}

	const QString name = tr("%1 · Kite Performance").arg(source->name());
	auto* task = new PerformanceInstanceTask(
		mcVersion, name, LAUNCHER->instances()->getInstanceGroup(sourceId));
	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(name);
	// The id is known once the instance exists, before the task reports
	// success, so finished(true) reaches QML with it set.
	connect(task, &PerformanceInstanceTask::instanceCreated, watcher,
			&TaskWatcher::setInstanceId);
	task->start();
	return expose(watcher);
}

void QmlShell::killInstance(const QString& id)
{
	emit killRequested(id);
}

void QmlShell::openInstanceFolder(const QString& id)
{
	emit folderRequested(id);
}

bool QmlShell::joinServer(const QString& id, const QString& address)
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	const bool starts = instance && instance->canLaunch();
	watchPerformance(id);
	emit joinServerRequested(id, address);
	return starts;
}

namespace
{
	using MinecraftInstances = QList<std::shared_ptr<MinecraftInstance>>;

	// minecraftVersion(), not the component's version, which is empty until
	// the profile has been read.
	QString gameVersionOf(const std::shared_ptr<MinecraftInstance>& instance)
	{
		return instance->minecraftVersion();
	}

	QVariantList joinCandidates(const MinecraftInstances& instances)
	{
		QVariantList list;
		for (const auto& instance : instances) {
			list.append(QVariantMap{
				{ QStringLiteral("id"), instance->id() },
				{ QStringLiteral("name"), instance->name() },
				{ QStringLiteral("iconKey"), instance->iconKey() },
				{ QStringLiteral("version"), gameVersionOf(instance) },
				{ QStringLiteral("loader"), instance->modLoaderName() },
				{ QStringLiteral("lastLaunch"), instance->lastLaunch() },
				{ QStringLiteral("running"), instance->isRunning() },
				{ QStringLiteral("canLaunch"), instance->canLaunch() },
			});
		}
		return list;
	}

	MinecraftInstances minecraftInstances()
	{
		MinecraftInstances all;
		auto instances = LAUNCHER->instances();
		for (int i = 0; i < instances->count(); ++i) {
			if (auto minecraft = std::dynamic_pointer_cast<MinecraftInstance>(
					instances->at(i))) {
				all.append(minecraft);
			}
		}
		return all;
	}

	QList<JoinChoice> choicesOf(const MinecraftInstances& instances)
	{
		QList<JoinChoice> choices;
		for (const auto& instance : instances) {
			choices.append({ gameVersionOf(instance), instance->lastLaunch(),
							 instance->name() });
		}
		return choices;
	}

	MinecraftInstances picked(const MinecraftInstances& instances,
							  const QList<int>& indexes)
	{
		MinecraftInstances chosen;
		for (int index : indexes) {
			chosen.append(instances.at(index));
		}
		return chosen;
	}

	// "1.21" of "1.21.4".
	QString releaseLineOf(const QString& version)
	{
		return version.split(QLatin1Char('.')).mid(0, 2).join(QLatin1Char('.'));
	}

	/* Most recently played first, then by name in the user's language. */
	bool playedBefore(const JoinChoice& a, const JoinChoice& b)
	{
		if (a.lastLaunch != b.lastLaunch) {
			return a.lastLaunch > b.lastLaunch;
		}
		return a.name.localeAwareCompare(b.name) < 0;
	}
} // namespace

QList<int> joinMatches(const QList<JoinChoice>& choices, const QString& version)
{
	QList<int> matches;
	for (int i = 0; i < choices.size(); ++i) {
		if (!version.isEmpty() && choices.at(i).version == version) {
			matches.append(i);
		}
	}
	std::stable_sort(matches.begin(), matches.end(), [&choices](int a, int b) {
		return playedBefore(choices.at(a), choices.at(b));
	});
	return matches;
}

QList<int> joinOthers(const QList<JoinChoice>& choices, const QString& version)
{
	const QString line = releaseLineOf(version);
	QList<int> others;
	for (int i = 0; i < choices.size(); ++i) {
		const QString& other = choices.at(i).version;
		if (!version.isEmpty() && !other.isEmpty() && other != version) {
			others.append(i);
		}
	}
	std::stable_sort(others.begin(), others.end(), [&](int a, int b) {
		const bool sameA = releaseLineOf(choices.at(a).version) == line;
		const bool sameB = releaseLineOf(choices.at(b).version) == line;
		return sameA != sameB ? sameA
							  : playedBefore(choices.at(a), choices.at(b));
	});
	return others;
}

QVariantList QmlShell::instancesForVersion(const QString& mcVersion) const
{
	const auto all = minecraftInstances();
	return joinCandidates(
		picked(all, joinMatches(choicesOf(all), mcVersion)));
}

QVariantList QmlShell::otherInstances(const QString& mcVersion) const
{
	const auto all = minecraftInstances();
	return joinCandidates(picked(all, joinOthers(choicesOf(all), mcVersion)));
}

QVariantList QmlShell::instancesForServer(const QString& minVersion,
										  const QString& maxVersion,
										  const QStringList& lines) const
{
	const auto all = minecraftInstances();
	const QList<JoinChoice> choices = choicesOf(all);
	QList<bool> fits;
	QList<int> order;
	for (int i = 0; i < choices.size(); ++i) {
		fits.append(FeaturedServers::versionFits(choices.at(i).version,
												 minVersion, maxVersion, lines));
		order.append(i);
	}
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		return fits.at(a) != fits.at(b)
				   ? fits.at(a)
				   : playedBefore(choices.at(a), choices.at(b));
	});
	QVariantList list = joinCandidates(picked(all, order));
	for (int i = 0; i < list.size(); ++i) {
		QVariantMap one = list.at(i).toMap();
		one.insert(QStringLiteral("fits"), fits.at(order.at(i)));
		list[i] = one;
	}
	return list;
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

	// Setting the name saves a file, so skip it when unchanged.
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

bool QmlShell::setInstanceCover(const QString& instanceId,
								const QString& pathOrFileUrl)
{
	return LAUNCHER->instances()->setCover(instanceId, pathOrFileUrl);
}

void QmlShell::clearInstanceCover(const QString& instanceId)
{
	LAUNCHER->instances()->clearCover(instanceId);
}

void QmlShell::renameInstanceCoverSource(const QString& instanceId,
										 const QString& fileName)
{
	LAUNCHER->instances()->renameCoverSource(instanceId, fileName);
}

QColor QmlShell::iconColor(const QString& key) const
{
	return LAUNCHER->instances()->iconColor(key);
}

void QmlShell::debugMarkInstanceCrashed(const QString& id)
{
	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return;
	}
	instance->setCrashed(true);
}

void QmlShell::debugClick(const QString& spec)
{
	const QStringList parts = spec.split(QLatin1Char(','));
	if (parts.size() < 2 || !m_window) {
		return;
	}
	const QPointF point(parts.at(0).toDouble(), parts.at(1).toDouble());
	const Qt::MouseButton button = parts.size() > 2 && parts.at(2) == QLatin1String("right")
									   ? Qt::RightButton
									   : Qt::LeftButton;
	const int delay = parts.size() > 3 ? parts.at(3).toInt() : 1000;
	const QPointF offset = parts.size() > 5
							   ? QPointF(parts.at(4).toDouble(), parts.at(5).toDouble())
							   : QPointF();
	const int steps = offset.isNull() ? 0 : 10;
	// Long enough for a hover fade-in to finish before the press.
	const int hoverMs = 400;
	const int stepMs = 40;
	QPointer<QQuickWindow> window = m_window;
	auto at = [this, window](int ms, QEvent::Type type, QPointF pos, Qt::MouseButton which,
							 Qt::MouseButtons held) {
		QTimer::singleShot(ms, this, [window, type, pos, which, held] {
			if (!window) {
				return;
			}
			QMouseEvent event(type, pos, window->mapToGlobal(pos), which, held, Qt::NoModifier);
			// Pointer handlers time taps and drags from the timestamp; zero
			// is not a click.
			static QElapsedTimer clock;
			if (!clock.isValid()) {
				clock.start();
			}
			event.setTimestamp(quint64(clock.elapsed()) + 1);
			QCoreApplication::sendEvent(window, &event);
		});
	};
	QTimer::singleShot(delay, this, [window, point] {
		// Menu.popup() with no position opens at the cursor.
		if (window) {
			QCursor::setPos(window->mapToGlobal(point).toPoint());
		}
	});
	at(delay, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
	if (parts.size() > 2 && parts.at(2) == QLatin1String("hover")) {
		return;
	}
	at(delay + hoverMs, QEvent::MouseButtonPress, point, button, button);
	for (int i = 1; i <= steps; ++i) {
		at(delay + hoverMs + stepMs * i, QEvent::MouseMove, point + offset * (qreal(i) / steps),
		   Qt::NoButton, button);
	}
	at(delay + hoverMs + stepMs * (steps + 1), QEvent::MouseButtonRelease, point + offset, button,
	   Qt::NoButton);
}

void QmlShell::debugShowRequest(const QString& kind)
{
	if (!m_uiHost) {
		return;
	}
	if (kind == QLatin1String("pickAccount")) {
		// Queued: pickAccount() blocks, and the QML call that got here
		// should return first.
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
		// Kept alive by its own skip callback, which ends it.
		auto holder =
			std::make_shared<std::unique_ptr<UiHost::BusyIndicator>>();
		*holder = m_uiHost->showBusy(tr("Refreshing account…"),
									 tr("Play offline"),
									 [holder]() { holder->reset(); });
	}
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
	// Trashing a folder Java has open fails on Windows, and the fallback
	// below would delete a running game's files one by one.
	if (instance->isRunning()) {
		return false;
	}

	auto instances = LAUNCHER->instances();
	if (!instances->trashInstance(id) && !instances->deleteInstance(id)) {
		return false;
	}

	return true;
}

QStringList QmlShell::groups() const
{
	return m_instances ? m_instances->groups() : QStringList();
}

QObject* QmlShell::iconsModel() const
{
	return expose(LAUNCHER->icons().get());
}

bool QmlShell::iconFillsTile(const QString& key) const
{
	return LAUNCHER->icons()->fillsTile(key);
}

void QmlShell::refreshAutoIcons()
{
	const auto* instances = LAUNCHER->instances().get();
	QList<AutoIcons::Instance> list;
	for (int i = 0; i < instances->count(); ++i) {
		const InstancePtr instance = instances->at(i);
		list.append({instance->id(), instance->iconKey()});
	}
	const QString stored = LAUNCHER->settings()->get("UiAutoIcons").toString();
	const QHash<QString, QString> map =
		AutoIcons::assign(list, AutoIcons::fromJson(stored), AutoIcons::pool());
	// QJsonObject writes its keys sorted, so an unchanged map reads the same.
	const QString json = AutoIcons::toJson(map);
	if (json != stored)
		LAUNCHER->settings()->set("UiAutoIcons", json);

	QVariantMap exposed;
	for (auto it = map.begin(); it != map.end(); ++it)
		exposed.insert(it.key(), it.value());
	if (exposed == m_autoIcons)
		return;
	m_autoIcons = exposed;
	emit autoIconsChanged();
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

	// A new machine may mean the recorded JavaPath no longer applies; record
	// the new hostname so this fires once.
	const QString currentHostName = QHostInfo::localHostName();
	const QString oldHostName = settings->get("LastHostname").toString();
	if (currentHostName != oldHostName) {
		settings->set("LastHostname", currentHostName);
		m_hostnameChanged = true;
	}
	const QString javaPath = settings->get("JavaPath").toString();
	const bool javaPathResolves = !FS::ResolveExecutable(javaPath).isNull();

	QStringList steps;
	if (languageSetupStepNeeded(settings->get("Language").toString())) {
		steps << QStringLiteral("language");
	}
	if (javaSetupStepNeeded(m_hostnameChanged, javaPathResolves,
							settings->get("JavaAutoDownload").toBool())) {
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
	if (id == QLatin1String("java")) {
		m_hostnameChanged = false;
	}
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
	// selectedLanguage(), not @p key: selectLanguage() falls back to the
	// default for an unrecognised key.
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
	// A scan is already running and its finished() is already connected.
	if (m_javaDetecting) {
		return;
	}
	m_javaDetecting = true;
	emit javaDetectingChanged();
	connect(
		task.get(), &Task::finished, this,
		[this]() {
			m_javaDetecting = false;
			emit javaDetectingChanged();
		},
		Qt::SingleShotConnection);
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
	// Null until a QML item has called setPresenterReady(true). Application
	// then uses the headless host, so an early call is declined instead of
	// hanging on a request nothing shows.
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
	// Made on first use: it loads the Minecraft version list, which opening
	// the launcher should not fetch.
	if (!m_newInstance && m_engine) {
		m_newInstance = std::make_unique<NewInstanceController>();
	}
	// The version list proxies it hands out need pinning too.
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

QObject* QmlShell::library() const
{
	if (!m_library && m_engine) {
		m_library = std::make_unique<LibraryController>(LAUNCHER->instances(),
														LAUNCHER->icons());
	}
	if (m_library) {
		expose(m_library->iconsModel());
	}
	return expose(m_library.get());
}

QObject* QmlShell::launcherLogs() const
{
	// The working directory is the data folder, where <KITE_NAME>-<n>.log
	// is written.
	if (!m_launcherLogs) {
		m_launcherLogs = std::make_unique<LauncherLogs>(
			QDir::currentPath(), BuildConfig.KITE_NAME);
	}
	return expose(m_launcherLogs.get());
}

QObject* QmlShell::catPacks() const
{
	if (!m_catPacks) {
		m_catPacks = std::make_unique<CatPackList>();
	}
	return expose(m_catPacks.get());
}

QObject* QmlShell::bundles() const
{
	if (!m_bundles) {
		m_bundles = std::make_unique<BundleCatalog>();
		// Texts resolve on each read; only the bindings need telling.
		connect(LAUNCHER->settings()->getSetting("Language").get(),
				&Setting::SettingChanged, m_bundles.get(),
				&BundleCatalog::changed);
		m_bundles->refresh();
	}
	return expose(m_bundles.get());
}

QObject* QmlShell::friends() const
{
	if (!FriendsController::available()) {
		return nullptr;
	}
	if (!m_friends) {
		// The working directory is the data folder, where accounts.json is.
		m_friends = std::make_unique<FriendsController>(QDir::currentPath());
		// The Kite mod of a running game reads it through the bridge.
		if (auto* bridge = LAUNCHER ? LAUNCHER->modBridge() : nullptr) {
			bridge->setFriends(m_friends.get());
		}
	}
	return expose(m_friends.get());
}

QObject* QmlShell::serverPinger() const
{
	if (!m_serverPinger) {
		m_serverPinger = std::make_unique<ServerPinger>();
	}
	return expose(m_serverPinger.get());
}

QObject* QmlShell::featuredServers() const
{
	if (!m_featuredServers) {
		m_featuredServers = std::make_unique<FeaturedServers>();
		// Notes resolve on each read; only the bindings need telling.
		connect(LAUNCHER->settings()->getSetting("Language").get(),
				&Setting::SettingChanged, m_featuredServers.get(),
				&FeaturedServers::changed);
		m_featuredServers->loadCached();
		m_featuredServers->refresh();
	}
	return expose(m_featuredServers.get());
}

QObject* QmlShell::exportController(const QString& id)
{
	if (!m_exportController || m_exportController->instanceId() != id) {
		auto instance = LAUNCHER->instances()->getInstanceById(id);
		if (!instance) {
			return nullptr;
		}
		// Replacing a running export is safe: its destructor aborts the
		// task and keeps it alive until it has wound down.
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

	m_instanceDetails = std::make_unique<InstanceDetails>(instance);

	// Every QObject* the bridge hands to QML needs pinning.
	expose(m_instanceDetails->settings());
	expose(m_instanceDetails->mods());
	expose(m_instanceDetails->worlds());
	expose(m_instanceDetails->log());
	expose(m_instanceDetails->otherLogs());
	expose(m_instanceDetails->components());
	expose(m_instanceDetails->screenshots());
	expose(m_instanceDetails->gameOptions());
	// contentBrowser()'s `results` needs no pin of its own: its models are
	// parented to the browser.
	expose(m_instanceDetails->contentBrowser());
	// Likewise loaderInstaller()'s `versions` proxy.
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
	// KITE_QML_ROUTE opens a screen at startup (Main.qml's applyDevRoute()),
	// so a snapshot can picture any page.
	props.insert(QStringLiteral("devRoute"),
				 qEnvironmentVariable("KITE_QML_ROUTE"));
	// Compile-time: KITE_HAS_SKIN3D is defined only when Kite_qml_skin3d was
	// linked, which needs Qt Quick3D.
#ifdef KITE_HAS_SKIN3D
	props.insert(QStringLiteral("skin3DAvailable"), true);
#else
	props.insert(QStringLiteral("skin3DAvailable"), false);
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

	m_instances = std::make_unique<InstanceFilterModel>();
	m_instances->setSourceModel(LAUNCHER->instances().get());
	connect(m_instances.get(), &InstanceFilterModel::groupsChanged, this,
			&QmlShell::groupsChanged);
	// Deferred a turn so the delegates of a vanished group are gone first.
	connect(m_instances.get(), &InstanceFilterModel::groupsChanged, this, [this]() {
		QTimer::singleShot(0, this, [this]() {
			const QStringList live = m_instances->groups();
			for (auto it = m_sections.begin(); it != m_sections.end();) {
				if (live.contains(it->first)) {
					++it;
					continue;
				}
				it->second.release()->deleteLater();
				it = m_sections.erase(it);
			}
		});
	});
	connect(LAUNCHER->icons().get(), &IconList::iconUpdated, this, [this]() {
		if (m_iconRevisionPending) {
			return;
		}
		m_iconRevisionPending = true;
		QTimer::singleShot(0, this, [this]() {
			m_iconRevisionPending = false;
			++m_iconRevision;
			emit iconRevisionChanged();
		});
	});
	connect(LAUNCHER->instances().get(), &InstanceList::iconColorsChanged, this,
			[this]() {
				++m_iconColorRevision;
				emit iconColorRevisionChanged();
			});
	// The auto icons follow the library: rows in or out, or an icon key
	// change (reported as Qt::DecorationRole).
	auto* instanceList = LAUNCHER->instances().get();
	const auto scheduleAutoIcons = [this]() {
		if (m_autoIconsPending)
			return;
		m_autoIconsPending = true;
		QTimer::singleShot(0, this, [this]() {
			m_autoIconsPending = false;
			refreshAutoIcons();
		});
	};
	connect(instanceList, &QAbstractItemModel::rowsInserted, this, scheduleAutoIcons);
	connect(instanceList, &QAbstractItemModel::rowsRemoved, this, scheduleAutoIcons);
	connect(instanceList, &QAbstractItemModel::modelReset, this, scheduleAutoIcons);
	connect(instanceList, &QAbstractItemModel::dataChanged, this,
			[scheduleAutoIcons](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
				if (roles.isEmpty() || roles.contains(Qt::DecorationRole))
					scheduleAutoIcons();
			});
	refreshAutoIcons();

	m_recent = std::make_unique<InstanceFilterModel>();
	m_recent->setRecentFirst(true);
	m_recent->setSourceModel(LAUNCHER->instances().get());
	m_hero = std::make_unique<InstanceFilterModel>();
	// Nothing matches until QML names an instance. Ids are folder names; none
	// contains '/'.
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
	// Optional mods are asked through UiHost; without an answer the pack's
	// defaults install.
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

	// Only the library proxy follows InstGrouped; the section models are what
	// grouping shows.
	m_instances->setGrouped(LAUNCHER->settings()->get("InstGrouped").toBool());

	// The sort order is read on every comparison, but a proxy only compares
	// when told to.
	connect(LAUNCHER->settings().get(), &SettingsObject::SettingChanged, this,
			[this](const Setting& setting, const QVariant& value) {
				if (setting.id() == QLatin1String("InstGrouped")) {
					m_instances->setGrouped(value.toBool());
					return;
				}
				if (setting.id() != QLatin1String("InstSortMode")) {
					return;
				}
				m_instances->invalidate();
				for (auto& section : m_sections) {
					section.second->invalidate();
				}
			});

	// Must precede the first engine: Qt Quick Controls binds its style on first
	// import. It falls back to Basic for what the style does not define.
	QQuickStyle::setStyle(QStringLiteral("Kite.Style"));

	m_engine = std::make_unique<QQmlApplicationEngine>();
	m_engine->addImportPath(QStringLiteral("qrc:/qt/qml"));

	// The engine owns image providers. Registered before load() so the first
	// frame has icons.
	m_engine->addImageProvider(QStringLiteral("instanceicon"),
							   new InstanceIconProvider(LAUNCHER->icons()));
	m_engine->addImageProvider(QStringLiteral("accountface"),
							   new AccountFaceProvider());
	m_engine->addImageProvider(QStringLiteral("screenshot"),
							   new ScreenshotThumbnailProvider());
	// Where every ModFolderModel's parse tasks write.
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

	// Tells a real close from a hide.
	m_window->installEventFilter(this);

#ifdef Q_OS_MACOS
	// The title bar lies over the shell's top bar (Main.qml sizes it). Cocoa
	// only: an offscreen snapshot window has no NSView.
	if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
		const MacWindowChrome::Geometry chrome = MacWindowChrome::mergeTitleBar(m_window);
		if (chrome.controlsWidth > 0) {
			// ApplicationWindow keeps content out of the safe area, which now
			// takes in the title bar. QQmlProperty::write drops the binding.
			QQmlProperty::write(m_window, QStringLiteral("topPadding"), 0);
			m_window->setProperty("windowControlsWidth", chrome.controlsWidth);
			m_window->setProperty("titleBarHeight", chrome.barHeight);
		}
	}
#endif

	// Imports requested before there was a window; Main.qml's handler exists
	// now.
	const QStringList pendingImports = std::exchange(m_pendingImports, {});
	for (const QString& source : pendingImports) {
		emit importRequested(source);
	}
	return true;
}

void QmlShell::scheduleSnapshotIfRequested()
{
	/* KITE_QML_SNAPSHOT=<file.png> renders the real window into an image and
	 * exits; with QT_QPA_PLATFORM=offscreen it needs no display. The delay
	 * lets layouts settle and async images arrive. */
	const QString path = qEnvironmentVariable("KITE_QML_SNAPSHOT");
	if (path.isEmpty())
		return;

	// Long enough for the first frame; KITE_QML_SNAPSHOT_DELAY (ms) waits
	// longer for pages that show network results.
	bool delayOk = false;
	const int delay =
		qEnvironmentVariableIntValue("KITE_QML_SNAPSHOT_DELAY", &delayOk);
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
	/* PluginManager's close filter is installed after this one, so Qt calls it
	 * first and a plugin veto never reaches here. A Close seen here means the
	 * window is really closing; hide() raises no Close. */
	if (watched == m_window && event->type() == QEvent::Close)
		emit closed();
	return QObject::eventFilter(watched, event);
}
