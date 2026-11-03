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

#include "Application.h"
#include "BuildConfig.h"
#include "plugin/PluginAuthRequestDecorator.h"
#include "ui/LaunchController.h"
#include "models/BackupController.h"
#include "qml/QmlShell.h"
#include "qml/QmlUiHost.h"
#include "models/UpdateSettingsController.h"
#include "plugin/PluginManager.h"
#include "plugin/PluginSurfaceModel.h"
#include "plugin/PluginListModel.h"

#include "news/NewsChecker.h"
#include "notifications/NotificationChecker.h"
#include "core/InstancePathProblems.h"
#include "modbridge/ModBridge.h"

#include <QWindow>

#include "ApplicationMessage.h"

#include <iostream>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTranslator>
#include <QLibraryInfo>
#include <QList>
#include <QMutex>
#include <QStringList>
#include <QDebug>

#include "InstanceList.h"

#include <minecraft/auth/AccountList.h>
#include "icons/IconList.h"
#include "net/HttpMetaCache.h"

#include "java/JavaUtils.h"

#include "updater/ExternalUpdater.h"
#include "updater/UpdateLockFile.h"
#if defined(Q_OS_MAC)
#if defined(KITE_SPARKLE_ENABLED)
#include "updater/MacSparkleUpdater.h"
#endif
#else
#include "updater/KiteExternalUpdater.h"
#endif

#include "tools/JProfiler.h"
#include "tools/JVisualVM.h"

#include <QIcon>

#include "settings/INISettingsObject.h"
#include "settings/Setting.h"

#include "translations/TranslationsModel.h"
#include "meta/Index.h"

#include <Commandline.h>
#include <FileSystem.h>
#include <DesktopServices.h>
#include <LocalPeer.h>
#include "MMCZip.h"

#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/Component.h"
#include "minecraft/PresetDefaults.h"

#include "Sys.h"
#include "Logging.h"
#include "MMCStrings.h"

#if defined Q_OS_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static const QLatin1String liveCheckFile("live.check");

using namespace Commandline;

#define MACOS_HINT                                                             \
	"If you are on macOS Sierra, you might have to move the app to your "      \
	"/Applications or ~/Applications"                                          \
	"folder. This usually fixes the problem and you can move the application " \
	"elsewhere afterwards.\n"                                                  \
	"\n"

namespace
{
#if defined(Q_OS_MAC)
	QString macOSDataPath()
	{
		auto dataPath =
			QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
		if (!dataPath.isEmpty()) {
			return dataPath;
		}
		return FS::PathCombine(QDir::homePath(), "Library",
							   "Application Support", BuildConfig.KITE_NAME);
	}

	QStringList legacyMacDataPatterns()
	{
		return {"*.cfg",	   "*.ini",	 "*.json",		"*.log",	 "accounts",
				"assets",	   "cache",	 "icons",		"instances", "java",
				"libraries",   "meta",	 "metacache",	"mods",		 "patches",
				"screenshots", "themes", "translations"};
	}

	QStringList legacyMacDataEntries(const QString& legacyDataPath)
	{
		QDir legacyDir(legacyDataPath);
		QSet<QString> entries;

		for (const auto& pattern : legacyMacDataPatterns()) {
			auto matches = legacyDir.entryInfoList(
				QStringList{pattern}, QDir::NoDotAndDotDot | QDir::Dirs |
										  QDir::Files | QDir::Hidden |
										  QDir::System);
			for (const auto& match : matches) {
				entries.insert(match.fileName());
			}
		}

		return entries.values();
	}

	bool migrateLegacyMacEntry(const QString& sourcePath,
							   const QString& targetPath)
	{
		QFileInfo sourceInfo(sourcePath);
		if (!sourceInfo.exists()) {
			return true;
		}

		QFileInfo targetInfo(targetPath);
		if (sourceInfo.isDir()) {
			if (!targetInfo.exists() && QDir().rename(sourcePath, targetPath)) {
				return true;
			}
			if (!FS::ensureFolderPathExists(targetPath)) {
				return false;
			}
			if (!FS::copy(sourcePath, targetPath)()) {
				return false;
			}
			return FS::deletePath(sourcePath);
		}

		if (targetInfo.exists()) {
			return QFile::remove(sourcePath);
		}
		if (!FS::ensureFilePathExists(targetPath)) {
			return false;
		}
		if (QFile::rename(sourcePath, targetPath)) {
			return true;
		}
		if (!QFile::copy(sourcePath, targetPath)) {
			return false;
		}
		return QFile::remove(sourcePath);
	}

	bool migrateLegacyMacData(const QString& legacyDataPath,
							  const QString& dataPath)
	{
		bool migrated = true;
		for (const auto& entry : legacyMacDataEntries(legacyDataPath)) {
			const auto sourcePath = FS::PathCombine(legacyDataPath, entry);
			const auto targetPath = FS::PathCombine(dataPath, entry);
			if (!migrateLegacyMacEntry(sourcePath, targetPath)) {
				qWarning() << "Failed to migrate legacy macOS data entry"
						   << sourcePath << "to" << targetPath;
				migrated = false;
			}
		}
		return migrated;
	}
#endif
} // namespace

namespace
{
	/* What uiHost() answers with while no QML host exists: nobody to ask, so
	 * every question gets the safe default (decline, cancel, empty). */
	class HeadlessUiHost final : public UiHost
	{
	  public:
		std::unique_ptr<BusyIndicator>
		showBusy(const QString& text, const QString&,
				 std::function<void()>) override
		{
			unanswered("showBusy", text);
			return std::make_unique<BusyIndicator>();
		}
		void message(const QString& title, const QString&, Severity) override
		{
			unanswered("message", title);
		}
		bool confirm(const QString& title, const QString&, Severity,
					 const QString&, const QString&) override
		{
			unanswered("confirm", title);
			return false;
		}
		int choose(const QString& title, const QString&, Severity,
				   const QStringList&) override
		{
			unanswered("choose", title);
			return -1;
		}
		AccountChoice pickAccount(const QString& title, const QString&,
								  const QList<MinecraftAccountPtr>&) override
		{
			unanswered("pickAccount", title);
			return {};
		}
		std::optional<QString> askText(const QString& title, const QString&,
									   const QString&) override
		{
			unanswered("askText", title);
			return std::nullopt;
		}
		bool resolveBlockedMods(const QString& title, const QString&,
								QList<BlockedMod>&) override
		{
			unanswered("resolveBlockedMods", title);
			return false;
		}
		bool confirmUntrustedMods(const QStringList&) override
		{
			unanswered("confirmUntrustedMods", QString());
			return false;
		}
		UpdateChoice offerUpdate(const QString&, const QString&,
								 const QString&) override
		{
			unanswered("offerUpdate", QString());
			return UpdateChoice::Later;
		}
		bool setupProfile(MinecraftAccountPtr) override
		{
			unanswered("setupProfile", QString());
			return false;
		}
		std::optional<QString> pickFile(FilePickerMode, const QString& title,
										const QString&, const QString&) override
		{
			unanswered("pickFile", title);
			return std::nullopt;
		}
		std::optional<PluginModalResult>
		runPluginModal(const QString& title, const QJsonObject&) override
		{
			unanswered("runPluginModal", title);
			return std::nullopt;
		}
		bool chooseOptionalMods(const QString& packName,
								AtlOptionalModsModel&) override
		{
			unanswered("chooseOptionalMods", packName);
			return false;
		}

	  private:
		static void unanswered(const char* call, const QString& subject)
		{
			qWarning() << "UiHost::" << call << "asked with no UI up:"
					   << subject;
		}
	};

	// Four layouts, built once.
	const QString& logPattern(bool coloured, bool withSourceLocation)
	{
		static const QString patterns[] = {
			Logging::messagePattern(false, false),
			Logging::messagePattern(false, true),
			Logging::messagePattern(true, false),
			Logging::messagePattern(true, true),
		};
		return patterns[(coloured ? 2 : 0) + (withSourceLocation ? 1 : 0)];
	}

	void appDebugOutput(QtMsgType type, const QMessageLogContext& context,
						const QString& msg)
	{
		// The log file is a plain QFile and messages arrive from worker
		// threads, so the handler is serialised; that also keeps the pattern
		// swap below from being seen by another thread.
		static QMutex mutex;
		QMutexLocker locker(&mutex);

		// Formatting goes through Qt for the category and function name.
		// QT_MESSAGE_PATTERN in the environment makes Qt ignore
		// qSetMessagePattern(). Qt's own libraries carry no log context, hence
		// no static location suffix.
		const bool located = context.line != 0;

		qSetMessagePattern(logPattern(false, located));
		const QString plain = qFormatLogMessage(type, context, msg);

		if (APPLICATION->logFile) {
			APPLICATION->logFile->write(plain.toUtf8() + '\n');
			APPLICATION->logFile->flush();
		}

		// Rendered only when the console understands escapes; otherwise it
		// gets the plain copy.
		QString console = plain;
		if (Logging::consoleColourEnabled()) {
			qSetMessagePattern(logPattern(true, located));
			console = qFormatLogMessage(type, context, msg);
			// Leave a plain layout installed: it is what anything formatting
			// outside this handler gets.
			qSetMessagePattern(logPattern(false, located));
		}

		QTextStream(stderr) << console.toLocal8Bit() << '\n';
		fflush(stderr);
	}
} // namespace

Application::Application(int& argc, char** argv) : QApplication(argc, argv)
{
	/* First: things constructed below already reach services through
	 * LAUNCHER-> and would see a null context. */
	LauncherContext::setInstance(this);
	m_uiHost = std::make_unique<HeadlessUiHost>();

	initPlatform();
	if (m_status != StartingUp)
		return;

	auto args = parseCommandLine(argc, argv);
	if (m_status != StartingUp)
		return;

	QString origcwdPath, adjustedBy, dataPath;
	if (!resolveDataPath(args, dataPath, adjustedBy, origcwdPath))
		return;
	m_dataPath = dataPath;

	if (m_instanceIdToLaunch.isEmpty() && !m_serverToJoin.isEmpty()) {
		qWarning() << "--server can only be used in combination with --launch!";
		m_status = Application::Failed;
		return;
	}

	if (m_instanceIdToLaunch.isEmpty() && !m_worldToJoin.isEmpty()) {
		qWarning() << "--world can only be used in combination with --launch!";
		m_status = Application::Failed;
		return;
	}

	if (!m_serverToJoin.isEmpty() && !m_worldToJoin.isEmpty()) {
		qWarning() << "--server and --world cannot be used together!";
		m_status = Application::Failed;
		return;
	}

	if (m_instanceIdToLaunch.isEmpty() && !m_profileToUse.isEmpty()) {
		qWarning()
			<< "--profile can only be used in combination with --launch!";
		m_status = Application::Failed;
		return;
	}

	// CLI fast path: needs only the settings and instance list. Skipping
	// network, plugins and the GUI also avoids a "corrupted double-linked
	// list" crash from plugin static destructors running after Qt is torn
	// down.
	if (m_cliListInstances || !m_cliInstanceInfoId.isEmpty() ||
		!m_cliExportId.isEmpty()) {
		initSettings();

		/* The additional folders too, or "list instances" would disagree with
		 * the window. */
		QStringList instDirs;
		instDirs << m_settings->get("InstanceDir").toString();
		instDirs << InstanceList::decodeInstanceDirList(
			m_settings->get("AdditionalInstanceDirs"));
		m_instances.reset(new InstanceList(m_settings, instDirs, this));
		m_instances->loadList();

		performCLIAction();
		return;
	}

	if (!initPeerInstance())
		return;
	if (!initLogging(dataPath))
		return;

	QString binPath = applicationDirPath();
	setupPaths(binPath, origcwdPath, adjustedBy);

	initSettings();

	reportUpdateMarkers();

	initSubsystems();

	// Scans the mmcmodules directories and loads all discovered .mmco modules.
	// The parent is nullptr: unique_ptr is the sole owner, and passing `this`
	// would free the PluginManager twice.
	m_pluginManager = std::make_unique<PluginManager>(this, nullptr);
	m_authRequestDecorator =
		std::make_unique<PluginAuthRequestDecorator>(m_pluginManager.get());
	m_pluginManager->initializeAll();

	performMainStartupAction();
}

void Application::initPlatform()
{
#if defined Q_OS_WIN32
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		if (freopen("CON", "w", stdout)) {
			std::ios_base::sync_with_stdio();
		}
		if (freopen("CON", "w", stderr)) {
			std::ios_base::sync_with_stdio();
		}
		if (freopen("CON", "r", stdin)) {
			std::cin.sync_with_stdio();
		}
		auto out = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD written;
		const char* endline = "\n";
		WriteConsole(out, endline, strlen(endline), &written, nullptr);
		consoleAttached = true;
	}
#endif
	// After the streams are reopened (the answer depends on where stderr ends
	// up) and before the message handler is installed.
	Logging::prepareConsoleColour();
	setOrganizationName(BuildConfig.KITE_NAME);
	setOrganizationDomain(BuildConfig.KITE_DOMAIN);
	setApplicationName(BuildConfig.KITE_NAME);
	setApplicationDisplayName(QString("%1 %2").arg(BuildConfig.KITE_DISPLAYNAME, BuildConfig.printableVersionString()));
	setApplicationVersion(BuildConfig.printableVersionString());

#ifdef Q_OS_LINUX
	{
		QFile osrelease("/proc/sys/kernel/osrelease");
		if (osrelease.open(QFile::ReadOnly | QFile::Text)) {
			QTextStream in(&osrelease);
			auto contents = in.readAll();
			if (contents.contains("WSL", Qt::CaseInsensitive) ||
				contents.contains("Microsoft", Qt::CaseInsensitive)) {
				showFatalErrorMessage(
					"Unsupported system detected!",
					"Linux-on-Windows distributions are not supported.\n\n"
					"Please use the Windows binary when playing on Windows.");
				return;
			}
		}
	}
#endif

	this->setQuitOnLastWindowClosed(false);
}

QHash<QString, QVariant> Application::parseCommandLine(int& argc, char** argv)
{
	QHash<QString, QVariant> args;

	Parser parser(FlagStyle::GNU, ArgumentStyle::SpaceAndEquals);

	parser.addSwitch("help");
	parser.addShortOpt("help", 'h');
	parser.addDocumentation("help", "Display this help and exit.");
	parser.addSwitch("version");
	parser.addShortOpt("version", 'V');
	parser.addDocumentation("version", "Display program version and exit.");
	parser.addOption("dir");
	parser.addShortOpt("dir", 'd');
	parser.addDocumentation(
		"dir", "Use the supplied folder as application root "
			   "instead of the binary location (use '.' for current)");
	parser.addOption("launch");
	parser.addShortOpt("launch", 'l');
	parser.addDocumentation("launch",
							"Launch the specified instance (by instance ID)");
	parser.addOption("server");
	parser.addShortOpt("server", 's');
	parser.addDocumentation("server",
							"Join the specified server on launch (only valid "
							"in combination with --launch)");
	parser.addOption("world");
	parser.addShortOpt("world", 'w');
	parser.addDocumentation("world",
							"Open the specified singleplayer world on launch, "
							"by its save folder name (only valid in "
							"combination with --launch)");
	parser.addOption("profile");
	parser.addShortOpt("profile", 'a');
	parser.addDocumentation("profile",
							"Use the account specified by its profile name "
							"(only valid in combination with --launch)");
	parser.addSwitch("alive");
	parser.addDocumentation("alive", "Write a small '" + liveCheckFile +
										 "' file after Kite starts");
	parser.addOption("import");
	parser.addShortOpt("import", 'I');
	parser.addDocumentation(
		"import", "Import instance from specified zip (local path or URL)");

	parser.addSwitch("list-instances");
	parser.addShortOpt("list-instances", 'L');
	parser.addDocumentation("list-instances", "List all instances and exit.");
	parser.addOption("instance-info");
	parser.addShortOpt("instance-info", 'i');
	parser.addDocumentation("instance-info",
							"Show detailed information about the specified "
							"instance (by ID) and exit.");
	parser.addOption("export");
	parser.addShortOpt("export", 'E');
	parser.addDocumentation("export",
							"Export the specified instance to a zip file. "
							"Requires --output to set the destination path.");
	parser.addOption("output");
	parser.addShortOpt("output", 'o');
	parser.addDocumentation("output", "Output file path (used with --export).");

	try {
		args = parser.parse(arguments());
	} catch (const ParsingError& e) {
		qCritical() << "CommandLineError:" << e.what();
		if (argc > 0)
			qCritical() << "Try '" << argv[0]
						<< "' -h' to get help on command line parameters.";
		m_status = Application::Failed;
		return args;
	}

	if (args["help"].toBool()) {
		QTextStream(stdout) << parser.compileHelp(arguments()[0]);
		m_status = Application::Succeeded;
		return args;
	}

	if (args["version"].toBool()) {
		QTextStream(stdout)
			<< "Version " << BuildConfig.printableVersionString() << "\n";
		QTextStream(stdout) << "Git " << BuildConfig.GIT_COMMIT << "\n";
		m_status = Application::Succeeded;
		return args;
	}

	m_instanceIdToLaunch = args["launch"].toString();
	m_serverToJoin = args["server"].toString();
	m_worldToJoin = args["world"].toString();
	m_profileToUse = args["profile"].toString();
	m_liveCheck = args["alive"].toBool();
	m_zipToImport = args["import"].toUrl();

	m_cliListInstances = args["list-instances"].toBool();
	m_cliInstanceInfoId = args["instance-info"].toString();
	m_cliExportId = args["export"].toString();
	m_cliOutputPath = args["output"].toString();

	if (!m_cliExportId.isEmpty() && m_cliOutputPath.isEmpty()) {
		qCritical() << "--export requires --output <path>.";
		m_status = Application::Failed;
		return args;
	}

	return args;
}

bool Application::resolveDataPath(const QHash<QString, QVariant>& args,
								  QString& dataPath, QString& adjustedBy,
								  QString& origcwdPath)
{
	origcwdPath = QDir::currentPath();

	QString dirParam = args["dir"].toString();
	if (!dirParam.isEmpty()) {
		adjustedBy += "Command line " + dirParam;
		dataPath = dirParam;
	} else {
#if defined(Q_OS_MAC)
		dataPath = macOSDataPath();
		adjustedBy += "macOS application data location " + dataPath;
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
		QDir portableDir(applicationDirPath());
		portableDir.cdUp();
		QString portableRoot = portableDir.absolutePath();
		QString portablePath = FS::PathCombine(portableRoot, "portable.txt");
		if (QFileInfo::exists(portablePath)) {
			dataPath = portableRoot;
			adjustedBy +=
				"Portable mode (portable.txt found), using portable root " +
				dataPath;
		} else {
			QString xdgDataHome =
				QProcessEnvironment::systemEnvironment().value("XDG_DATA_HOME");
			if (xdgDataHome.isEmpty()) {
				xdgDataHome = QDir::homePath() + "/.local/share";
			}
			dataPath = FS::PathCombine(xdgDataHome, BuildConfig.KITE_NAME);
			adjustedBy +=
				"Non-portable mode, using XDG data location " + dataPath;
		}
#elif defined(Q_OS_WIN32)
		QString portablePath =
			FS::PathCombine(applicationDirPath(), "portable.txt");
		if (QFileInfo::exists(portablePath)) {
			dataPath = applicationDirPath();
			adjustedBy +=
				"Portable mode (portable.txt found), using binary path " +
				dataPath;
		} else {
			QString appDataPath = QStandardPaths::writableLocation(
				QStandardPaths::AppDataLocation);
			dataPath = appDataPath;
			adjustedBy +=
				"Non-portable mode, using AppData location " + dataPath;
		}
#else
		dataPath = applicationDirPath();
		adjustedBy += "Fallback to binary path " + dataPath;
#endif
	}

	if (!FS::ensureFolderPathExists(dataPath)) {
		showFatalErrorMessage(
			"Kite data folder could not be created.",
			QString("Kite data folder could not be created.\n"
					"\n"
#if defined(Q_OS_MAC)
					MACOS_HINT
#endif
					"Make sure you have the right permissions to Kite data "
					"folder and any folder needed to access it.\n"
					"(%1)\n"
					"\n"
					"Kite cannot continue until you fix this problem.")
				.arg(dataPath));
		return false;
	}

	if (!QDir::setCurrent(dataPath)) {
		showFatalErrorMessage(
			"Kite data folder could not be opened.",
			QString("Kite data folder could not be opened.\n"
					"\n"
#if defined(Q_OS_MAC)
					MACOS_HINT
#endif
					"Make sure you have the right permissions to Kite data "
					"folder.\n"
					"(%1)\n"
					"\n"
					"Kite cannot continue until you fix this problem.")
				.arg(dataPath));
		return false;
	}

#if defined(Q_OS_MAC)
	const auto legacyDataPath = QDir(applicationDirPath()).absolutePath();
	if (dataPath != legacyDataPath) {
		const auto legacyEntries = legacyMacDataEntries(legacyDataPath);
		if (!legacyEntries.isEmpty()) {
			qInfo() << "Migrating legacy macOS data from app bundle to"
					<< dataPath << legacyEntries;
			migrateLegacyMacData(legacyDataPath, dataPath);
		}
	}
#endif

	return true;
}

bool Application::initPeerInstance()
{
	/* If an instance already runs on the same data path, tell it what the user
	 * wanted and exit. Before logging, to leave its log untouched. */
	auto appID = ApplicationId::fromPathAndVersion(
		QDir::currentPath(), BuildConfig.printableVersionString());

	// FIXME: you can run the same binaries with multiple data dirs
	// and they won't clash. This could cause issues for updates.
	m_peerInstance = new LocalPeer(this, appID);
	connect(m_peerInstance, &LocalPeer::messageReceived, this,
			&Application::messageReceived);
	if (m_peerInstance->isClient()) {
		int timeout = 2000;

		if (m_instanceIdToLaunch.isEmpty()) {
			ApplicationMessage activate;
			activate.command = "activate";
			m_peerInstance->sendMessage(activate.serialize(), timeout);

			if (!m_zipToImport.isEmpty()) {
				ApplicationMessage import;
				import.command = "import";
				import.args.insert("path", m_zipToImport.toString());
				m_peerInstance->sendMessage(import.serialize(), timeout);
			}
		} else {
			ApplicationMessage launch;
			launch.command = "launch";
			launch.args["id"] = m_instanceIdToLaunch;

			if (!m_serverToJoin.isEmpty()) {
				launch.args["server"] = m_serverToJoin;
			}
			if (!m_worldToJoin.isEmpty()) {
				launch.args["world"] = m_worldToJoin;
			}
			if (!m_profileToUse.isEmpty()) {
				launch.args["profile"] = m_profileToUse;
			}
			m_peerInstance->sendMessage(launch.serialize(), timeout);
		}
		m_status = Application::Succeeded;
		return false;
	}

	return true;
}

bool Application::initLogging(const QString& dataPath)
{
	static const QString logBase = BuildConfig.KITE_NAME + "-%0.log";
	auto moveFile = [](const QString& oldName, const QString& newName) {
		static_cast<void>(QFile::remove(newName));
		static_cast<void>(QFile::copy(oldName, newName));
		static_cast<void>(QFile::remove(oldName));
	};

	moveFile(logBase.arg(3), logBase.arg(4));
	moveFile(logBase.arg(2), logBase.arg(3));
	moveFile(logBase.arg(1), logBase.arg(2));
	moveFile(logBase.arg(0), logBase.arg(1));

	logFile = std::make_unique<QFile>(logBase.arg(0));
	if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text |
					   QIODevice::Truncate)) {
		showFatalErrorMessage(
			"Kite data folder is not writable!",
			QString("Kite couldn't create a log file - the data folder is "
					"not writable.\n"
					"\n"
#if defined(Q_OS_MAC)
					MACOS_HINT
#endif
					"Make sure you have write permissions to the data folder.\n"
					"(%1)\n"
					"\n"
					"Kite cannot continue until you fix this problem.")
				.arg(dataPath));
		return false;
	}
	qInstallMessageHandler(appDebugOutput);
	qDebug() << "<> Log initialized.";
	return true;
}

void Application::setupPaths(const QString& binPath, const QString& origcwdPath,
							 const QString& adjustedBy)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
	QDir foo(FS::PathCombine(binPath, ".."));
	m_rootPath = foo.absolutePath();
#elif defined(Q_OS_WIN32)
	m_rootPath = binPath;
#elif defined(Q_OS_MAC)
	QDir foo(FS::PathCombine(binPath, "../.."));
	m_rootPath = foo.absolutePath();
	// macOS: touch the root so Finder reloads the .app metadata (icon
	// changes).
	FS::updateTimestamp(m_rootPath);
#endif

	qInfo().noquote() << BuildConfig.KITE_DISPLAYNAME << ", (c) 2026"
					  << BuildConfig.KITE_COPYRIGHT;
	qInfo().noquote() << "Version                    : "
					  << BuildConfig.printableVersionString();
	qInfo().noquote() << "Git commit                 : " << BuildConfig.GIT_COMMIT;
	qInfo().noquote() << "Git refspec                : " << BuildConfig.GIT_REFSPEC;
	qInfo().noquote() << "Compiled for               : " << BuildConfig.systemID();
	qInfo().noquote() << "Compiled by                : " << BuildConfig.compilerID();
	qInfo().noquote() << "Build Artifact             : " << BuildConfig.BUILD_ARTIFACT;
	if (adjustedBy.size()) {
		qInfo().noquote() << "Work dir before adjustment : " << origcwdPath;
		qInfo().noquote() << "Work dir after adjustment  : " << QDir::currentPath();
		qInfo().noquote() << "Adjusted by                : " << adjustedBy;
	} else {
		qInfo().noquote() << "Work dir                   : " << QDir::currentPath();
	}
	qInfo().noquote() << "Binary path                : " << binPath;
	qInfo().noquote() << "Application root path      : " << m_rootPath;
	if (!m_instanceIdToLaunch.isEmpty()) {
		qInfo().noquote() << "ID of instance to launch   : " << m_instanceIdToLaunch;
	}
	if (!m_serverToJoin.isEmpty()) {
		qInfo().noquote() << "Address of server to join  :" << m_serverToJoin;
	}
	if (!m_worldToJoin.isEmpty()) {
		qInfo().noquote() << "Name of world to join      :" << m_worldToJoin;
	}
	qInfo().noquote() << "<> Paths set.";

	if (m_liveCheck) {
		auto appID = ApplicationId::fromPathAndVersion(
			QDir::currentPath(), BuildConfig.printableVersionString());
		QFile check(liveCheckFile);
		if (check.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			auto payload = appID.toString().toUtf8();
			if (check.write(payload) != payload.size()) {
				qWarning() << "Could not write into" << liveCheckFile << "!";
				check.remove();
			} else {
				check.close();
			}
		} else {
			qWarning() << "Could not open" << liveCheckFile << "for writing!";
		}
	}
}

void Application::initSettings()
{
	m_settings.reset(
		new INISettingsObject(BuildConfig.KITE_CONFIGFILE, this));
	m_settings->registerSetting("AutoUpdate", true);

	m_settings->registerSetting("UiThemeMode", QString("dark"));
	// Colour scheme of the QML interface: amethyst, ember or diamond.
	m_settings->registerSetting("UiPalette", QString("amethyst"));
	// The QML interface's look: stage (Sahne), calm (Sade), night (Gece),
	// or classic, which keeps UiPalette and UiThemeMode.
	m_settings->registerSetting("UiLook", QString("stage"));
	// Built-in icon per instance without its own, per look
	// (icons/AutoIcons.h): a JSON object of id -> icon key.
	m_settings->registerSetting("UiAutoIcons", QString());
	m_settings->registerSetting("UiFontFamily", QString("Inter"));
	m_settings->registerSetting("UiSidebarCollapsed", true);
	m_settings->registerSetting("UiSidebarWidth", 240);
	m_settings->registerSetting("UiReduceMotion", false);

	/* Screen-top menu bar (macOS only; elsewhere carried but unused). The
	 * second key is the original name, kept so existing configs still read. */
	m_settings->registerSetting({"MacNativeMenuBar", "UseMacNativeMenuBar"},
								true);

	m_settings->registerSetting("ShownNotifications", QString());

	m_settings->registerSetting("ConsoleMaxLines", 100000);
	m_settings->registerSetting("ConsoleOverflowStop", true);

	m_settings->registerSetting("InstanceDir", "instances");
	/* Extra instance folders searched after the primary one. Held as a JSON
	 * array in one string, not a QStringList: INIFile saves values through
	 * QVariant::toString(), which is empty for a multi-element list. Use
	 * InstanceList::decodeInstanceDirList() and encodeInstanceDirList(). */
	m_settings->registerSetting("AdditionalInstanceDirs", QString());
	m_settings->registerSetting({"CentralModsDir", "ModsDir"}, "mods");
	m_settings->registerSetting("IconsDir", "icons");
	m_settings->registerSetting("SkinsDir", "skins");
    m_settings->registerSetting("JavaDir", "java");

	/* Whether installing an already-installed modpack offers to update that
	 * instance instead of creating a second. Phrased as "skip" so the default
	 * (false) asks the question. */
	m_settings->registerSetting("SkipModpackUpdatePrompt", false);

	/* Whether creating or updating an instance also fetches the game's own
	 * files. On by default: otherwise the first launch of a fresh pack is the
	 * slow one. */
	m_settings->registerSetting("DownloadGameFilesDuringInstanceCreation",
								true);

	m_settings->registerSetting("JsonEditor", QString());

	m_settings->registerSetting("Language", QString());

	m_settings->registerSetting("ShowConsole", false);
	m_settings->registerSetting("ShowConsoleOnError", true);
	m_settings->registerSetting("LogPrePostOutput", true);

	m_settings->registerSetting({"LaunchMaximized", "MCWindowMaximize"}, false);
	m_settings->registerSetting({"MinecraftWinWidth", "MCWindowWidth"}, 854);
	m_settings->registerSetting({"MinecraftWinHeight", "MCWindowHeight"}, 480);

	m_settings->registerSetting("ProxyType", "None");
	m_settings->registerSetting({"ProxyAddr", "ProxyHostName"}, "127.0.0.1");
	m_settings->registerSetting("ProxyPort", 8080);
	m_settings->registerSetting({"ProxyUser", "ProxyUsername"}, "");
	m_settings->registerSetting({"ProxyPass", "ProxyPassword"}, "");

	// Memory defaults from system RAM; a saved value always wins.
	const quint64 systemRamMiB = Sys::getSystemRam() / Sys::mebibyte;
	m_settings->registerSetting({"MinMemAlloc", "MinMemoryAlloc"},
								PresetDefaults::minMemoryMiB(systemRamMiB));
	m_settings->registerSetting({"MaxMemAlloc", "MaxMemoryAlloc"},
								PresetDefaults::maxMemoryMiB(systemRamMiB));
	m_settings->registerSetting("PermGen", 128);

	// CurseForge API key, baked in via BuildConfig. Exposed as a read-only
	// setting so .mmco plugins can read it through
	// MMCOContext::app_setting_get (ABI 3); they cannot link BuildConfig.
	m_settings->registerSetting("CurseForgeAPIKey", "");
	m_settings->set("CurseForgeAPIKey", BuildConfig.CURSEFORGE_API_KEY);

	m_settings->registerSetting("JavaPath", "");
	m_settings->registerSetting("JavaTimestamp", 0);
	m_settings->registerSetting("JavaArchitecture", "");
	// os.arch of the Java, which natives have to match ("aarch64", ...)
	m_settings->registerSetting("JavaRealArchitecture", "");
	// the Java the values above were recorded for
	m_settings->registerSetting("JavaCheckedPath", "");
	m_settings->registerSetting("JavaVersion", "");
	m_settings->registerSetting("JavaVendor", "");
	m_settings->registerSetting("LastHostname", "");
	m_settings->registerSetting("JvmArgs", "");

	m_settings->registerSetting("JavaAutoDownload", true);
	m_settings->registerSetting("JavaAutoDownloadVendor", "net.minecraft.java");

	m_settings->registerSetting("UseNativeOpenAL", false);
	m_settings->registerSetting("UseNativeGLFW", false);

	m_settings->registerSetting("ShowGameTime", true);
	m_settings->registerSetting("RecordGameTime", true);

	m_settings->registerSetting("MCLaunchMethod", "KitePart");

	m_settings->registerSetting("WrapperCommand", "");

	m_settings->registerSetting({"PreLaunchCommand", "PreLaunchCmd"}, "");
	m_settings->registerSetting({"PostExitCommand", "PostExitCmd"}, "");

	// Instance backups. The legacy synonym is the key the old BackupSystem
	// .mmco plugin registered; it is dropped from kite.cfg the first time the
	// setting is written.
	m_settings->registerSetting(
		{"BackupBeforeLaunch", "plugin.backup_system.BackupBeforeLaunch"},
		false);

	m_settings->registerSetting("TheCat", false);
	m_settings->registerSetting("BackgroundCat", QString("kitteh"));

	m_settings->registerSetting("InstSortMode", "Name");
	m_settings->registerSetting("InstGrouped", true);

	m_settings->registerSetting("PasteEEAPIKey", "kite");

	// Friends (server/friends/): on unless turned off. FriendsNoticeSeen is
	// the one-time "Friends is on" card. The Mojang URL is a test hook for the
	// local end-to-end run.
	m_settings->registerSetting("FriendsEnabled", true);
	m_settings->registerSetting("FriendsNoticeSeen", false);
	// Friends panel on the QML shell's right edge. FriendsPanelExpanded: names
	// and groups shown; FriendsPanelWidth: pixel width the user dragged it to.
	m_settings->registerSetting("FriendsPanelOpen", true);
	m_settings->registerSetting("FriendsPanelExpanded", false);
	m_settings->registerSetting("FriendsPanelWidth", 320);
	m_settings->registerSetting("FriendsServerUrl", BuildConfig.FRIENDS_URL);
	m_settings->registerSetting("FriendsMojangSessionUrl",
								QString("https://sessionserver.mojang.com"));

	qDebug() << "<> Settings loaded.";
}

void Application::initSubsystems()
{
	{
		m_network = new QNetworkAccessManager();
		QString proxyTypeStr = settings()->get("ProxyType").toString();
		QString addr = settings()->get("ProxyAddr").toString();
		int port = settings()->get("ProxyPort").value<qint16>();
		QString user = settings()->get("ProxyUser").toString();
		QString pass = settings()->get("ProxyPass").toString();
		updateProxySettings(proxyTypeStr, addr, port, user, pass);
		qDebug() << "<> Network done.";
	}

	/* The Kite mod bridge: nothing listens until a launch registers a session. */
	m_modBridge.reset(new ModBridge(
		QString("%1 %2").arg(BuildConfig.KITE_DISPLAYNAME,
							 BuildConfig.printableVersionString())));

	/* News and notifications belong to the application (the QML shell and the
	 * plugin news API use them too). Nothing is fetched until
	 * startBackgroundChecks(). Feed 0 is ours; later ones come from
	 * -DKite_NEWS_EXTRA_FEEDS. */
	{
		QStringList feeds{ BuildConfig.NEWS_RSS_URL };
		feeds.append(BuildConfig.NEWS_EXTRA_FEEDS.split(QLatin1Char(';'),
														Qt::SkipEmptyParts));
		m_newsChecker.reset(new NewsChecker(m_network, feeds));
		/* Re-published as MMCO_HOOK_NEWS_UPDATED here, the only place that
		 * knows when every feed has landed. */
		connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this, [this]() {
			if (m_pluginManager) {
				m_pluginManager->dispatchHook(MMCO_HOOK_NEWS_UPDATED);
			}
		});

		auto* notifications = new NotificationChecker();
		notifications->setNotificationsUrl(QUrl(BuildConfig.NOTIFICATION_URL));
		notifications->setApplicationChannel(BuildConfig.VERSION_CHANNEL);
		notifications->setApplicationPlatform(BuildConfig.BUILD_PLATFORM);
		notifications->setApplicationFullVersion(BuildConfig.FULL_VERSION_STR);
		m_notificationChecker.reset(notifications);
		/* Asked through UiHost once the presenter can answer. */
		connect(m_notificationChecker.get(),
				&NotificationChecker::notificationCheckFinished, this,
				[this]() {
					runWhenQmlPresenterReady(
						[this]() { showNotificationsThroughUiHost(); });
				});
	}

	{
		m_translations.reset(new TranslationsModel("translations"));
		auto bcp47Name = m_settings->get("Language").toString();
		m_translations->selectLanguage(bcp47Name);
		qDebug() << "Your language is" << bcp47Name;
		qDebug() << "<> Translations loaded.";
	}

	// Created before the QML shell, so no window exists yet to anchor its dialogs
	// (matters only for the "On Launch" check).
	if (updaterEnabled()) {
		qDebug() << "Initializing the updater";
		[[maybe_unused]] const auto startUpdater =
			[this](ExternalUpdater* updater) {
				m_updater.reset(updater);
				m_updater->setBetaAllowed(BuildConfig.UPDATE_CHANNEL ==
										  QLatin1String("beta"));
				qDebug() << "<> Updater started.";
				connectUpdaterToQmlShell();
			};
#if defined(Q_OS_MAC)
#if defined(KITE_SPARKLE_ENABLED)
		startUpdater(new MacSparkleUpdater());
#endif
#else
		/* The "On Launch" check asks questions from the constructor. Nothing
		 * can answer yet, so it is made once the QML presenter is up. */
		runWhenQmlPresenterReady([this, startUpdater]() {
			startUpdater(new KiteExternalUpdater(
				m_rootPath, m_dataPath,
				m_settings->get("AutoUpdate").toBool()));
		});
#endif
	} else {
		qDebug() << "<> Updater not available for this build.";
	}

	{
		auto setting = APPLICATION->settings()->getSetting("IconsDir");
		QStringList instFolders = {":/icons/multimc/32x32/instances/",
								   ":/icons/multimc/50x50/instances/",
								   ":/icons/multimc/128x128/instances/",
								   ":/icons/multimc/scalable/instances/"};
		m_icons.reset(new IconList(instFolders, setting->get().toString()));
		connect(setting.get(), &Setting::SettingChanged,
				[&](const Setting&, QVariant value) {
					m_icons->directoryChanged(value.toString());
				});
		qDebug() << "<> Instance icons intialized.";
	}

	{
		// Built-in instance icons resolve through QIcon::fromTheme().
		auto searchPaths = QIcon::themeSearchPaths();
		searchPaths.prepend(QStringLiteral(":/icons"));
		QIcon::setThemeSearchPaths(searchPaths);
		QIcon::setThemeName(QStringLiteral("multimc"));
		QIcon::setFallbackThemeName(QStringLiteral("multimc"));
		qDebug() << "<> Icon theme initialized.";
	}

	// The folder Appearance tells users to drop their own cat pictures into.
	if (!QDir().mkpath(QStringLiteral("catpacks"))) {
		qWarning() << "Couldn't create catpacks folder";
	}

	{
		auto InstDirSetting = m_settings->getSetting("InstanceDir");
		auto AdditionalInstDirsSetting =
			m_settings->getSetting("AdditionalInstanceDirs");

		QStringList instDirs;
		instDirs << InstDirSetting->get().toString();
		instDirs << InstanceList::decodeInstanceDirList(
			AdditionalInstDirsSetting->get());

		// Warn in the log about '!' in an instance path, and remember to show
		// the dialog when the GUI starts.
		/* Every folder is checked: the '!' problem is Java's and does not care
		 * which folder the instance came from. */
		for (const QString& instDir : instDirs) {
			qDebug() << "Instance path              : " << instDir;
			if (FS::checkProblemticPathJava(QDir(instDir))) {
				qWarning() << "Your instance path" << instDir
						   << "contains \'!\' and this is "
							  "known to cause java problems!";
			}
		}

		m_instances.reset(new InstanceList(m_settings, instDirs, this));
		connect(InstDirSetting.get(), &Setting::SettingChanged,
				m_instances.get(), &InstanceList::on_InstFolderChanged);
		/* One slot for both settings: it re-reads the pair and rebuilds the
		 * folder list, since which folder is primary depends on both. */
		connect(AdditionalInstDirsSetting.get(), &Setting::SettingChanged,
				m_instances.get(), &InstanceList::on_InstFolderChanged);
		qDebug() << "Loading Instances...";
		m_instances->loadList();
		qDebug() << "<> Instances loaded.";
	}

	{
		m_accounts.reset(new AccountList(this));
		qDebug() << "Loading accounts...";
		m_accounts->setListFilePath("accounts.json", true);
		m_accounts->loadList();
		m_accounts->fillQueue();
		qDebug() << "<> Accounts loaded.";
	}

	{
		m_metacache.reset(new HttpMetaCache("metacache"));
		m_metacache->addBase("asset_indexes",
							 QDir("assets/indexes").absolutePath());
		m_metacache->addBase("asset_objects",
							 QDir("assets/objects").absolutePath());
		m_metacache->addBase("versions", QDir("versions").absolutePath());
		m_metacache->addBase("libraries", QDir("libraries").absolutePath());
		m_metacache->addBase("minecraftforge",
							 QDir("mods/minecraftforge").absolutePath());
		m_metacache->addBase("fmllibs",
							 QDir("mods/minecraftforge/libs").absolutePath());
		m_metacache->addBase("liteloader",
							 QDir("mods/liteloader").absolutePath());
		m_metacache->addBase("general", QDir("cache").absolutePath());
		m_metacache->addBase("ATLauncherPacks",
							 QDir("cache/ATLauncherPacks").absolutePath());
		m_metacache->addBase("ModpacksCHPacks",
							 QDir("cache/ModpacksCHPacks").absolutePath());
		m_metacache->addBase("root", QDir::currentPath());
		m_metacache->addBase("translations",
							 QDir("translations").absolutePath());
		m_metacache->addBase("icons", QDir("cache/icons").absolutePath());
		m_metacache->addBase("meta", QDir("meta").absolutePath());
		m_metacache->Load();
		qDebug() << "<> Cache initialized.";
	}

	m_translations->downloadIndex();

	// FIXME: what to do with these?
	m_profilers.insert("jprofiler", std::make_shared<JProfilerFactory>());
	m_profilers.insert("jvisualvm", std::make_shared<JVisualVMFactory>());
	for (auto profiler : m_profilers.values()) {
		profiler->registerSettings(m_settings);
	}

	// The path is edited in the external-tools settings; the setting has to
	// exist for that.
	m_settings->registerSetting("MCEditPath");

	connect(this, &Application::aboutToQuit, this,
			[this]() { shutdownForExit(); });
}

void Application::shutdownForExit()
{
	if (m_instances) {
		m_instances->saveNow();
	}
	// Shut plugins down while the log file and Qt are still alive.
	if (m_pluginManager) {
		m_pluginManager->shutdownAll();
	}
	if (logFile) {
		logFile->flush();
		logFile->close();
	}
}

void Application::performCLIAction()
{
	if (m_cliListInstances) {
		auto instList = instances();
		int count = instList->count();
		if (count == 0) {
			fprintf(stdout, "No instances found.\n");
		} else {
			fprintf(stdout, "%-24s %-30s %-12s %s\n", "ID", "Name",
					"MC Version", "Group");
			for (int i = 0; i < 80; i++)
				fputc('-', stdout);
			fputc('\n', stdout);
			for (int i = 0; i < count; i++) {
				auto inst = instList->at(i);
				if (!inst)
					continue;
				QString group = instList->getInstanceGroup(inst->id());
				QString mcVer = "?";
				auto* mcInst = dynamic_cast<MinecraftInstance*>(inst.get());
				if (mcInst) {
					auto profile = mcInst->getPackProfile();
					if (profile)
						mcVer = profile->getComponentVersion("net.minecraft");
				}
				fprintf(
					stdout, "%-24s %-30s %-12s %s\n", qPrintable(inst->id()),
					qPrintable(inst->name()), qPrintable(mcVer),
					qPrintable(group.isEmpty() ? QStringLiteral("-") : group));
			}
		}
		fprintf(stdout, "\nTotal: %d instance(s)\n", count);
		m_status = Application::Succeeded;
		return;
	}

	if (!m_cliInstanceInfoId.isEmpty()) {
		auto inst = instances()->getInstanceById(m_cliInstanceInfoId);
		if (!inst) {
			fprintf(stderr, "Error: Instance '%s' not found.\n",
					qPrintable(m_cliInstanceInfoId));
			m_status = Application::Failed;
			return;
		}
		fprintf(stdout, "Instance: %s\n", qPrintable(inst->name()));
		fprintf(stdout, "ID:       %s\n", qPrintable(inst->id()));
		fprintf(stdout, "Path:     %s\n", qPrintable(inst->instanceRoot()));
		fprintf(stdout, "Type:     %s\n", qPrintable(inst->typeName()));
		fprintf(stdout, "Group:    %s\n",
				qPrintable(instances()->getInstanceGroup(inst->id())));

		auto* mcInst = dynamic_cast<MinecraftInstance*>(inst.get());
		if (mcInst) {
			auto profile = mcInst->getPackProfile();
			if (profile) {
				fprintf(
					stdout, "MC Ver:   %s\n",
					qPrintable(profile->getComponentVersion("net.minecraft")));
				fprintf(stdout, "\nComponents:\n");
				for (int i = 0; i < profile->rowCount(); i++) {
					auto comp = profile->getComponent(i);
					if (comp) {
						fprintf(stdout, "  %s %s\n",
								qPrintable(comp->getName()),
								qPrintable(comp->getVersion()));
					}
				}
			}
		}

		auto totalTime = inst->totalTimePlayed();
		if (totalTime > 0) {
			int hours = totalTime / 3600;
			int mins = (totalTime % 3600) / 60;
			fprintf(stdout, "\nPlay time: %dh %dm\n", hours, mins);
		}

		auto lastLaunch = inst->lastLaunch();
		if (lastLaunch > 0) {
			fprintf(stdout, "Last launched: %s\n",
					qPrintable(QDateTime::fromSecsSinceEpoch(lastLaunch)
								   .toString(Qt::ISODate)));
		}

		m_status = Application::Succeeded;
		return;
	}

	if (!m_cliExportId.isEmpty()) {
		auto inst = instances()->getInstanceById(m_cliExportId);
		if (!inst) {
			fprintf(stderr, "Error: Instance '%s' not found.\n",
					qPrintable(m_cliExportId));
			m_status = Application::Failed;
			return;
		}

		fprintf(stdout, "Exporting '%s' to %s ...\n", qPrintable(inst->name()),
				qPrintable(m_cliOutputPath));
		fflush(stdout);

		QString srcPath = inst->instanceRoot();
		auto noFilter = [](const QString&) { return false; };
		if (!MMCZip::compressDir(m_cliOutputPath, srcPath, noFilter)) {
			fprintf(stderr, "Error: Failed to export instance.\n");
			m_status = Application::Failed;
			return;
		}

		fprintf(stdout, "Export complete.\n");
		m_status = Application::Succeeded;
		return;
	}

	m_status = Application::Failed;
}

void Application::performMainStartupAction()
{
	m_status = Application::Initialized;
	if (!m_instanceIdToLaunch.isEmpty()) {
		auto inst = instances()->getInstanceById(m_instanceIdToLaunch);
		if (inst) {
			MinecraftServerTargetPtr serverToJoin = nullptr;
			MinecraftAccountPtr accountToUse = nullptr;

			qDebug() << "<> Instance" << m_instanceIdToLaunch << "launching";
			if (!m_serverToJoin.isEmpty()) {
				// FIXME: validate the server string
				serverToJoin.reset(new MinecraftServerTarget(
					MinecraftServerTarget::parse(m_serverToJoin, false)));
				qDebug() << "   Launching with server" << m_serverToJoin;
			} else if (!m_worldToJoin.isEmpty()) {
				serverToJoin.reset(new MinecraftServerTarget(
					MinecraftServerTarget::parse(m_worldToJoin, true)));
				qDebug() << "   Launching with world" << m_worldToJoin;
			}

			if (!m_profileToUse.isEmpty()) {
				accountToUse =
					accounts()->getAccountByProfileName(m_profileToUse);
				if (!accountToUse) {
					return;
				}
				qDebug() << "   Launching with account" << m_profileToUse;
			}

			/* A command-line launch opens no main window. The shell is still
			 * loaded, hidden, so questions are asked there. */
			if (!ensureQmlShell()) {
				return;
			}
			launch(inst, LaunchMode::Normal, serverToJoin, accountToUse);
			return;
		}
	}
	if (!showMainWindow(false)) {
		return;
	}
	qDebug() << "<> Main window shown.";
	if (!m_zipToImport.isEmpty()) {
		qDebug() << "<> Importing instance from zip:" << m_zipToImport;
		importFromUrl(m_zipToImport);
	}
}

void Application::importFromUrl(const QUrl& url)
{
	/* An import asks its questions in a window; a launcher started with
	 * --launch has none on screen, so bring the usual one up first. */
	if (!m_qmlWindowShown && !showMainWindow()) {
		return;
	}
	m_qmlShell->requestImport(FS::pathFromUrl(url));
}

void Application::showFatalErrorMessage(const QString& title,
										const QString& content)
{
	m_status = Application::Failed;
	QMessageBox::critical(nullptr, title, content);
}

Application::~Application()
{
	/* Stop handing out a context that is being torn down; anything still
	 * running must cope with LAUNCHER being null. */
	LauncherContext::setInstance(nullptr);

	// Shut the plugin system down first. shutdownAll() already ran from
	// aboutToQuit; this guards other exit paths.
	if (m_pluginManager) {
		m_pluginManager->shutdownAll();
	}

	qInstallMessageHandler(nullptr);

#if defined Q_OS_WIN32
	if (consoleAttached) {
		fclose(stdout);
		fclose(stdin);
		fclose(stderr);
		FreeConsole();
	}
#endif
}

void Application::messageReceived(const QByteArray& message)
{
	if (status() != Initialized) {
		qDebug() << "Received message" << message
				 << "while still initializing. It will be ignored.";
		return;
	}

	ApplicationMessage received;
	received.parse(message);

	auto& command = received.command;

	if (command == "activate") {
		showMainWindow();
	} else if (command == "import") {
		QString path = received.args["path"];
		if (path.isEmpty()) {
			qWarning() << "Received" << command
					   << "message without a zip path/URL.";
			return;
		}
		importFromUrl(QUrl(path));
	} else if (command == "launch") {
		QString id = received.args["id"];
		QString server = received.args["server"];
		QString world = received.args["world"];
		QString profile = received.args["profile"];

		InstancePtr instance;
		if (!id.isEmpty()) {
			instance = instances()->getInstanceById(id);
			if (!instance) {
				qWarning() << "Launch command requires an valid instance ID. "
						   << id << "resolves to nothing.";
				return;
			}
		} else {
			qWarning() << "Launch command called without an instance ID...";
			return;
		}

		MinecraftServerTargetPtr serverObject = nullptr;
		if (!server.isEmpty()) {
			serverObject = std::make_shared<MinecraftServerTarget>(
				MinecraftServerTarget::parse(server, false));
		} else if (!world.isEmpty()) {
			serverObject = std::make_shared<MinecraftServerTarget>(
				MinecraftServerTarget::parse(world, true));
		}

		MinecraftAccountPtr accountObject;
		if (!profile.isEmpty()) {
			accountObject = accounts()->getAccountByProfileName(profile);
			if (!accountObject) {
				qWarning() << "Launch command requires the specified"
						   << "profile to be valid." << profile
						   << "does not resolve to any account.";
				return;
			}
		}

		launch(instance, LaunchMode::Normal, serverObject, accountObject);
	} else {
		qWarning() << "Received invalid message" << message;
	}
}

std::shared_ptr<TranslationsModel> Application::translations()
{
	return m_translations;
}

std::shared_ptr<JavaInstallList> Application::javalist()
{
	if (!m_javalist) {
		m_javalist.reset(new JavaInstallList());
	}
	return m_javalist;
}

bool Application::launch(InstancePtr instance, LaunchMode mode,
						 MinecraftServerTargetPtr serverToJoin,
						 MinecraftAccountPtr accountToUse)
{
	/* LaunchController asks everything through UiHost, so an early launch is
	 * held until the QML presenter is up. */
	if (!usingQmlShell()) {
		runWhenQmlPresenterReady(
			[this, instance, mode, serverToJoin, accountToUse]() {
				launch(instance, mode, serverToJoin, accountToUse);
			});
		return true;
	}
	if (BackupController::isRestoring(instance->id())) {
		uiHost()->message(
			tr("Backup Restoring"),
			tr("A backup is being restored into this instance. Launch it "
			   "once the restore is done."),
			UiHost::Severity::Information);
		return false;
	}
	if (instance->canLaunch()) {
		auto& extras = m_instanceExtras[instance->id()];
		auto& controller = extras.controller;
		controller.reset(new LaunchController());
		controller->setInstance(instance);
		controller->setOnline(mode != LaunchMode::Offline);
		controller->setDemoMode(mode == LaunchMode::Demo);

		/* The profiler follows the instance, not the click. An unknown key is
		 * left alone (this build has no such profiler); a known but
		 * misconfigured one is still handed over, so LaunchController reports
		 * it. */
		const QString profilerKey = instance->profilerKey();
		if (!profilerKey.isEmpty()) {
			auto profiler = m_profilers.value(profilerKey, nullptr);
			if (profiler) {
				controller->setProfiler(profiler.get());
			} else {
				qWarning() << "Instance" << instance->id()
						   << "asks for profiler" << profilerKey
						   << "which this build does not have. Launching "
							  "without a profiler.";
			}
		}

		controller->setServerToJoin(serverToJoin);
		controller->setAccountToUse(accountToUse);
		connect(controller.get(), &LaunchController::succeeded, this,
				&Application::controllerSucceeded);
		connect(controller.get(), &LaunchController::failed, this,
				&Application::controllerFailed);
		addRunningInstance();
		controller->start();
		return true;
	} else if (instance->isRunning()) {
		// Raises the shell's window on the instance's Log tab.
		showInstanceLog(instance);
		return true;
	} else if (instance->canEdit()) {
		showQmlShellWindow(false);
		m_qmlShell->showInstanceLogRequested(instance->id());
		return true;
	}
	return false;
}

bool Application::kill(InstancePtr instance)
{
	if (!instance->isRunning()) {
		qWarning() << "Attempted to kill instance" << instance->id()
				   << ", which isn't running.";
		return false;
	}
	auto& extras = m_instanceExtras[instance->id()];
	// NOTE: copy of the shared pointer keeps it alive
	auto controller = extras.controller;
	if (controller) {
		return controller->abort();
	}
	return true;
}

void Application::addRunningInstance()
{
	m_runningInstances++;
}

void Application::subRunningInstance()
{
	if (m_runningInstances == 0) {
		qCritical() << "Something went really wrong and we now have less "
					   "than 0 running instances... WTF";
		return;
	}
	m_runningInstances--;
}

bool Application::shouldExitNow() const
{
	return m_runningInstances == 0 && m_openWindows == 0;
}

QString Application::updaterBinaryName()
{
	// Kept in step with KiteExternalUpdater::updaterBinaryRelativePath(),
	// which actually launches it; duplicated because the macOS build does not
	// compile that class.
#if defined(Q_OS_WIN32)
	return QStringLiteral("kite-updater.exe");
#else
	return QStringLiteral("bin/kite-updater");
#endif
}

bool Application::updaterEnabled()
{
#if defined(Q_OS_MAC)
	// Sparkle is linked in, so there is no separate binary to look for.
#if defined(KITE_SPARKLE_ENABLED)
	return BuildConfig.UPDATER_ENABLED;
#else
	return false;
#endif
#else
	// A distribution package takes over updating and does not ship the
	// updater; offering updates would show a menu entry that can only fail.
	return BuildConfig.UPDATER_ENABLED &&
		   QFileInfo(FS::PathCombine(m_rootPath, updaterBinaryName())).isFile();
#endif
}

void Application::triggerUpdateCheck()
{
	if (!m_updater) {
		qWarning() << "The updater is not available; cannot check for "
					  "updates.";
		return;
	}

	qDebug() << "Checking for updates.";
	m_updater->checkForUpdates();
}

void Application::connectUpdaterToQmlShell()
{
	if (!m_qmlShell) {
		return;
	}
	m_qmlShell->setUpdaterAvailable(updaterEnabled() && m_updater);
	if (m_updater && updaterEnabled()) {
		ExternalUpdater* updater = m_updater.get();
		m_qmlShell->updateSettingsController()->setBackend(
			{[updater] { return updater->getAutomaticallyChecksForUpdates(); },
			 [updater](bool v) { updater->setAutomaticallyChecksForUpdates(v); },
			 [updater] { return updater->getUpdateCheckInterval(); },
			 [updater](double s) { updater->setUpdateCheckInterval(s); },
			 [this] { triggerUpdateCheck(); }});
	}
}

void Application::reportUpdateMarkers()
{
	const QString updateLog = UpdateLockFile::updateLogPath(m_dataPath);

	/* Runs during startup, before the shell exists, so no window can answer a
	 * UiHost call yet: queue it. ensureQmlShell() runs the queue once the
	 * QmlUiHost reports presenterReady(). */

	// A leftover lock means an update started and never finished, so the
	// installation may mix two versions.
	const QString lockPath = UpdateLockFile::lockPath(m_dataPath);
	if (QFileInfo::exists(lockPath)) {
		UpdateLockFile::Contents lock;
		UpdateLockFile::read(lockPath, &lock);

		const QString text =
			tr("This installation has an update lock file at: %1\n"
			   "\n"
			   "Timestamp: %2\n"
			   "Updating from version %3 to %4\n"
			   "Target install path: %5\n"
			   "Data path: %6\n"
			   "\n"
			   "This usually means an update attempt failed. "
			   "Please make sure your installation still works "
			   "before continuing.\n"
			   "The updater log at:\n"
			   "%7\n"
			   "has the details of the last attempt.\n"
			   "\n"
			   "To delete this lock and continue, choose "
			   "\"Ignore\".")
				.arg(QDir::toNativeSeparators(lockPath),
					 lock.timestamp.toString(Qt::ISODate),
					 lock.from, lock.to, lock.target,
					 lock.dataPath,
					 QDir::toNativeSeparators(updateLog));

		m_pendingQmlPresenterCalls.append([this, lockPath, text]() {
			const bool ignore = uiHost()->confirm(
				tr("Update In Progress"), text, UiHost::Severity::Warning,
				tr("Ignore"), tr("Quit"));
			if (!ignore) {
				qDebug() << "Quitting because an update lock file is present.";
				abortStartup();
				return;
			}
			QFile::remove(lockPath);
		});
	}

	const QString failMarker = UpdateLockFile::markerPath(
		m_dataPath, QLatin1String(UpdateLockFile::kFailMarkerName));
	if (QFileInfo::exists(failMarker)) {
		const QString text = tr("An update attempt failed.\n"
								"\n"
								"Please make sure your installation still "
								"works before continuing.\n"
								"The updater log at:\n"
								"%1\n"
								"has the details of the last attempt.")
								 .arg(QDir::toNativeSeparators(updateLog));

		m_pendingQmlPresenterCalls.append([this, failMarker, text]() {
			const bool ignore = uiHost()->confirm(
				tr("Update Failed"), text, UiHost::Severity::Warning,
				tr("Ignore"), tr("Quit"));
			if (!ignore) {
				qDebug() << "Quitting because the last update failed.";
				abortStartup();
				return;
			}
			QFile::remove(failMarker);
		});
	}

	const QString successMarker = UpdateLockFile::markerPath(
		m_dataPath, QLatin1String(UpdateLockFile::kSuccessMarkerName));
	if (QFileInfo::exists(successMarker)) {
		const QString text = tr("The update succeeded.\n"
								"\n"
								"You are now running %1.\n"
								"The updater log at:\n"
								"%2\n"
								"has the details.")
								 .arg(BuildConfig.printableVersionString(),
									  QDir::toNativeSeparators(updateLog));

		m_pendingQmlPresenterCalls.append([this, text]() {
			uiHost()->message(tr("Update Succeeded"), text,
							  UiHost::Severity::Information);
		});
		QFile::remove(successMarker);
	}
}

void Application::abortStartup()
{
	/* quit() does nothing before exec() runs, and these prompts are answered
	 * during construction. The failed status makes main() exit with 1; the
	 * queued exit covers an answer that arrives once the loop is running. */
	m_status = Application::Failed;
	m_startupAborted = true;
	QMetaObject::invokeMethod(
		this, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
}

void Application::controllerSucceeded()
{
	auto controller = qobject_cast<LaunchController*>(QObject::sender());
	if (!controller)
		return;
	auto id = controller->id();
	auto& extras = m_instanceExtras[id];

	extras.controller.reset();
	subRunningInstance();

	if (shouldExitNow()) {
		m_status = Status::Succeeded;
		// Qualified so a member or free function named exit() cannot turn this
		// into ::exit() (libc, no Qt shutdown).
		QCoreApplication::exit(0);
	}
}

void Application::controllerFailed(const QString& error)
{
	Q_UNUSED(error);
	auto controller = qobject_cast<LaunchController*>(QObject::sender());
	if (!controller)
		return;
	auto id = controller->id();
	auto& extras = m_instanceExtras[id];

	extras.controller.reset();
	subRunningInstance();

	if (shouldExitNow()) {
		m_status = Status::Failed;
		QCoreApplication::exit(1);
	}
}

bool Application::ensureQmlShell()
{
	if (m_qmlShell) {
		return !m_startupAborted;
	}
	m_qmlShell = std::make_unique<QmlShell>();

	/* QmlShell cannot see PluginManager (Kite_qml does not link Kite_logic),
	 * so this factory is the bridge. */
	QmlShell::setPluginSurfaceFactory(
		[this](int anchor, const QString& anchorContext) -> QObject* {
			if (!m_pluginManager) {
				return nullptr;
			}
			return QmlShell::expose(new PluginSurfaceModel(
				m_pluginManager.get(), anchor, anchorContext));
		});

	m_qmlShell->setProfilersProvider([this]() {
		QVariantList list;
		for (auto it = m_profilers.constBegin(); it != m_profilers.constEnd();
			 ++it) {
			QString error;
			list.append(QVariantMap{
				{ QStringLiteral("key"), it.key() },
				{ QStringLiteral("name"), it.value()->name() },
				{ QStringLiteral("ready"), it.value()->check(&error) },
			});
		}
		return list;
	});

	/* Settings' "Installed plugins": nothing to list without the plugin
	 * host (Kite_PLUGINS off). */
	m_qmlShell->setPluginListFactory([this]() -> QObject* {
		return m_pluginManager ? new PluginListModel(m_pluginManager.get())
							   : nullptr;
	});

	/* The launcher quits when its last window goes, but only while
	 * showQmlShellWindow() counts it as open (a hidden shell is not a window
	 * anyone closed). */
	connect(m_qmlShell.get(), &QmlShell::closed, this, [this]() {
		if (!m_qmlWindowShown) {
			return;
		}
		m_qmlWindowShown = false;
		m_openWindows--;
		if (shouldExitNow()) {
			QCoreApplication::exit(0);
		}
	});

	/* QmlShell cannot see the launcher's actions: it emits these signals and
	 * they are wired up here. */
	connect(m_qmlShell.get(), &QmlShell::launchRequested, this,
			[this](const QString& id) {
				if (auto inst = instances()->getInstanceById(id)) {
					launch(inst);
				}
			});
	connect(m_qmlShell.get(), &QmlShell::launchModeRequested, this,
			[this](const QString& id, const QString& mode) {
				auto inst = instances()->getInstanceById(id);
				if (!inst) {
					return;
				}
				if (mode == QLatin1String("offline")) {
					launch(inst, LaunchMode::Offline);
				} else if (mode == QLatin1String("demo")) {
					auto minecraft =
						std::dynamic_pointer_cast<MinecraftInstance>(inst);
					if (!minecraft || !minecraft->supportsDemo()) {
						qWarning() << "Application: no demo launch for" << id
								   << "-- its Minecraft version has no demo";
						return;
					}
					launch(inst, LaunchMode::Demo);
				} else {
					launch(inst);
				}
			});
	connect(m_qmlShell.get(), &QmlShell::launchAsRequested, this,
			[this](const QString& id, const QString& accountId) {
				auto inst = instances()->getInstanceById(id);
				if (!inst) {
					return;
				}
				// The id QmlShell::launchOptions() hands out.
				MinecraftAccountPtr account;
				for (int i = 0; i < m_accounts->count(); ++i) {
					const auto candidate = m_accounts->at(i);
					if (candidate->accountId() == accountId) {
						account = candidate;
						break;
					}
				}
				if (!account) {
					qWarning() << "Application: no account" << accountId
							   << "to launch" << id << "with";
					return;
				}
				launch(inst, LaunchMode::Normal, nullptr, account);
			});
	/* QmlShell cannot reach the updater (macOS menu bar, Updates settings). */
	connect(m_qmlShell.get(), &QmlShell::checkForUpdatesRequested, this,
			&Application::triggerUpdateCheck);
	connectUpdaterToQmlShell();
	connect(m_qmlShell.get(), &QmlShell::killRequested, this,
			[this](const QString& id) {
				if (auto inst = instances()->getInstanceById(id)) {
					kill(inst);
				}
			});
	connect(m_qmlShell.get(), &QmlShell::joinServerRequested, this,
			[this](const QString& id, const QString& address) {
				if (auto inst = instances()->getInstanceById(id)) {
					launch(inst, LaunchMode::Normal,
						   std::make_shared<MinecraftServerTarget>(
							   MinecraftServerTarget::parse(address, false)));
				}
			});
	connect(m_qmlShell.get(), &QmlShell::folderRequested, this,
			[this](const QString& id) {
				if (auto inst = instances()->getInstanceById(id)) {
					DesktopServices::openDirectory(inst->instanceRoot(), true);
				}
			});

	if (!m_qmlShell->load()) {
		qCritical() << "QML shell failed to load";
		m_qmlShell.reset();
		m_pendingQmlPresenterCalls.clear();
		const QString logPath =
			logFile ? QDir::toNativeSeparators(
						  QFileInfo(*logFile).absoluteFilePath())
					: m_dataPath;
		showFatalErrorMessage(
			tr("Kite could not start"),
			tr("The interface failed to load.\n\nThe log file has the "
			   "details:\n%1")
				.arg(logPath));
		return false;
	}

	/* Runs what waited for the presenter once QmlUiHost can show a request.
	 * Wired here, not after QmlShell's construction: it builds its QmlUiHost
	 * inside load(), so uiHost() is null before this point. */
	if (auto* host = qobject_cast<QmlUiHost*>(m_qmlShell->uiHost())) {
		connect(host, &QmlUiHost::presenterReadyChanged, this,
				[this, host]() {
					if (host->presenterReady()) {
						runPendingQmlPresenterCalls();
					}
				});

		const auto reveal = [this, host]() {
			if (!m_qmlWindowShown && (host->current() || host->busy())) {
				showQmlShellWindow(false);
			}
		};
		connect(host, &QmlUiHost::currentChanged, this, reveal);
		connect(host, &QmlUiHost::busyChanged, this, reveal);

		/* presenterReady() is normally already true here
		 * (Component.onCompleted ran during load()), so
		 * presenterReadyChanged() would not fire again; run the queue once
		 * here too. */
		if (host->presenterReady()) {
			runPendingQmlPresenterCalls();
		}
	}
	return !m_startupAborted;
}

void Application::showQmlShellWindow(bool minimized)
{
	if (!m_qmlShell || !m_qmlShell->show(minimized) || m_qmlWindowShown) {
		return;
	}
	m_qmlWindowShown = true;
	m_openWindows++;

	/* MMCO_HOOK_UI_MAIN_READY fires once here, the first time the root
	 * window is on screen, with every widget handle null. Plugins are
	 * initialised by then. */
	if (!m_qmlMainReadyDispatched) {
		m_qmlMainReadyDispatched = true;
		if (m_pluginManager) {
			MMCOUiMainReadyPayload mainReady{};
			m_pluginManager->dispatchHook(MMCO_HOOK_UI_MAIN_READY,
										  &mainReady);
			bridgeInstanceHooks();
		}
	}
}

/* Instances present at UI_MAIN_READY are the plugins' to pick up there; from then on every
 * instance added to the list is announced with INSTANCE_CREATED, and every one that left it with
 * INSTANCE_REMOVED (after it is gone from the list, so a plugin re-reading the list does not see it). */
void Application::bridgeInstanceHooks()
{
	if (!m_instances || !m_pluginManager) {
		return;
	}
	struct InstanceStrings {
		QByteArray id;
		QByteArray name;
		QByteArray path;
	};
	auto dispatchStrings = [this](uint32_t hookId, const InstanceStrings& s) {
		MMCOInstanceInfo info{};
		info.instance_id = s.id.constData();
		info.instance_name = s.name.constData();
		info.instance_path = s.path.constData();
		info.minecraft_version = nullptr;
		m_pluginManager->dispatchHook(hookId, &info);
	};
	auto strings = [](const InstancePtr& inst) {
		return InstanceStrings{inst->id().toUtf8(), inst->name().toUtf8(),
							   inst->instanceRoot().toUtf8()};
	};
	auto pendingRemoved = std::make_shared<QList<InstanceStrings>>();
	connect(m_instances.get(), &QAbstractItemModel::rowsInserted, this,
			[this, dispatchStrings, strings](const QModelIndex&, int first, int last) {
				for (int row = first; row <= last && row < m_instances->count(); ++row) {
					if (InstancePtr inst = m_instances->at(row)) {
						dispatchStrings(MMCO_HOOK_INSTANCE_CREATED, strings(inst));
					}
				}
			});
	connect(m_instances.get(), &QAbstractItemModel::rowsAboutToBeRemoved, this,
			[this, strings, pendingRemoved](const QModelIndex&, int first, int last) {
				pendingRemoved->clear();
				for (int row = first; row <= last && row < m_instances->count(); ++row) {
					if (InstancePtr inst = m_instances->at(row)) {
						pendingRemoved->append(strings(inst));
					}
				}
			});
	connect(m_instances.get(), &QAbstractItemModel::rowsRemoved, this,
			[dispatchStrings, pendingRemoved](const QModelIndex&, int, int) {
				const QList<InstanceStrings> removed = std::move(*pendingRemoved);
				pendingRemoved->clear();
				for (const InstanceStrings& s : removed) {
					dispatchStrings(MMCO_HOOK_INSTANCE_REMOVED, s);
				}
			});
}

void Application::navigateQmlShell(const QString& target)
{
	if (!m_qmlShell) {
		return;
	}
	showQmlShellWindow(false);
	m_qmlShell->requestNavigate(target);
}

bool Application::showMainWindow(bool minimized)
{
	if (!ensureQmlShell()) {
		return false;
	}
	showQmlShellWindow(minimized);
	startBackgroundChecks();
	return true;
}

void Application::startBackgroundChecks()
{
	if (m_backgroundChecksStarted) {
		return;
	}
	m_backgroundChecksStarted = true;
	m_newsChecker->reloadNews();
	m_notificationChecker->checkForNotifications();
	runWhenQmlPresenterReady([this]() { reportInstancePathProblems(); });
}

void Application::showNotificationsThroughUiHost()
{
	QList<int> shown = NotificationChecker::parseShownIds(
		m_settings->get("ShownNotifications").toString());
	bool changed = false;
	for (const auto& entry : m_notificationChecker->notificationEntries()) {
		if (shown.contains(entry.id)) {
			continue;
		}
		UiHost::Severity severity = UiHost::Severity::Information;
		if (entry.type == NotificationChecker::NotificationEntry::Critical) {
			severity = UiHost::Severity::Critical;
		} else if (entry.type ==
				   NotificationChecker::NotificationEntry::Warning) {
			severity = UiHost::Severity::Warning;
		}
		// Two buttons; backing out is "Close".
		const int choice = uiHost()->choose(
			tr("Notification"), entry.message, severity,
			{ tr("Close"), tr("Don't show again") });
		if (choice == 1) {
			shown.append(entry.id);
			changed = true;
		}
	}
	if (changed) {
		m_settings->set("ShownNotifications",
						NotificationChecker::formatShownIds(shown));
	}
}

void Application::reportInstancePathProblems()
{
	const auto problems =
		instancePathProblems(instances()->instanceDirs(), QDir::tempPath());
	for (const auto& problem : problems) {
		uiHost()->message(tr("Check your instance folder"),
						  problem.text + QStringLiteral("<br/><br/>") +
							  problem.advice,
						  UiHost::Severity::Warning);
	}
}

QWindow* Application::qmlShellWindow() const
{
	return m_qmlShell ? m_qmlShell->window() : nullptr;
}

QString Application::msaClientId() const
{
	return BuildConfig.MSAClientID;
}

void Application::updateProxySettings(QString proxyTypeStr, QString addr,
									  int port, QString user, QString password)
{
	if (proxyTypeStr == "SOCKS5") {
		QNetworkProxy::setApplicationProxy(QNetworkProxy(
			QNetworkProxy::Socks5Proxy, addr, port, user, password));
	} else if (proxyTypeStr == "HTTP") {
		QNetworkProxy::setApplicationProxy(QNetworkProxy(
			QNetworkProxy::HttpProxy, addr, port, user, password));
	} else if (proxyTypeStr == "None") {
		QNetworkProxy::setApplicationProxy(
			QNetworkProxy(QNetworkProxy::NoProxy));
	} else {
		QNetworkProxyFactory::setUseSystemConfiguration(true);
	}

	qDebug() << "Detecting proxy settings...";
	QNetworkProxy proxy = QNetworkProxy::applicationProxy();
	m_network->setProxy(proxy);

	QString proxyDesc;
	if (proxy.type() == QNetworkProxy::NoProxy) {
		qDebug() << "Using no proxy is an option!";
		return;
	}
	switch (proxy.type()) {
		case QNetworkProxy::DefaultProxy:
			proxyDesc = "Default proxy: ";
			break;
		case QNetworkProxy::Socks5Proxy:
			proxyDesc = "Socks5 proxy: ";
			break;
		case QNetworkProxy::HttpProxy:
			proxyDesc = "HTTP proxy: ";
			break;
		case QNetworkProxy::HttpCachingProxy:
			proxyDesc = "HTTP caching: ";
			break;
		case QNetworkProxy::FtpCachingProxy:
			proxyDesc = "FTP caching: ";
			break;
		default:
			proxyDesc = "DERP proxy: ";
			break;
	}
	proxyDesc += QString("%1:%2").arg(proxy.hostName()).arg(proxy.port());
	qDebug() << proxyDesc;
}

shared_qobject_ptr<HttpMetaCache> Application::metacache()
{
	return m_metacache;
}

shared_qobject_ptr<QNetworkAccessManager> Application::network()
{
	return m_network;
}

shared_qobject_ptr<Meta::Index> Application::metadataIndex()
{
	if (!m_metadataIndex) {
		m_metadataIndex.reset(new Meta::Index());
	}
	return m_metadataIndex;
}

QString Application::getJarsPath()
{
	if (m_jarsPath.isEmpty()) {
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
		auto appDir = QCoreApplication::applicationDirPath();
		auto installedPath =
			FS::PathCombine(appDir, "..", "share", BuildConfig.KITE_NAME);
		if (QDir(installedPath).exists()) {
			return installedPath;
		}
		return FS::PathCombine(appDir, "jars");
#elif defined(Q_OS_MAC)
		// macOS: Java archives live in <bundle>/Contents/Resources/jars, not
		// Contents/MacOS, because codesign refuses to seal the main executable
		// while non-Mach-O files sit there. Contents/MacOS/jars is still
		// checked as a fallback for older installs and developer builds.
		auto appDir = QCoreApplication::applicationDirPath();
		auto resourcesJars = FS::PathCombine(appDir, "..", "Resources", "jars");
		if (QDir(resourcesJars).exists()) {
			return resourcesJars;
		}
		return FS::PathCombine(appDir, "jars");
#else
		return FS::PathCombine(QCoreApplication::applicationDirPath(), "jars");
#endif
	}
	return m_jarsPath;
}

AuthRequestDecorator* Application::authRequestDecorator() const
{
	return m_authRequestDecorator.get();
}

UiHost* Application::uiHost() const
{
	/* The QML shell's own UiHost once a QML item called
	 * setPresenterReady(true); until then the headless one. Call sites reach
	 * this fresh, so the answer may change between calls. */
	if (m_qmlShell) {
		if (auto* host = m_qmlShell->uiHostInterface()) {
			return host;
		}
	}
	return m_uiHost.get();
}

NewsChecker* Application::newsChecker() const
{
	return m_newsChecker.get();
}

ModBridge* Application::modBridge() const
{
	return m_modBridge.get();
}

bool Application::usingQmlShell() const
{
	return m_qmlShell && m_qmlShell->uiHostInterface() != nullptr;
}

void Application::runWhenQmlPresenterReady(std::function<void()> call)
{
	if (!usingQmlShell()) {
		m_pendingQmlPresenterCalls.append(std::move(call));
		return;
	}
	call();
}

void Application::runPendingQmlPresenterCalls()
{
	/* Taken out first: a call may queue again (e.g. a launch after the
	 * presenter went away) and must land in the next batch, not the list being
	 * walked. */
	const auto pending = std::move(m_pendingQmlPresenterCalls);
	m_pendingQmlPresenterCalls.clear();
	for (const auto& call : pending) {
		call();
		// The user chose Quit: nothing after it may run.
		if (m_startupAborted) {
			break;
		}
	}
}

void Application::showInstanceLog(InstancePtr instance)
{
	if (!instance) {
		return;
	}
	// Not made yet: shown once it is.
	if (!m_qmlShell) {
		runWhenQmlPresenterReady(
			[this, instance]() { showInstanceLog(instance); });
		return;
	}
	// The request can arrive with the QML window minimized or behind
	// others (a background crash, a second Play click): raise it as the
	// dock or taskbar icon would, so the log gets seen.
	showQmlShellWindow(false);
	m_qmlShell->showInstanceLogRequested(instance->id());
}
