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
class InstanceShortcutController;
class SettingsAdapter;
class ModrinthModpackModel;
class FlameModpackModel;
class FtbModpackModel;
class AtlModpackModel;
class TechnicModpackModel;
class AccountsController;
class NewInstanceController;
class JavaController;
class JavaDownloadController;
class FolderSettingsController;
class UpdateSettingsController;
class PlayTimeSummary;
class LibraryController;
class LauncherLogs;
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

/* Same trim rule the widget's inline rename editor applies before
 * committing (see ui/instanceview/InstanceDelegate.cpp's setModelData()):
 * embedded newlines become spaces, then the whole string is trimmed. An
 * empty result means "no usable name".
 *
 * Free-standing rather than a QmlShell member so it can be unit-tested
 * without a LauncherContext, the way NewInstanceController.h's
 * composeSuggestedInstanceName() is. */
QString sanitizedInstanceName(const QString& name);

/* The onboarding rules Application::createSetupWizard() used to gate the
 * widget SetupWizard's pages (LanguageWizardPage/JavaWizardPage) -- now
 * used by QmlShell::recomputeSetupSteps() instead, since the QML shell runs
 * its own onboarding rather than showing that widget on top of itself (see
 * the class comment below). Free-standing for the same reason as
 * sanitizedInstanceName() above: testable without a LauncherContext. */
bool languageSetupStepNeeded(const QString& language);
/* @p hostnameChanged: the machine's hostname no longer matches the
 * "LastHostname" setting recorded on a previous run -- same trigger the
 * widget wizard used (a new machine, or a rename, may mean Java moved or
 * vanished). @p javaPathResolves: FS::ResolveExecutable() on the "JavaPath"
 * setting found something. Callers compute both against live state; this
 * function only combines them, so it needs neither Qt network calls
 * (QHostInfo) nor filesystem access to test. */
bool javaSetupStepNeeded(bool hostnameChanged, bool javaPathResolves);

/*
 * The QML user interface: owns the engine, hands the core's models to it and
 * loads the root window.
 *
 * Everything the QML side sees is passed in as a required property of the
 * root window rather than set as a context property. That keeps each object's
 * type visible to the QML tooling, makes a missing one a load error instead of
 * a silent undefined, and leaves exactly one place -- expose() -- where
 * ownership is decided.
 */
class QmlShell : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString accountName READ accountName NOTIFY accountChanged)
	Q_PROPERTY(QString accountKind READ accountKind NOTIFY accountChanged)
	/// Image url of the default account's skin face; empty without one.
	Q_PROPERTY(QString accountFace READ accountFace NOTIFY accountChanged)
	Q_PROPERTY(int accountCount READ accountCount NOTIFY accountChanged)
	/// The launcher's settings, as a SettingsAdapter.
	Q_PROPERTY(QObject* settings READ settings CONSTANT)
	/* The core's UiHost, when it is this shell's QmlUiHost -- see
	 * Application::uiHost(), which prefers this over the widget host for
	 * as long as it is non-null. QML binds to `current`/`busy`/`busyText`
	 * on it (see QmlUiHost's class comment). Null until load() has run. */
	Q_PROPERTY(QObject* uiHost READ uiHost CONSTANT)
	/// Installed memory in MiB: the ceiling for the memory settings.
	Q_PROPERTY(int systemMemoryMiB READ systemMemoryMiB CONSTANT)
	/// Modrinth modpack search for the Discover page.
	Q_PROPERTY(QObject* modpackModel READ modpackModel CONSTANT)
	/// CurseForge modpack search for the Discover page's provider switch.
	Q_PROPERTY(QObject* curseForgeModpackModel READ curseForgeModpackModel CONSTANT)
	/// FTB (modpacks.ch), ATLauncher and Technic modpack search for Discover.
	Q_PROPERTY(QObject* ftbModpackModel READ ftbModpackModel CONSTANT)
	Q_PROPERTY(QObject* atlModpackModel READ atlModpackModel CONSTANT)
	Q_PROPERTY(QObject* technicModpackModel READ technicModpackModel CONSTANT)
	/// Every instance, most recently played first; never-played ones left out.
	Q_PROPERTY(QObject* recentModel READ recentModel CONSTANT)
	/// At most one row: the instance whose id QML writes into instanceId.
	Q_PROPERTY(QObject* heroModel READ heroModel CONSTANT)
	/// Same, for the instance page -- separate, since the library's hero
	/// keeps following its own choice underneath.
	Q_PROPERTY(QObject* instancePageModel READ instancePageModel CONSTANT)
	/// The Home page's "Recent worlds" row -- most recently played worlds
	/// across every instance (see RecentWorldsModel's class comment).
	Q_PROPERTY(QObject* recentWorlds READ recentWorlds CONSTANT)
	/// Accounts page: the AccountList model plus add/remove/default/login.
	Q_PROPERTY(QObject* accountsController READ accountsController CONSTANT)
	/// The QML "New instance" flow: picks a Minecraft version and loader.
	Q_PROPERTY(QObject* newInstance READ newInstance CONSTANT)
	/// Java detection, tests and downloaded runtimes (see models/JavaController.h).
	Q_PROPERTY(QObject* javaController READ javaController CONSTANT)
	/// The "Download Java" flow (see models/JavaDownloadController.h).
	Q_PROPERTY(QObject* javaDownload READ javaDownload CONSTANT)
	/// Settings' folder rows (see models/FolderSettingsController.h).
	Q_PROPERTY(QObject* folderSettings READ folderSettings CONSTANT)
	/// Settings' Updates section (see models/UpdateSettingsController.h).
	Q_PROPERTY(QObject* updateSettings READ updateSettings CONSTANT)
	/// Play time across every instance (see models/PlayTimeSummary.h).
	Q_PROPERTY(QObject* playTime READ playTime CONSTANT)
	/// The library's housekeeping: groups, undo delete, duplicate with
	/// options, an instance's folders. See models/LibraryController.h.
	Q_PROPERTY(QObject* library READ library CONSTANT)
	/// Settings' Launcher logs section: the launcher's own log files and
	/// their uploader (see models/LauncherLogs.h).
	Q_PROPERTY(QObject* launcherLogs READ launcherLogs CONSTANT)
	/* Distinct groups currently in use, for a "move to group" picker's
	 * suggestions -- the same list InstanceFilterModel derives for the
	 * library's own section headers (see m_instances), so a group picker
	 * never suggests something the library itself would not show. */
	Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)
	/// The icon grid model for an icon picker; roles are `key`, `name`,
	/// `isBuiltin` (see IconList::roleNames()).
	Q_PROPERTY(QObject* iconsModel READ iconsModel CONSTANT)
	/// Where installed icons live, for a picker's "open folder" action.
	Q_PROPERTY(QString iconsDir READ iconsDir CONSTANT)

	/* Onboarding: the widget SetupWizard (LanguageWizardPage/JavaWizardPage)
	 * used to run before the main window -- including under the QML shell,
	 * which this replaces. See recomputeSetupSteps() for the rules, unchanged
	 * from Application::createSetupWizard(). */
	/// Ids of the wizard steps still needed, in the widget wizard's own
	/// order: "language", then "java". Empty once nothing is needed.
	Q_PROPERTY(QStringList setupSteps READ setupSteps NOTIFY setupStepsChanged)
	/// The language picker's model; roles are `languageKey`, `name` (native
	/// name), `completeness` (see TranslationsModel::roleNames()).
	Q_PROPERTY(QObject* languages READ languages CONSTANT)
	/// Detected Java installs; roles include `path`, `version`,
	/// `architecture`, `recommended` (see BaseVersionList::roleNames(), which
	/// JavaInstallList inherits).
	Q_PROPERTY(QObject* javaInstalls READ javaInstalls CONSTANT)
	/// Whether detectJava()'s task is still running.
	Q_PROPERTY(bool javaDetecting READ javaDetecting NOTIFY javaDetectingChanged)

	/// Settings' About section: version, build, credits, licences, help
	/// links (see AboutInfo). Created in load().
	Q_PROPERTY(QObject* about READ about CONSTANT)
	/* The launcher's news, newest first (see NewsModel), following the
	 * application's NewsChecker; null without one. Created in load(). */
	Q_PROPERTY(QObject* news READ news CONSTANT)
	/* Whether this build can update itself, so "Check for updates" has
	 * something to do -- set by Application, which owns the updater. */
	Q_PROPERTY(bool updaterAvailable READ updaterAvailable NOTIFY
				   updaterAvailableChanged)

  public:
	explicit QmlShell(QObject* parent = nullptr);
	~QmlShell() override;

	/* Builds the models and loads the root window without showing it --
	 * a launch straight from the command line (`meshmc --launch`) keeps the
	 * window hidden until it has something to show (see
	 * Application::ensureQmlShell()). Does nothing once loaded. Returns
	 * false if the QML failed to load, in which case the reasons have
	 * already been logged. */
	bool load();

	/* Loads the root window if needed and shows it on first call; raises
	 * it on later ones. Returns false if the QML failed to load, as load()
	 * does. */
	bool show(bool minimized = false);

	/* The root window load() created, or nullptr before it has
	 * succeeded. PluginManager (MeshMC_logic, which links MeshMC_qml --
	 * see setPluginSurfaceFactory()'s comment) uses this through
	 * Application::qmlShellWindow() to generalise main-window handling
	 * (show/hide/close-filter) to the QML shell the same way it already
	 * does for the widget MainWindow. */
	QWindow* window() const;

	/* Hands a C++-owned object to QML. The engine takes ownership of any
	 * QObject without a parent that crosses into JavaScript, and would delete
	 * core models out from under the rest of the launcher; this pins them. */
	static QObject* expose(QObject* object);

	/*
	 * QmlShell sits in MeshMC_qml, which links MeshMC_core only -- it
	 * cannot see PluginManager or PluginSurfaceModel, both of which live in
	 * MeshMC_logic (the plugin host), a target that links MeshMC_qml and
	 * not the other way around. Application installs the real factory once
	 * at startup (see Application::showMainWindow()), the same indirection
	 * launchInstance()/killInstance()/etc. use via *Requested() signals --
	 * except pluginSurfaces() needs to *return* a model built from a
	 * PluginManager, where a signal handing back a value has no natural
	 * fit, hence a factory instead. Returns a C++-owned QObject* (a
	 * PluginSurfaceModel) for the given (anchor, anchorContext), or nullptr
	 * if no factory has been installed (e.g. a test/tool binary that never
	 * wires one up).
	 */
	using PluginSurfaceFactory =
		std::function<QObject*(int anchor, const QString& anchorContext)>;
	static void setPluginSurfaceFactory(PluginSurfaceFactory factory);

	/* The launcher's profilers (Application::profilers()), which QmlShell
	 * cannot see from MeshMC_qml either: one {key, name, ready} map per
	 * profiler, `ready` meaning its own check() passes (the program is set
	 * up under Settings, External tools). Asked fresh by launchOptions()
	 * every time, since that check reads settings the user can change.
	 * Application installs it right after creating the shell. */
	using ProfilersProvider = std::function<QVariantList()>;
	void setProfilersProvider(ProfilersProvider provider);

	/* Builds the "Installed plugins" list (a PluginListModel, which lives
	 * next to PluginManager in MeshMC_logic) -- same indirection as
	 * setPluginSurfaceFactory(). Application installs it; it returns null
	 * in a build without the plugin host. */
	using PluginListFactory = std::function<QObject*()>;
	void setPluginListFactory(PluginListFactory factory);
	/* The plugin list, made by that factory on first use and kept; null
	 * without one, which Settings takes as "no plugins section". */
	Q_INVOKABLE QObject* pluginList();

	/* One C++-owned model per (anchor, anchorContext) pair asked for,
	 * reused across calls the same way sectionModel() reuses one
	 * InstanceFilterModel per group. `anchor` is an MMCOUiAnchor value, or
	 * -1 for "every anchor"; `anchorContext` is an instance id, or empty
	 * for GLOBAL_SETTINGS / "every context" (see
	 * PluginManager::surfaces()). */
	Q_INVOKABLE QObject* pluginSurfaces(int anchor,
										const QString& anchorContext = QString());

	/* Sidebar account summary, read from LAUNCHER->accounts() - QmlShell has
	 * no reason to go through Application for this, and reaching it via the
	 * core keeps QmlShell usable without one. */
	QString accountName() const;
	/// "Microsoft", "Offline", or empty when there is no default account.
	QString accountKind() const;
	int accountCount() const;
	QString accountFace() const;

	QObject* settings() const;
	/* Applies the ProxyType/ProxyAddr/ProxyPort/ProxyUser/ProxyPass
	 * settings immediately, the way the widget ProxyPage's apply button
	 * does. The QML settings page (once it has a proxy page of its own)
	 * writes those five settings through `settings` above like any other
	 * setting, then calls this so the change takes effect without a
	 * restart -- QmlShell cannot reach QNetworkProxy/Application itself,
	 * so this goes through LauncherContext::updateProxySettings(). */
	Q_INVOKABLE void applyProxySettings();
	/// Bound to `shell.uiHost` in QML -- see the Q_PROPERTY comment above.
	QObject* uiHost() const;
	/* The same object as uiHost() above, typed for Application's own use
	 * (Application::uiHost() prefers this over the widget UiHost while
	 * this is non-null) instead of QML's property binding. Null until
	 * load() has run *and* QmlUiHost::presenterReady() is true -- see
	 * QmlUiHost's class comment's PRESENTER READINESS section for why a
	 * call must not reach this object before some QML item can answer it. */
	UiHost* uiHostInterface() const;
	int systemMemoryMiB() const;
	QObject* modpackModel() const;
	QObject* curseForgeModpackModel() const;
	QObject* ftbModpackModel() const;
	QObject* atlModpackModel() const;
	QObject* technicModpackModel() const;
	/* Starts installing a Modrinth modpack version as a new instance and
	 * returns its TaskWatcher, owned by C++: the install must outlive the
	 * page that started it, whatever QML does with the reference. */
	Q_INVOKABLE QObject* installModpack(const QString& projectId,
										const QString& versionId,
										const QString& instanceName,
										const QString& group);
	QObject* recentModel() const;
	QObject* heroModel() const;
	QObject* instancePageModel() const;
	QObject* recentWorlds() const;
	/* The instances of one group (empty = ungrouped) that pass the search,
	 * for one section of the library. Created on first use and kept, so
	 * QML asking again from a rebuilt delegate gets the same model back. */
	Q_INVOKABLE QObject* sectionModel(const QString& group);
	/* The InstanceDetails bridge for one instance's detail page. At most
	 * one is kept at a time: asking for a different id replaces (and
	 * destroys) whichever one was open before; asking again for the same
	 * id returns the one already open. Null if @p id names no instance. */
	Q_INVOKABLE QObject* instanceDetails(const QString& id);
	/* The Export dialog's controller for instance @p id. At most one is
	 * kept, like instanceDetails(): asking for another id replaces it,
	 * asking again for the same id returns the one already open. Null if
	 * @p id names no instance. */
	Q_INVOKABLE QObject* exportController(const QString& id);
	/* The Create shortcut dialog's controller for instance @p id; the same
	 * keeping rule. */
	Q_INVOKABLE QObject* shortcutController(const QString& id);
	/// The Accounts page's model+actions object. Created in load(), like
	/// the other CONSTANT properties above.
	QObject* accountsController() const;
	/// The "New instance" flow's controller. Created in load(), like the
	/// other CONSTANT properties above.
	QObject* newInstance() const;
	/// Made on first use, like newInstance().
	QObject* javaController() const;
	QObject* javaDownload() const;
	/// Made on first use, like javaController().
	QObject* folderSettings() const;
	QObject* updateSettings() const;
	QObject* playTime() const;
	/* The same object as updateSettings(), typed for Application, which
	 * hands it the updater (UpdateSettingsController::setBackend()). */
	UpdateSettingsController* updateSettingsController() const;
	/// Made on first use, like newInstance().
	QObject* library() const;
	/// Made on first use: nothing watches the log folder before then.
	QObject* launcherLogs() const;

	/* Everything below just emits the matching *Requested() signal: QmlShell
	 * sits in MeshMC_qml, which cannot see the widget code that actually
	 * launches an instance, opens a dialog or shows a folder. Application
	 * connects these to the real actions. */
	Q_INVOKABLE void launchInstance(const QString& id);
	/* Play in a given way: @p mode is "offline" (no sign-in, the account
	 * cache as it is), "demo" (the demo, no account needed; refused by
	 * Application where launchOptions() says supportsDemo is false) or
	 * anything else for a normal launch -- the widget launch menu's Launch,
	 * Launch Offline and Launch Demo. */
	Q_INVOKABLE void launchInstanceMode(const QString& id, const QString& mode);
	/* Play with account @p accountId (an `accounts` id from
	 * launchOptions()) instead of the default one, without making it the
	 * default. */
	Q_INVOKABLE void launchInstanceAs(const QString& id,
									  const QString& accountId);
	/* What a "Play options" menu needs for instance @p id, read fresh each
	 * time it opens:
	 *   supportsDemo  whether the game version has a demo (1.3.1 and newer)
	 *   profiler      the instance's profiler key ("" for none)
	 *   profilers     [{key, name, ready}] -- see setProfilersProvider()
	 *   accounts      [{id, name, kind, isDefault}] for "Play as"
	 * Empty for an unknown id. */
	Q_INVOKABLE QVariantMap launchOptions(const QString& id) const;
	/* Sets instance @p id's profiler ("" for none), the setting every way
	 * into the game reads -- the widget launch menu's profiler entries.
	 * Starts nothing by itself. */
	Q_INVOKABLE void setInstanceProfiler(const QString& id,
										 const QString& profilerKey);
	Q_INVOKABLE void killInstance(const QString& id);
	Q_INVOKABLE void openInstanceFolder(const QString& id);
	/* Launches instance @p id straight into server @p address (host[:port]),
	 * the QML-facing replacement for ServersPage's "Join" action
	 * (ServersPage::on_actionJoin_triggered()). */
	Q_INVOKABLE void joinServer(const QString& id, const QString& address);
	/// Opens a folder in the file manager; relative paths are resolved
	/// against the data folder, which is the working directory.
	Q_INVOKABLE void openPath(const QString& path);

	/* Called by Application for an import that comes from outside the
	 * window -- `meshmc --import <zip|url>`, or a second launcher process
	 * passing its file on (the IPC "import" message) -- where the widget
	 * UI used MainWindow::droppedURLs(). Relays @p source (a local path or
	 * a URL) into QML as importRequested() below, which opens the New
	 * instance dialog in import mode with it filled in. Before load() has
	 * loaded the root window nothing in QML could answer that signal yet,
	 * so the source is kept and delivered as soon as load() succeeds. */
	void requestImport(const QString& source);

	/* Called by Application to send the user somewhere in the QML shell
	 * from outside it -- a launch that needs an account opens the Accounts
	 * page. @p target is a Main.qml navigate() path ("accounts",
	 * "settings/java", ...); relayed as navigateRequested() below. */
	void requestNavigate(const QString& target);

	/* Called by Application (see Application::showInstanceLog()) instead
	 * of raising a widget InstanceWindow when the QML shell is the active
	 * UI: a launch that would have opened the console -- ShowConsole, or a
	 * crash with ShowConsoleOnError -- relays the request into QML as
	 * openInstanceLog() below, which opens instance @p id's page on its
	 * Log tab. */
	void showInstanceLogRequested(const QString& id);

	/* Everything below replicates the core part of a MainWindow instance
	 * action directly against LAUNCHER->instances()/icons() -- no widget
	 * code, no dialogs (QML supplies its own and asks for confirmation
	 * itself where the widget would have). */

	/// False (no change) for a name that trims to nothing, same rule the
	/// widget's inline rename editor applies.
	Q_INVOKABLE bool renameInstance(const QString& id, const QString& name);
	/// "" ungroups; the same InstanceList API both the widget's
	/// drag-to-group and its "Change group" dialog call.
	Q_INVOKABLE void setInstanceGroup(const QString& id,
									  const QString& group);
	Q_INVOKABLE void setInstanceIcon(const QString& id,
									 const QString& iconKey);
	/* Installs a local image as a new icon, the way IconPickerDialog's
	 * "Add icon" button does (IconList::installIcons()). Returns whether
	 * the file looked installable (readable, a regular file); like the
	 * widget's own button, a same-key collision or a rejected extension is
	 * not detected here. On success, iconImported() follows once the icon
	 * list actually picks the new file up. */
	Q_INVOKABLE bool importIcon(const QString& fileUrlOrPath);
	/* Starts the same InstanceCopyTask CopyInstanceDialog starts, with the
	 * same defaults its checkboxes start with (copy saves and keep
	 * playtime, both on) and the source instance's own icon. Returns a
	 * C++-owned TaskWatcher for the running copy, or null if @p id names no
	 * instance or @p newName trims to nothing. */
	Q_INVOKABLE QObject* duplicateInstance(const QString& id,
										   const QString& newName,
										   const QString& group);
	/* The same deletion MainWindow performs once its confirmation dialog
	 * is accepted -- QML asks for confirmation itself before calling this.
	 * Refuses (false) while the instance is running, like the widget does. */
	Q_INVOKABLE bool deleteInstance(const QString& id);
	Q_INVOKABLE bool isInstanceRunning(const QString& id) const;
	/* Preview/dev-only: sets the same runtime flag LaunchTask sets around a
	 * real game process's exit (BaseInstance::setCrashed()), so a
	 * MESHMC_QML_ROUTE devRoute step (Main.qml's applyDevRoute(), "crashed=
	 * <id>") can exercise the Home page's "Crashed last time" chip. Unlike
	 * every other instance property, hasCrashed is never written to
	 * instance.cfg, so gen_preview.py's static preview data cannot fake it
	 * on disk -- this reaches the exact same in-memory flag production
	 * does instead of inventing a parallel on-disk one. No-op for an
	 * unknown id. */
	Q_INVOKABLE void debugMarkInstanceCrashed(const QString& id);
	/* Preview/dev-only, for review snapshots of questions only a real
	 * launch would otherwise ask (Main.qml's applyDevRoute(), "ask=<kind>"):
	 * "pickAccount" asks the launch account picker over the real account
	 * list; "busySkip" shows the account-refresh wait with its "Play
	 * offline" button, which ends it; "modal" shows a sample plugin prompt
	 * (UiHost::runPluginModal()); "optionalMods" a sample ATLauncher
	 * optional-mods question (UiHost::chooseOptionalMods()). Through the same QmlUiHost calls the
	 * real code makes. No-op for any other kind. */
	Q_INVOKABLE void debugShowRequest(const QString& kind);

	QStringList groups() const;
	QObject* iconsModel() const;
	QString iconsDir() const;

	QStringList setupSteps() const;
	/* Recomputes setupSteps() from live settings, exactly like
	 * Application::createSetupWizard() did once at startup for the widget
	 * wizard -- called once from the constructor, and again whenever QML
	 * says a step is done (finishSetupStep()), since finishing one step
	 * (e.g. picking a language) does not change whether another (Java) is
	 * still needed, but the shell has no other way to notice that a step it
	 * already reported is now satisfied. */
	Q_INVOKABLE void finishSetupStep(const QString& id);
	QObject* languages() const;
	/* Applies @p key live the way LanguageSelectionWidget's row-changed
	 * handler does (TranslationsModel::selectLanguage() +
	 * updateLanguage()), retranslates the running QML engine so qsTr()
	 * strings update immediately, and persists it as LanguageWizardPage's
	 * validatePage() does. */
	Q_INVOKABLE void selectLanguage(const QString& key);
	QObject* javaInstalls() const;
	bool javaDetecting() const;
	/* Starts the same JavaInstallList detection task the widget wizard's
	 * refresh button runs (JavaSettingsWidget::refresh() ->
	 * VersionSelectWidget::loadList()) -- always a fresh run, not only when
	 * nothing has been detected yet. */
	Q_INVOKABLE void detectJava();
	/* Writes JavaPath the way JavaWizardPage::validatePage() does for a
	 * good result -- @p path is expected to already be a checked candidate
	 * (one of javaInstalls()'s rows), so this does not re-run JavaChecker.
	 * The wizard page never writes JavaVersion/JavaArchitecture itself
	 * (CheckJava, the launch step, does that later), so neither does this. */
	Q_INVOKABLE void useJava(const QString& path);

	QObject* about() const;
	QObject* news() const;
	bool updaterAvailable() const;
	/// Called by Application, which knows whether the updater exists.
	void setUpdaterAvailable(bool available);
	/// Asks Application to check for updates now (checkForUpdatesRequested()).
	Q_INVOKABLE void checkForUpdates();

  signals:
	/* Emitted when the root window really closes -- not merely when it
	 * is hidden (see eventFilter()): a plugin's main_window_hide(), or a
	 * plugin close-filter vetoing the close (main_window_install_close_filter()),
	 * both hide the window without this firing, the same way neither
	 * triggers MainWindow::isClosing() on the widget path. */
	void closed();

	/// accountName()/accountKind()/accountCount() moved.
	void accountChanged();

	void launchRequested(const QString& id);
	/// launchInstanceMode() and launchInstanceAs() -- see there.
	void launchModeRequested(const QString& id, const QString& mode);
	void launchAsRequested(const QString& id, const QString& accountId);
	void killRequested(const QString& id);
	void folderRequested(const QString& id);
	void joinServerRequested(const QString& id, const QString& address);

	/// Emitted by requestImport() -- asks QML to open the New instance
	/// dialog in import mode with @p source filled in.
	void importRequested(const QString& source);

	/// Emitted by requestNavigate() -- Main.qml passes @p target to its
	/// navigate().
	void navigateRequested(const QString& target);

	/// Emitted by showInstanceLogRequested() above -- asks QML to open
	/// instance @p id's page on its Log tab.
	void openInstanceLog(const QString& id);

	/// groups() moved.
	void groupsChanged();
	/// importIcon() succeeded and the icon list now has @p key.
	void iconImported(const QString& key);

	/// setupSteps() moved.
	void setupStepsChanged();
	/// javaDetecting() moved.
	void javaDetectingChanged();

	void updaterAvailableChanged();
	/// checkForUpdates() -- Application runs the updater's check.
	void checkForUpdatesRequested();

  private:
	QVariantMap rootProperties();
	void scheduleSnapshotIfRequested();
	/* The actual rule-running: see setupSteps()'s Q_PROPERTY comment and
	 * finishSetupStep(). */
	void recomputeSetupSteps();

	/* Installed on m_window by load(). Watches for the window's own
	 * QEvent::Close to emit closed() -- see the signal's doc comment
	 * above and the longer comment on the definition. */
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
	/* Created in load(), like m_settings above -- see uiHost()'s Q_PROPERTY
	 * comment for how Application reaches this. */
	std::unique_ptr<QmlUiHost> m_uiHost;
	std::unique_ptr<ModrinthModpackModel> m_modpacks;
	std::unique_ptr<FlameModpackModel> m_curseForgeModpacks;
	std::unique_ptr<FtbModpackModel> m_ftbModpacks;
	std::unique_ptr<AtlModpackModel> m_atlModpacks;
	std::unique_ptr<TechnicModpackModel> m_technicModpacks;
	/* The one open instance detail page, if any - see instanceDetails(). */
	std::unique_ptr<InstanceDetails> m_instanceDetails;
	std::unique_ptr<InstanceExportController> m_exportController;
	std::unique_ptr<InstanceShortcutController> m_shortcutController;
	std::unique_ptr<AccountsController> m_accountsController;
	mutable std::unique_ptr<NewInstanceController> m_newInstance;
	mutable std::unique_ptr<JavaController> m_javaController;
	mutable std::unique_ptr<JavaDownloadController> m_javaDownload;
	mutable std::unique_ptr<FolderSettingsController> m_folderSettings;
	mutable std::unique_ptr<UpdateSettingsController> m_updateSettings;
	mutable std::unique_ptr<PlayTimeSummary> m_playTime;
	mutable std::unique_ptr<LibraryController> m_library;
	mutable std::unique_ptr<LauncherLogs> m_launcherLogs;
	/* Keyed by (anchor, anchorContext) -- see pluginSurfaces(). Declared
	 * alongside the other cached models, for the same reason: destroyed
	 * before the engine is torn down. */
	std::map<std::pair<int, QString>, std::unique_ptr<QObject>>
		m_pluginSurfaceModels;

	std::unique_ptr<QQmlApplicationEngine> m_engine;
	QQuickWindow* m_window = nullptr;
	int m_accountRevision = 0;

	/* Onboarding -- see setupSteps()'s Q_PROPERTY comment. Not exposed as
	 * shared_ptr members the way m_instances etc. are: TranslationsModel and
	 * JavaInstallList are LauncherContext-owned singletons (LAUNCHER->
	 * translations()/javalist()), shared with the rest of the launcher and
	 * outliving any one QmlShell, so there is nothing here to own. */
	QStringList m_setupSteps;
	bool m_javaDetecting = false;

	/* requestImport() sources that arrived before load() loaded the root
	 * window; emitted, in order, once it has. */
	QStringList m_pendingImports;

	ProfilersProvider m_profilersProvider;
	PluginListFactory m_pluginListFactory;
	std::unique_ptr<QObject> m_pluginList;
	std::unique_ptr<AboutInfo> m_about;
	std::unique_ptr<NewsModel> m_news;
	bool m_updaterAvailable = false;
	/* Whether show() has shown m_window yet: the first call shows it,
	 * later ones only raise it. */
	bool m_shown = false;
};
