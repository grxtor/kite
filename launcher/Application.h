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

#include <QApplication>
#include <memory>
#include <functional>
#include <QDebug>
#include <QFlag>
#include <QIcon>
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
class InstanceWindow;
class InstanceSettingsPage;
class MainWindow;
class SetupWizard;
class GenericPageProvider;
class QFile;
class HttpMetaCache;
class SettingsObject;
class InstanceList;
class AccountList;
class IconList;
class QNetworkAccessManager;
class JavaInstallList;
class BaseProfilerFactory;
class BaseDetachedToolFactory;
class TranslationsModel;
class ITheme;
class ThemeManager;
class MCEditTool;
class PluginManager;
class BasePage;
class ExternalUpdater;

namespace Meta
{
	class Index;
}

/**
 * How the game is to be started.
 *
 * These are mutually exclusive on purpose: a demo session is never a
 * logged-in one, so a pair of booleans would allow combinations that mean
 * nothing. The profiler is deliberately not part of this -- it is an
 * instance setting now, not a property of one launch.
 */
enum class LaunchMode
{
	/// Log in, launch.
	Normal,
	/// Launch with whatever the account cache already has.
	Offline,
	/// Launch the demo, without logging in at all.
	Demo
};

#if defined(APPLICATION)
#undef APPLICATION
#endif
#define APPLICATION (static_cast<Application*>(QCoreApplication::instance()))

class Application : public QApplication, public LauncherContext
{
	// friends for the purpose of limiting access to deprecated stuff
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

	qint64 timeSinceStart() const
	{
		return startTime.msecsTo(QDateTime::currentDateTime());
	}

	QIcon getThemedIcon(const QString& name) override;

	void setIconTheme(const QString& name);

	std::vector<ITheme*> getValidApplicationThemes();

	void setApplicationTheme(const QString& name, bool initial);

	ThemeManager* themeManager() const;

	ExternalUpdater* updater()
	{
		return m_updater.get();
	}

	bool updaterEnabled();

	/// Where the updater binary sits, relative to the installation root.
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

	MCEditTool* mcedit() const
	{
		return m_mcedit.get();
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

	const QMap<QString, std::shared_ptr<BaseProfilerFactory>>& profilers() const
	{
		return m_profilers;
	}

	void updateProxySettings(QString proxyTypeStr, QString addr, int port,
							 QString user, QString password) override;

	shared_qobject_ptr<QNetworkAccessManager> network() override;

	shared_qobject_ptr<HttpMetaCache> metacache() override;

	shared_qobject_ptr<Meta::Index> metadataIndex() override;

	QString getJarsPath() override;

	AuthRequestDecorator* authRequestDecorator() const override;

	UiHost* uiHost() const override;

	/* Whether the QML shell is up and ready to answer -- the same check
	 * uiHost() makes before it will route a UiHost call to QmlUiHost (see
	 * that method's comment). Callers that need to choose between a
	 * widget window and a QML-side equivalent themselves (LaunchController's
	 * account picker, for one) use this instead of duplicating the check. */
	bool usingQmlShell() const;

	/// this is the root of the 'installation'. Used for automatic updates
	const QString& root()
	{
		return m_rootPath;
	}

	const QString javaPath();

	/*!
	 * Opens a json file using either a system default editor, or, if not empty,
	 * the editor specified in the settings
	 */
	bool openJsonEditor(const QString& filename);

	InstanceWindow* showInstanceWindow(InstancePtr instance,
									   QString page = QString());

	/* What a launch reaches for instead of showInstanceWindow() when it
	 * wants to show an instance's console: routes to the QML shell's own
	 * instance-log signal while it is the active UI (usingQmlShell()), or
	 * to the widget console window (showInstanceWindow(), selecting its
	 * Log page) otherwise -- so a launch never raises a widget window
	 * under the QML shell. */
	void showInstanceLog(InstancePtr instance);
	MainWindow* showMainWindow(bool minimized = false);
	MainWindow* mainWindow() const
	{
		return m_mainWindow;
	}

	/* The QML shell's top-level window when it is the active UI (see
	 * useQmlShell() in Application.cpp) and has been shown; null when
	 * the widget MainWindow is in use instead, or before either has
	 * been shown. PluginManager uses this to generalise main-window
	 * handling (show/hide/close-filter) to whichever UI is actually on
	 * screen -- see PluginManager::resolveShellWindow(). */
	QWindow* qmlShellWindow() const;

	void updateIsRunning(bool running);
	bool updatesAreAllowed();

	void ShowGlobalSettings(class QWidget* parent,
							QString open_page = QString());

	void registerGlobalSettingsPage(std::function<BasePage*()> creator);

  signals:
	void updateAllowedChanged(bool status);
	void globalSettingsAboutToOpen();
	void globalSettingsClosed();
	void instanceSettingsPageCreated(InstanceSettingsPage* page,
									 BaseInstance* instance);

  public slots:
	/**
	 * Start an instance.
	 *
	 * The profiler is read from the instance's own settings, so every way
	 * into the game profiles the same way; there is no per-launch profiler
	 * argument left to forget to pass.
	 */
	bool launch(InstancePtr instance, LaunchMode mode = LaunchMode::Normal,
				MinecraftServerTargetPtr serverToJoin = nullptr,
				MinecraftAccountPtr accountToUse = nullptr);
	bool kill(InstancePtr instance);

  private slots:
	void on_windowClose();
	void messageReceived(const QByteArray& message);
	void controllerSucceeded();
	void controllerFailed(const QString& error);
	void setupWizardFinished(int status);

  private:
	bool createSetupWizard();
	void performCLIAction();
	void performMainStartupAction();

	// sets the fatal error message and m_status to Failed.
	void showFatalErrorMessage(const QString& title, const QString& content);

	// Constructor initialization helpers
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
	bool reportUpdateMarkers();

	void addRunningInstance();
	void subRunningInstance();
	bool shouldExitNow() const;

  private:
	QDateTime startTime;

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
	std::shared_ptr<GenericPageProvider> m_globalSettingsProvider;
	std::unique_ptr<ThemeManager> m_themeManager;
	std::unique_ptr<MCEditTool> m_mcedit;
	QString m_jarsPath;
	QSet<QString> m_features;

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
		InstanceWindow* window = nullptr;
		shared_qobject_ptr<LaunchController> controller;
	};
	std::map<QString, InstanceXtras> m_instanceExtras;

	// main state variables
	size_t m_openWindows = 0;
	size_t m_runningInstances = 0;
	bool m_updateRunning = false;

	// main window, if any
	MainWindow* m_mainWindow = nullptr;

	// peer launcher instance connector - used to implement single instance
	// launcher and signalling
	LocalPeer* m_peerInstance = nullptr;

	SetupWizard* m_setupWizard = nullptr;
	std::unique_ptr<PluginManager> m_pluginManager;

	/* Built alongside m_pluginManager, so it is null in builds without the
	 * plugin host (MeshMC_PLUGINS is OFF by default). */
	std::unique_ptr<AuthRequestDecorator> m_authRequestDecorator;

	/* Built before anything that could ask the user a question, because
	 * uiHost() promises never to return null. */
	std::unique_ptr<UiHost> m_uiHost;

	/* The QML user interface, when it is the one in use instead of
	 * MainWindow. See useQmlShell(). */
	std::unique_ptr<QmlShell> m_qmlShell;

	/* Set by reportUpdateMarkers() when init() (before useQmlShell() is
	 * even consulted) finds an update lock/fail/success marker and the QML
	 * shell is about to be used: there is no QML window yet to answer
	 * through uiHost() at that point (see QmlUiHost's PRESENTER READINESS),
	 * so showing the classic QMessageBox there instead would be exactly
	 * the widget-under-QML leak the whole audit is about. Queued here and
	 * run once showMainWindow()'s QmlUiHost reports presenterReady(); run
	 * through the widget host instead if the QML shell then fails to load
	 * (see showMainWindow()). Empty when there is nothing to report. */
	QList<std::function<void()>> m_pendingQmlUpdateReports;

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
