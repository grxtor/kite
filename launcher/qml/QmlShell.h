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

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <map>
#include <memory>
#include <utility>

class IdSelectionModel;
class InstanceFilterModel;
class InstanceDetails;
class InstanceExportController;
class ContentDrop;
class InstanceShortcutController;
class SettingsAdapter;
class ModrinthModpackModel;
class FlameModpackModel;
class FtbModpackModel;
class AtlModpackModel;
class TaskWatcher;
class TechnicModpackModel;
class AccountsController;
class NewInstanceController;
class JavaController;
class JavaDownloadController;
class FolderSettingsController;
class UpdateSettingsController;
class LibraryController;
class LauncherLogs;
class BundleCatalog;
class FeaturedServers;
class FriendsController;
class ShareCodesController;
class ServerPinger;
class CatPackList;
class RecentWorldsModel;
class JavaInstallList;
class TranslationsModel;
class AboutInfo;
class NewsModel;
class QmlUiHost;
class UiHost;
class QQmlApplicationEngine;
class QQuickWindow;
class QWindow;
class QEvent;

/* Newlines become spaces, then the string is trimmed; empty means no usable
 * name. */
QString sanitizedInstanceName(const QString& name);

struct JoinChoice
{
	QString version;
	qint64 lastLaunch = 0;
	QString name;
};
/* Indexes into @p choices of those on exactly @p version, most recently played
 * first, then by name. Empty for an empty version. */
QList<int> joinMatches(const QList<JoinChoice>& choices, const QString& version);
/* Indexes of the other known versions: the release line of @p version (1.21.x
 * for 1.21.4) first, each group ordered as above. */
QList<int> joinOthers(const QList<JoinChoice>& choices, const QString& version);

/* The onboarding rules, free-standing so they can be tested without a
 * LauncherContext. */
bool languageSetupStepNeeded(const QString& language);
/* The callers compute the three flags from live state. With @p autoDownload on,
 * a missing Java path alone does not bring the step back. */
bool javaSetupStepNeeded(bool hostnameChanged, bool javaPathResolves,
						 bool autoDownload);

/*
 * The QML user interface: owns the engine, hands the core's models to it and
 * loads the root window.
 *
 * Everything QML sees is a required property of the root window, not a
 * context property, so expose() is the one place ownership is decided.
 */
class QmlShell : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString accountName READ accountName NOTIFY accountChanged)
	Q_PROPERTY(QString accountKind READ accountKind NOTIFY accountChanged)
	/// Image url of the default account's skin face; empty without one.
	Q_PROPERTY(QString accountFace READ accountFace NOTIFY accountChanged)
	Q_PROPERTY(int accountCount READ accountCount NOTIFY accountChanged)
	Q_PROPERTY(QObject* settings READ settings CONSTANT)
	/// Null until load() has run.
	Q_PROPERTY(QObject* uiHost READ uiHost CONSTANT)
	/// Installed memory in MiB: the ceiling for the memory settings.
	Q_PROPERTY(int systemMemoryMiB READ systemMemoryMiB CONSTANT)
	Q_PROPERTY(QObject* modpackModel READ modpackModel CONSTANT)
	Q_PROPERTY(QObject* curseForgeModpackModel READ curseForgeModpackModel CONSTANT)
	Q_PROPERTY(QObject* ftbModpackModel READ ftbModpackModel CONSTANT)
	Q_PROPERTY(QObject* atlModpackModel READ atlModpackModel CONSTANT)
	Q_PROPERTY(QObject* technicModpackModel READ technicModpackModel CONSTANT)
	/// Played instances, most recently played first.
	Q_PROPERTY(QObject* recentModel READ recentModel CONSTANT)
	/// At most one row: the instance whose id QML writes into instanceId.
	Q_PROPERTY(QObject* heroModel READ heroModel CONSTANT)
	/// Like heroModel, independent of the library's hero.
	Q_PROPERTY(QObject* instancePageModel READ instancePageModel CONSTANT)
	Q_PROPERTY(QObject* recentWorlds READ recentWorlds CONSTANT)
	Q_PROPERTY(QObject* accountsController READ accountsController CONSTANT)
	Q_PROPERTY(QObject* newInstance READ newInstance CONSTANT)
	Q_PROPERTY(QObject* javaController READ javaController CONSTANT)
	Q_PROPERTY(QObject* javaDownload READ javaDownload CONSTANT)
	Q_PROPERTY(QObject* folderSettings READ folderSettings CONSTANT)
	Q_PROPERTY(QObject* updateSettings READ updateSettings CONSTANT)
	Q_PROPERTY(QObject* library READ library CONSTANT)
	Q_PROPERTY(QObject* launcherLogs READ launcherLogs CONSTANT)
	Q_PROPERTY(QObject* catPacks READ catPacks CONSTANT)
	Q_PROPERTY(QObject* bundles READ bundles CONSTANT)
	/// Null in a build without a friends server; contacts nothing while
	/// FriendsEnabled is off.
	Q_PROPERTY(QObject* friends READ friends CONSTANT)
	/// Share codes of instances (ShareCodesController); null without friends.
	Q_PROPERTY(QObject* shareCodes READ shareCodes CONSTANT)
	/// Live status of Minecraft servers (ServerPinger), shared by every view.
	Q_PROPERTY(QObject* serverPinger READ serverPinger CONSTANT)
	/// The big servers offered for one-click joining (FeaturedServers).
	Q_PROPERTY(QObject* featuredServers READ featuredServers CONSTANT)
	/// Distinct groups in use, derived like the library's section headers.
	Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)
	/// Icon grid model; roles `key`, `name`, `isBuiltin`.
	Q_PROPERTY(QObject* iconsModel READ iconsModel CONSTANT)
	Q_PROPERTY(QString iconsDir READ iconsDir CONSTANT)
	/* Bumps whenever any icon's pixels may have changed. QML caches
	 * image://instanceicon/<key> by URL, so Icons.instanceIcon() appends this
	 * as `?rev=`. */
	Q_PROPERTY(int iconRevision READ iconRevision NOTIFY iconRevisionChanged)
	/// Bumps when an icon colour asked for with iconColor() arrived or changed.
	Q_PROPERTY(int iconColorRevision READ iconColorRevision NOTIFY iconColorRevisionChanged)
	/// Instance id -> built-in icon given while its own is the generic one
	/// (the UiAutoIcons setting).
	Q_PROPERTY(QVariantMap autoIcons READ autoIcons NOTIFY autoIconsChanged)

	/// Ids of the onboarding steps still needed, in order: "language", "java".
	Q_PROPERTY(QStringList setupSteps READ setupSteps NOTIFY setupStepsChanged)
	/// Roles `languageKey`, `name` (native name), `completeness`.
	Q_PROPERTY(QObject* languages READ languages CONSTANT)
	/// Roles include `path`, `version`, `architecture`, `recommended`.
	Q_PROPERTY(QObject* javaInstalls READ javaInstalls CONSTANT)
	Q_PROPERTY(bool javaDetecting READ javaDetecting NOTIFY javaDetectingChanged)

	Q_PROPERTY(QObject* about READ about CONSTANT)
	/// Null without an application NewsChecker.
	Q_PROPERTY(QObject* news READ news CONSTANT)
	/// Set by Application, which owns the updater.
	Q_PROPERTY(bool updaterAvailable READ updaterAvailable NOTIFY
				   updaterAvailableChanged)

  public:
	explicit QmlShell(QObject* parent = nullptr);
	~QmlShell() override;

	/* Builds the models and loads the root window without showing it. Does
	 * nothing once loaded. False if the QML failed to load (already logged). */
	bool load();

	/* Loads if needed, shows the window on the first call and raises it on
	 * later ones. False if the QML failed to load. */
	bool show(bool minimized = false);

	/// The root window, or nullptr before load() has succeeded.
	QWindow* window() const;

	/* The engine takes ownership of any parentless QObject that crosses into
	 * JavaScript and would delete core models; this pins them. */
	static QObject* expose(QObject* object);

	/* Installed by Application: PluginManager and PluginSurfaceModel live in
	 * Kite_logic, which links Kite_qml and not the other way around. Returns a
	 * C++-owned model for (anchor, anchorContext), or nullptr without a
	 * factory. */
	using PluginSurfaceFactory =
		std::function<QObject*(int anchor, const QString& anchorContext)>;
	static void setPluginSurfaceFactory(PluginSurfaceFactory factory);

	/* Installed by Application (the profilers live outside Kite_qml). One
	 * {key, name, ready} map per profiler, `ready` meaning its check() passes.
	 * Asked fresh by every launchOptions(), since the settings can change. */
	using ProfilersProvider = std::function<QVariantList()>;
	void setProfilersProvider(ProfilersProvider provider);

	/* Installed by Application (PluginListModel lives in Kite_logic). Returns
	 * null in a build without the plugin host. */
	using PluginListFactory = std::function<QObject*()>;
	void setPluginListFactory(PluginListFactory factory);
	/* Made by that factory on first use and kept; null without one, which
	 * Settings takes as "no plugins section". */
	Q_INVOKABLE QObject* pluginList();

	/* One C++-owned model per (anchor, anchorContext), reused across calls.
	 * `anchor` is an MMCOUiAnchor value, or -1 for every anchor;
	 * `anchorContext` is an instance id, or empty for GLOBAL_SETTINGS / every
	 * context. */
	Q_INVOKABLE QObject* pluginSurfaces(int anchor,
										const QString& anchorContext = QString());

	QString accountName() const;
	/// "Microsoft", "Offline", or empty without a default account.
	QString accountKind() const;
	int accountCount() const;
	QString accountFace() const;

	QObject* settings() const;
	/* Applies the Proxy* settings now, without a restart. QML writes them
	 * through `settings` first. */
	Q_INVOKABLE void applyProxySettings();
	QObject* uiHost() const;
	/* Same object as uiHost(). Null until load() has run and
	 * QmlUiHost::presenterReady() is true. */
	UiHost* uiHostInterface() const;
	int systemMemoryMiB() const;
	QObject* modpackModel() const;
	QObject* curseForgeModpackModel() const;
	QObject* ftbModpackModel() const;
	QObject* atlModpackModel() const;
	QObject* technicModpackModel() const;
	/* Returns a C++-owned TaskWatcher: the install must outlive the page that
	 * started it. */
	Q_INVOKABLE QObject* installModpack(const QString& projectId,
										const QString& versionId,
										const QString& instanceName,
										const QString& group);
	QObject* recentModel() const;
	QObject* heroModel() const;
	QObject* instancePageModel() const;
	QObject* recentWorlds() const;
	/* The instances of one group (empty = ungrouped) that pass the search.
	 * Created on first use and kept, so a rebuilt delegate gets the same
	 * model back. */
	Q_INVOKABLE QObject* sectionModel(const QString& group);
	Q_INVOKABLE bool iconFillsTile(const QString& key) const;
	/* At most one is kept: another id replaces (and destroys) the open one,
	 * the same id returns it. Null if @p id names no instance. */
	Q_INVOKABLE QObject* instanceDetails(const QString& id);
	/// Kept like instanceDetails().
	Q_INVOKABLE QObject* exportController(const QString& id);
	Q_INVOKABLE QObject* shortcutController(const QString& id);
	/* Files and links dropped on instance @p id's tile, without opening its
	 * page. One is kept per instance so a download started by a drop outlives
	 * the next one; null if @p id names no Minecraft instance. */
	Q_INVOKABLE QObject* contentDrop(const QString& id);
	QObject* accountsController() const;
	QObject* newInstance() const;
	QObject* javaController() const;
	QObject* javaDownload() const;
	QObject* folderSettings() const;
	QObject* updateSettings() const;
	/// Typed for Application, which hands it the updater.
	UpdateSettingsController* updateSettingsController() const;
	QObject* library() const;
	QObject* launcherLogs() const;
	QObject* catPacks() const;
	QObject* bundles() const;
	QObject* friends() const;
	QObject* shareCodes() const;
	QObject* serverPinger() const;
	QObject* featuredServers() const;

	/* The actions below emit the matching *Requested() signal; Application
	 * connects the real work, which Kite_qml cannot see. */
	Q_INVOKABLE void launchInstance(const QString& id);
	/* @p mode: "offline" (no sign-in, the account cache as it is), "demo"
	 * (refused by Application where launchOptions() says supportsDemo is
	 * false) or anything else for a normal launch. */
	Q_INVOKABLE void launchInstanceMode(const QString& id, const QString& mode);
	/* Launches with account @p accountId (an `accounts` id from
	 * launchOptions()) without making it the default. */
	Q_INVOKABLE void launchInstanceAs(const QString& id,
									  const QString& accountId);
	/* Read fresh on each call; empty for an unknown id.
	 *   supportsDemo  whether the game version has a demo (1.3.1 and newer)
	 *   profiler      the instance's profiler key ("" for none)
	 *   profilers     [{key, name, ready}], see setProfilersProvider()
	 *   accounts      [{id, name, kind, isDefault}] for "Play as"
	 *   performance   whether Kite Performance is on for the instance
	 *   performanceAvailable  the instance has a mod loader and a game
	 *                 version the mods exist for */
	Q_INVOKABLE QVariantMap launchOptions(const QString& id) const;
	/// Sets the profiler key ("" for none); starts nothing.
	Q_INVOKABLE void setInstanceProfiler(const QString& id,
										 const QString& profilerKey);
	/* Kite Performance, a mode of one instance: while on, every Play first
	 * installs the Performance bundle's default mods, minus the Modrinth
	 * project ids in performanceExcluded(), plus those in performanceIncluded()
	 * (extras the player ticked). An instance without a mod loader is left as
	 * it is (see createPerformanceInstance()). The setters do nothing for an
	 * unknown or non-Minecraft id and emit performanceChanged(). */
	Q_INVOKABLE bool performanceEnabled(const QString& id) const;
	Q_INVOKABLE void setPerformanceEnabled(const QString& id, bool on);
	Q_INVOKABLE QStringList performanceExcluded(const QString& id) const;
	Q_INVOKABLE void setPerformanceExcluded(const QString& id,
											const QStringList& projects);
	Q_INVOKABLE QStringList performanceIncluded(const QString& id) const;
	Q_INVOKABLE void setPerformanceIncluded(const QString& id,
											const QStringList& projects);
	/* Names of the mods the last Minecraft version or loader change found no
	 * build for (left at their old files, which the game will likely refuse)
	 * that are still in the mods folder: the name each was installed under,
	 * else its file name. Empty for an unknown id or when all matched.
	 * Announced by performanceUnmatchedFound() once launching or speeding up
	 * the instance has begun. */
	Q_INVOKABLE QStringList performanceUnmatched(const QString& id) const;
	/* Releases from 1.16 on and numbered versions (26.1.2); not 1.12.2,
	 * 1.8.9, snapshots or betas. */
	Q_INVOKABLE bool performanceSupported(const QString& mcVersion) const;
	/* The Graphics API setting of 26.2+ in the instance's options.txt:
	 * "default" (Automatic), "vulkan" or "opengl"; "" for a non-Minecraft
	 * instance or a version without it. */
	Q_INVOKABLE QString graphicsBackend(const QString& id) const;
	/* Writes it back, other lines kept. False, nothing changed, for an
	 * unknown id or @p backend, a version without the setting, a running game
	 * (it rewrites options.txt on exit) or a failed write. */
	Q_INVOKABLE bool setGraphicsBackend(const QString& id,
										const QString& backend);
	/* "Speed up" on a modded instance: turns the mode on and does the setup
	 * now. Returns its TaskWatcher, or null (nothing changed) for a running,
	 * non-Minecraft or loaderless instance. The setup never fails; the
	 * finished watcher's `status` is a sentence for a toast. While one setup
	 * runs on the instance, returns its watcher instead of starting another.
	 * @p extras are other catalog projects for this run only. */
	Q_INVOKABLE QObject* speedUp(const QString& id, const QStringList& extras = {});
	/// The watcher of the speedUp() setup still running on @p id, or null.
	Q_INVOKABLE QObject* performanceSetup(const QString& id) const;
	/* "Speed up" on a vanilla instance, which is never changed: makes a new
	 * empty instance "<source> · Kite Performance" on the source's Minecraft
	 * version with the newest Fabric, in the source's group, with the
	 * Performance preset's memory and options.txt and the mode on, then
	 * installs the mods into it.
	 * One TaskWatcher covers the chain: its `instanceId` is the new
	 * instance's by the time finished(true) fires and its final `status` is a
	 * toast sentence. finished(false) with `error` if the instance could not
	 * be created (the mods are best effort). Null unless @p sourceId is a
	 * Minecraft instance whose version is performanceSupported(). */
	Q_INVOKABLE QObject* createPerformanceInstance(const QString& sourceId);
	Q_INVOKABLE void killInstance(const QString& id);
	Q_INVOKABLE void openInstanceFolder(const QString& id);
	/* Launches straight into server @p address (host[:port]). False if the
	 * instance cannot start: a running or broken one only gets its Log or
	 * edit page (Application::launch()). */
	Q_INVOKABLE bool joinServer(const QString& id, const QString& address);
	/* The Minecraft instances on exactly @p mcVersion, most recently played
	 * first: [{id, name, iconKey, version, loader, lastLaunch, running,
	 * canLaunch}]. Empty for an empty version. */
	Q_INVOKABLE QVariantList instancesForVersion(const QString& mcVersion) const;
	/* The other Minecraft instances, same maps: the release line of
	 * @p mcVersion (1.21.x for 1.21.4) first, each group most recently played
	 * first. */
	Q_INVOKABLE QVariantList otherInstances(const QString& mcVersion) const;
	/* Every Minecraft instance, same maps plus `fits`: whether a server
	 * taking @p minVersion to @p maxVersion and @p lines
	 * (FeaturedServers::versionFits) lets its version in. Those that fit
	 * first, each group most recently played first. */
	Q_INVOKABLE QVariantList instancesForServer(const QString& minVersion,
												const QString& maxVersion,
												const QStringList& lines) const;
	/// Relative paths resolve against the data folder (the working directory).
	Q_INVOKABLE void openPath(const QString& path);

	/* For an import that comes from outside the window (`kite --import`, the
	 * IPC "import" message). Relays @p source (a local path or URL) as
	 * importRequested(); kept until load() succeeds if QML cannot answer
	 * yet. */
	void requestImport(const QString& source);

	/* Relays @p target, a Main.qml navigate() path ("accounts",
	 * "settings/java"), as navigateRequested(). */
	void requestNavigate(const QString& target);

	/// Relays as openInstanceLog(): instance @p id's page on its Log tab.
	void showInstanceLogRequested(const QString& id);

	/// False (no change) for a name that trims to nothing.
	Q_INVOKABLE bool renameInstance(const QString& id, const QString& name);
	/// "" ungroups.
	Q_INVOKABLE void setInstanceGroup(const QString& id,
									  const QString& group);
	Q_INVOKABLE void setInstanceIcon(const QString& id,
									 const QString& iconKey);
	// Copies a screenshot (path or file URL) into the instance as its cover; false for an unknown instance.
	Q_INVOKABLE bool setInstanceCover(const QString& instanceId,
									  const QString& pathOrFileUrl);
	// Drops the picked cover and deletes its copy.
	Q_INVOKABLE void clearInstanceCover(const QString& instanceId);
	// The screenshot the cover was copied from got a new name (path or name).
	Q_INVOKABLE void renameInstanceCoverSource(const QString& instanceId,
											   const QString& fileName);
	/* The average colour of instance icon @p key for the cover-less gradient;
	 * invalid until it is averaged (iconColorRevision then bumps). */
	Q_INVOKABLE QColor iconColor(const QString& key) const;
	/* Copies saves and keeps playtime, with the source's icon. Returns a
	 * C++-owned TaskWatcher, or null if @p id names no instance or @p newName
	 * trims to nothing. */
	Q_INVOKABLE QObject* duplicateInstance(const QString& id,
										   const QString& newName,
										   const QString& group);
	/* QML confirms first. Refuses (false) while the instance is running. */
	Q_INVOKABLE bool deleteInstance(const QString& id);
	/* Dev only: sets BaseInstance::setCrashed()'s runtime flag, which is never
	 * written to instance.cfg, for KITE_QML_ROUTE snapshots ("crashed=<id>").
	 * No-op for an unknown id. */
	Q_INVOKABLE void debugMarkInstanceCrashed(const QString& id);
	/* Dev only: marks @p id crashed and has the crash doctor read its last
	 * run's files, for KITE_QML_ROUTE snapshots: "crashdoctor=<id>" as a crash
	 * read again later, "crashwindow=<id>" as one that just happened (which
	 * opens the "why" window). */
	Q_INVOKABLE void debugAnalyseCrash(const QString& id);
	Q_INVOKABLE void debugCrashNow(const QString& id);
	/* Dev only, for the crash reports' end-to-end run: "crashsend=<id>" sends
	 * that instance's analysed crash to FriendsServerUrl as Send would;
	 * "crashrules=<file>" uses that crash-rules.json as if the meta server had
	 * sent it, which reads kept crashes again. */
	Q_INVOKABLE void debugSendCrash(const QString& id);
	Q_INVOKABLE void debugUseCrashRules(const QString& path);
	/* Dev only: "crashos=windows" reads crashes as if the game ran on that
	 * system, for snapshots of Windows-only causes and fixes. */
	Q_INVOKABLE void debugUseCrashOs(const QString& os);
	/* Dev only: a real mouse click at window point "x,y" for review snapshots
	 * ("click=x,y[,right[,delayMs]]"). The pointer moves there first so
	 * hover-only controls show; delayMs defaults to 1000.
	 * "x,y,left,delayMs,dx,dy" drags by (dx, dy) before letting go;
	 * "x,y,hover[,delayMs]" only moves the pointer there (a tooltip). */
	Q_INVOKABLE void debugClick(const QString& spec);
	/* Dev only, for snapshots ("ask=<kind>"): "pickAccount" asks the launch
	 * account picker; "busySkip" shows the account-refresh wait with its "Play
	 * offline" button; "modal" a sample plugin prompt; "optionalMods" a sample
	 * ATLauncher optional-mods question. No-op for any other kind. */
	Q_INVOKABLE void debugShowRequest(const QString& kind);

	QStringList groups() const;
	QObject* iconsModel() const;
	QString iconsDir() const;

	QStringList setupSteps() const;
	int iconRevision() const { return m_iconRevision; }
	int iconColorRevision() const { return m_iconColorRevision; }
	QVariantMap autoIcons() const { return m_autoIcons; }
	/// Recomputes setupSteps() from live settings once QML finishes a step.
	Q_INVOKABLE void finishSetupStep(const QString& id);
	QObject* languages() const;
	/// Applies @p key live, retranslates the QML engine and persists it.
	Q_INVOKABLE void selectLanguage(const QString& key);
	QObject* javaInstalls() const;
	bool javaDetecting() const;
	/// Starts a detection run unless one is already in progress.
	Q_INVOKABLE void detectJava();
	/* Writes JavaPath. @p path should be one of javaInstalls()'s rows; it is
	 * not re-checked. JavaVersion/JavaArchitecture are left to CheckJava. */
	Q_INVOKABLE void useJava(const QString& path);

	QObject* about() const;
	QObject* news() const;
	bool updaterAvailable() const;
	void setUpdaterAvailable(bool available);
	Q_INVOKABLE void checkForUpdates();

  signals:
	/// The root window really closed, not merely hidden (a plugin's hide or
	/// close-filter veto does not fire it).
	void closed();

	void accountChanged();

	void launchRequested(const QString& id);
	void launchModeRequested(const QString& id, const QString& mode);
	void launchAsRequested(const QString& id, const QString& accountId);
	void killRequested(const QString& id);
	void performanceChanged(const QString& id);
	/// Instance @p id's Kite Performance setup found no version of the mods
	/// @p names (performanceUnmatched()) for its new Minecraft version or
	/// loader. Only for an instance this shell has launched or sped up.
	void performanceUnmatchedFound(const QString& id, const QStringList& names);
	/// The crash doctor read instance @p id's crash; @p cause is its first
	/// cause's title, "" when it found none. @p fresh: the crash of a launch
	/// that just ended (not one read again later).
	void crashAnalysed(const QString& id, const QString& cause, bool fresh);
	void folderRequested(const QString& id);
	void joinServerRequested(const QString& id, const QString& address);

	void importRequested(const QString& source);
	void navigateRequested(const QString& target);
	void openInstanceLog(const QString& id);

	void groupsChanged();
	void iconRevisionChanged();
	void iconColorRevisionChanged();
	void autoIconsChanged();

	void setupStepsChanged();
	void javaDetectingChanged();

	void updaterAvailableChanged();
	void checkForUpdatesRequested();

  private:
	QVariantMap rootProperties();
	void scheduleSnapshotIfRequested();
	void recomputeSetupSteps();

	/* Installed on m_window by load(); emits closed() on QEvent::Close. */
	bool eventFilter(QObject* watched, QEvent* event) override;

	/* Declared before the engine so they are destroyed after it: QML holds
	 * pointers to both until the engine is gone. */
	std::unique_ptr<InstanceFilterModel> m_instances;
	std::unique_ptr<InstanceFilterModel> m_recent;
	std::unique_ptr<InstanceFilterModel> m_hero;
	std::unique_ptr<InstanceFilterModel> m_instancePage;
	std::unique_ptr<RecentWorldsModel> m_recentWorlds;
	// Declared after m_instances, their source, so they are destroyed first.
	std::map<QString, std::unique_ptr<InstanceFilterModel>> m_sections;
	std::unique_ptr<IdSelectionModel> m_selection;
	std::unique_ptr<SettingsAdapter> m_settings;
	std::unique_ptr<QmlUiHost> m_uiHost;
	std::unique_ptr<ModrinthModpackModel> m_modpacks;
	std::unique_ptr<FlameModpackModel> m_curseForgeModpacks;
	std::unique_ptr<FtbModpackModel> m_ftbModpacks;
	std::unique_ptr<AtlModpackModel> m_atlModpacks;
	std::unique_ptr<TechnicModpackModel> m_technicModpacks;
	std::unique_ptr<InstanceDetails> m_instanceDetails;
	std::unique_ptr<InstanceExportController> m_exportController;
	std::map<QString, std::unique_ptr<ContentDrop>> m_contentDrops;
	std::unique_ptr<InstanceShortcutController> m_shortcutController;
	std::unique_ptr<AccountsController> m_accountsController;
	mutable std::unique_ptr<NewInstanceController> m_newInstance;
	mutable std::unique_ptr<JavaController> m_javaController;
	mutable std::unique_ptr<JavaDownloadController> m_javaDownload;
	mutable std::unique_ptr<FolderSettingsController> m_folderSettings;
	mutable std::unique_ptr<UpdateSettingsController> m_updateSettings;
	mutable std::unique_ptr<LibraryController> m_library;
	mutable std::unique_ptr<LauncherLogs> m_launcherLogs;
	mutable std::unique_ptr<CatPackList> m_catPacks;
	mutable std::unique_ptr<BundleCatalog> m_bundles;
	mutable std::unique_ptr<FriendsController> m_friends;
	mutable std::unique_ptr<ShareCodesController> m_shareCodes;
	mutable std::unique_ptr<ServerPinger> m_serverPinger;
	mutable std::unique_ptr<FeaturedServers> m_featuredServers;
	/* Keyed by (anchor, anchorContext). Declared with the other cached models
	 * so they are destroyed before the engine. */
	std::map<std::pair<int, QString>, std::unique_ptr<QObject>>
		m_pluginSurfaceModels;
	ProfilersProvider m_profilersProvider;
	PluginListFactory m_pluginListFactory;
	std::unique_ptr<QObject> m_pluginList;
	std::unique_ptr<AboutInfo> m_about;
	std::unique_ptr<NewsModel> m_news;

	std::unique_ptr<QQmlApplicationEngine> m_engine;
	QQuickWindow* m_window = nullptr;
	int m_accountRevision = 0;
	/* Running speedUp() setups by instance id; dropped when their watcher
	 * finishes. */
	QHash<QString, QPointer<TaskWatcher>> m_performanceSetups;
	/* Starts announcing instance @p id's unmatched Kite Performance mods
	 * (performanceUnmatchedFound()) once per instance; called wherever the
	 * shell launches or speeds one up, since the setup runs inside that. */
	void watchPerformance(const QString& id);
	QSet<QString> m_watchedPerformance;

	QStringList m_setupSteps;
	/* Set when the hostname differs from the recorded one; keeps the "java"
	 * step until finishSetupStep("java"). */
	bool m_hostnameChanged = false;

	/* Bumped once per event-loop turn however many iconUpdated signals
	 * arrived, so every bound Image reloads once. */
	int m_iconRevision = 0;
	bool m_iconRevisionPending = false;
	int m_iconColorRevision = 0;
	/* Recomputed once per event-loop turn after the instance list gains or
	 * loses rows or an icon key changes. */
	void refreshAutoIcons();
	QVariantMap m_autoIcons;
	bool m_autoIconsPending = false;
	bool m_javaDetecting = false;

	/* requestImport() sources that arrived before load(); emitted in order
	 * once it has loaded the root window. */
	QStringList m_pendingImports;

	bool m_updaterAvailable = false;
	bool m_shown = false;
};
