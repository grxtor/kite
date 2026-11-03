/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include <QApplication>
#include <memory>
#include <functional>
#include <QDebug>
#include <QFlag>
#include <QDateTime>
#include <QUrl>
#include <QHash>
#include <QList>

#include <BaseInstance.h>

#include "Logging.h"

#include "minecraft/launch/MinecraftServerTarget.h"
#include "core/LauncherContext.h"

class LaunchController;
class QmlShell;
class QWindow;
class LocalPeer;
class QFile;
class HttpMetaCache;
class SettingsObject;
class InstanceList;
class AccountList;
class IconList;
class QNetworkAccessManager;
class JavaInstallList;
class BaseProfilerFactory;
class TranslationsModel;
class PluginManager;
class ExternalUpdater;

namespace Meta
{
	class Index;
}

/** How the game is to be started. Mutually exclusive on purpose: an offline
 * session and a demo are different ways to start. The profiler is not part of
 * this; it is an instance setting. */
enum class LaunchMode
{
	Normal,
	Offline,
	Demo
};

#if defined(APPLICATION)
#undef APPLICATION
#endif
#define APPLICATION (static_cast<Application*>(QCoreApplication::instance()))

class Application : public QApplication, public LauncherContext
{
	Q_OBJECT
  public:
	enum Status { StartingUp, Failed, Succeeded, Initialized };

  public:
	Application(int& argc, char** argv);
	~Application() override;

	PluginManager* pluginManager() const
	{
		return m_pluginManager.get();
	}

	std::shared_ptr<SettingsObject> settings() const override
	{
		return m_settings;
	}


	bool updaterEnabled();

	QString updaterBinaryName();

	void triggerUpdateCheck();

	std::shared_ptr<TranslationsModel> translations() override;

	std::shared_ptr<JavaInstallList> javalist() override;

	std::shared_ptr<InstanceList> instances() const override
	{
		return m_instances;
	}

	std::shared_ptr<IconList> icons() const override
	{
		return m_icons;
	}

	shared_qobject_ptr<AccountList> accounts() const override
	{
		return m_accounts;
	}

	QString msaClientId() const override;

	Status status() const
	{
		return m_status;
	}

	void updateProxySettings(QString proxyTypeStr, QString addr, int port,
							 QString user, QString password) override;

	shared_qobject_ptr<QNetworkAccessManager> network() override;

	shared_qobject_ptr<HttpMetaCache> metacache() override;

	shared_qobject_ptr<Meta::Index> metadataIndex() override;

	QString getJarsPath() override;

	AuthRequestDecorator* authRequestDecorator() const override;

	UiHost* uiHost() const override;

	NewsChecker* newsChecker() const override;
	ModBridge* modBridge() const override;
	bool dispatchPluginHook(uint32_t hookId, void* payload) override;

	/* Whether the QML shell is up and its presenter ready to answer, the check
	 * uiHost() makes before routing a UiHost call to QmlUiHost. */
	bool usingQmlShell() const;

	/* Runs @p call now, unless the QML shell's presenter is not ready yet
	 * (!usingQmlShell()): then it is queued with the startup update reports
	 * and run once the presenter is up. */
	void runWhenQmlPresenterReady(std::function<void()> call);

	/* Brings the QML shell's window up on @p target, a Main.qml navigate()
	 * path ("accounts", "settings/java", ...), for code outside QML that sends
	 * the user to a page. No-op without a QML shell. */
	void navigateQmlShell(const QString& target);

	/// this is the root of the 'installation'. Used for automatic updates
	const QString& root()
	{
		return m_rootPath;
	}

	/* What a launch calls to show an instance's console: the QML shell's
	 * instance-log signal. */
	void showInstanceLog(InstancePtr instance);

	/* Brings the main window up, loading the QML shell first if needed.
	 * False if the shell failed to load: the user has been told and the
	 * application is on its way out. */
	bool showMainWindow(bool minimized = false);

	/* What aboutToQuit does: saves the instance list, shuts the plugins down
	 * and closes the log. main() calls it on the failed-start path, where the
	 * event loop never runs. */
	void shutdownForExit();

	/* The QML shell's top-level window once it has been shown; null before.
	 * PluginManager uses it for the main_window_* API and the close filter. */
	QWindow* qmlShellWindow() const;


  public slots:
	/** Starts an instance. The profiler comes from the instance's own settings,
	 * so every way into the game profiles the same way. */
	bool launch(InstancePtr instance, LaunchMode mode = LaunchMode::Normal,
				MinecraftServerTargetPtr serverToJoin = nullptr,
				MinecraftAccountPtr accountToUse = nullptr);
	bool kill(InstancePtr instance);

  private slots:
	void messageReceived(const QByteArray& message);
	void controllerSucceeded();
	void controllerFailed(const QString& error);

  private:
	void performCLIAction();
	void performMainStartupAction();

	void showFatalErrorMessage(const QString& title, const QString& content);

	void initPlatform();
	QHash<QString, QVariant> parseCommandLine(int& argc, char** argv);
	bool resolveDataPath(const QHash<QString, QVariant>& args,
						 QString& dataPath, QString& adjustedBy,
						 QString& origcwdPath);
	bool initPeerInstance();
	bool initLogging(const QString& dataPath);
	void setupPaths(const QString& binPath, const QString& origcwdPath,
					const QString& adjustedBy);
	void initSettings();
	void initSubsystems();

  private:
	void reportUpdateMarkers();

	/* The user chose Quit on a startup prompt. quit() is a no-op before
	 * exec(), so this marks the start as failed and skips the rest of it. */
	void abortStartup();

	void addRunningInstance();
	void subRunningInstance();
	bool shouldExitNow() const;

  private:

	shared_qobject_ptr<QNetworkAccessManager> m_network;

	std::unique_ptr<ExternalUpdater> m_updater;
	shared_qobject_ptr<AccountList> m_accounts;

	shared_qobject_ptr<HttpMetaCache> m_metacache;
	shared_qobject_ptr<Meta::Index> m_metadataIndex;

	std::shared_ptr<SettingsObject> m_settings;
	std::shared_ptr<InstanceList> m_instances;
	std::shared_ptr<IconList> m_icons;
	std::shared_ptr<JavaInstallList> m_javalist;
	std::shared_ptr<TranslationsModel> m_translations;
	QString m_jarsPath;

	QMap<QString, std::shared_ptr<BaseProfilerFactory>> m_profilers;

	QString m_rootPath;

	QString m_dataPath;

	Status m_status = Application::StartingUp;

#if defined Q_OS_WIN32
	// used on Windows to attach the standard IO streams
	bool consoleAttached = false;
#endif

	// FIXME: attach to instances instead.
	struct InstanceXtras {
		shared_qobject_ptr<LaunchController> controller;
	};
	std::map<QString, InstanceXtras> m_instanceExtras;

	size_t m_openWindows = 0;
	size_t m_runningInstances = 0;

	// peer launcher instance connector - used to implement single instance
	// launcher and signalling
	LocalPeer* m_peerInstance = nullptr;

	std::unique_ptr<PluginManager> m_pluginManager;

	/* Built alongside m_pluginManager, so null in builds without the plugin
	 * host (Kite_PLUGINS). */
	std::unique_ptr<AuthRequestDecorator> m_authRequestDecorator;

	/* Answers every question with the safe default (decline, cancel, empty).
	 * Built before anything that could ask the user a question, because
	 * uiHost() promises never to return null; used only while no QML host
	 * is ready to answer (before the shell exists, and until its presenter is up). */
	std::unique_ptr<UiHost> m_uiHost;

	std::unique_ptr<QmlShell> m_qmlShell;

	/* Work that waits for the QML presenter: update reports queued by
	 * reportUpdateMarkers() during startup, before the shell exists, and calls
	 * queued by runWhenQmlPresenterReady(). Run by
	 * runPendingQmlPresenterCalls() once presenterReady(). */
	QList<std::function<void()>> m_pendingQmlPresenterCalls;
	void runPendingQmlPresenterCalls();

	/* An import from outside the window (--import, or the IPC "import"
	 * message): the QML shell's New instance dialog. Shows a window first if
	 * there is none. */
	void importFromUrl(const QUrl& url);

	/* Creates the QML shell, wires its signals to the real actions and loads
	 * its window without showing it. True once the shell exists; false if its
	 * QML failed to load, in which case the user has been told, m_qmlShell is
	 * reset and m_status is Failed. */
	bool ensureQmlShell();
	/* Tells the QML shell about the updater (whether "Check for updates" has
	 * anything to do, and the Updates settings' backend). Called by whichever
	 * of the two is made second; no-op without a shell. */
	void connectUpdaterToQmlShell();
	/* Loads the news and checks notifications once, after the first window is
	 * on screen (never for a command-line launch), and checks the instance
	 * folders. */
	void startBackgroundChecks();
	bool m_backgroundChecksStarted = false;
	/* Shows the notifications not yet dismissed for good through UiHost. */
	void showNotificationsThroughUiHost();
	/* Warns through UiHost about instance folders Java cannot live in. */
	void reportInstancePathProblems();
	unique_qobject_ptr<NewsChecker> m_newsChecker;
	unique_qobject_ptr<NotificationChecker> m_notificationChecker;
	/* The Kite mod's way to the launcher; listens only while a game with
	 * the mod runs. QmlShell::friends() hands it the friends controller. */
	unique_qobject_ptr<ModBridge> m_modBridge;
	/* Shows (first time) or raises the QML shell's window, counting it as open
	 * while shown (m_qmlWindowShown). */
	void showQmlShellWindow(bool minimized);
	void bridgeInstanceHooks();
	/* Whether the QML window is on screen and counted in m_openWindows: false
	 * while loaded but hidden (command-line launch) and once the user closes
	 * it. */
	bool m_qmlWindowShown = false;
	bool m_qmlMainReadyDispatched = false;
	bool m_startupAborted = false;

  public:
	QString m_instanceIdToLaunch;
	QString m_serverToJoin;
	/* Save folder name of a world to open on launch, from --world. Mutually
	 * exclusive with m_serverToJoin; startup rejects both being set. */
	QString m_worldToJoin;
	QString m_profileToUse;
	bool m_liveCheck = false;
	QUrl m_zipToImport;

	// CLI-only flags (headless, exit after action)
	bool m_cliListInstances = false;
	QString m_cliInstanceInfoId;
	QString m_cliExportId;
	QString m_cliOutputPath;
	std::unique_ptr<QFile> logFile;
};
