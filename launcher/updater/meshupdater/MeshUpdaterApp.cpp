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

#include "MeshUpdaterApp.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <mutex>

#include "AssetMatcher.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "ReleaseArchive.h"
#include "ReleaseDownload.h"
#include "UpdaterDialogs.h"
#include "ui/dialogs/ProgressDialog.h"
#include "updater/UpdateLockFile.h"

namespace
{

	//! Releases per request; GitHub allows 100, 30 keeps the usual single page small.
	constexpr int kReleasesPerPage = 30;

	constexpr int kVersionQueryTimeoutMs = 5000;

	/*! Everything goes through the log file: this process is built for the windowing subsystem so
	 * updating does not flash a console, and has nowhere else to write. */
	void updaterMessageHandler(QtMsgType type, const QMessageLogContext& context,
							   const QString& message)
	{
		static std::mutex logMutex;
		const std::lock_guard<std::mutex> lock(logMutex);

		auto* app = static_cast<MeshUpdaterApp*>(QCoreApplication::instance());
		if (!app)
			return;

		const QString line = qFormatLogMessage(type, context, message) +
							 QLatin1Char('\n');

		if (app->logFile) {
			app->logFile->write(line.toUtf8());
			app->logFile->flush();
		}
		if (app->logToConsole) {
			QTextStream(stderr) << line;
			fflush(stderr);
		}
	}

#if defined(Q_OS_WIN32)
	/*! Borrows the console of whoever started us. A windowed executable has none, so --debug would
	 * print into the void when the updater is run by hand. */
	void attachParentConsole();
#endif

	//! "12.3 MiB", for the log and the dialogs.
	QString formatBytes(qint64 bytes)
	{
		return QLocale().formattedDataSize(bytes);
	}

	/*! Copies \a from onto \a to, replacing what is there. QFile::copy refuses to overwrite, and
	 * FS::copy has no overwrite mode and would pull in the launcher's path matchers. */
	bool copyOverFile(const QString& from, const QString& to, QString* error)
	{
		if (!QDir().mkpath(QFileInfo(to).absolutePath())) {
			*error = QStringLiteral("could not create %1")
						 .arg(QDir::toNativeSeparators(
							 QFileInfo(to).absolutePath()));
			return false;
		}

		// symlinkTarget() before exists(): a QFileInfo on a symlink describes its target.
		const QFileInfo source(from);
		const bool sourceIsSymlink = source.isSymLink();

		if ((sourceIsSymlink || QFile::exists(to)) && QFileInfo(to).exists() &&
			!QFile::remove(to)) {
			// On Windows a file that is still mapped cannot be deleted. The two-pass design exists
			// to avoid that, so if it happens something is still running.
			*error = QStringLiteral("could not replace %1")
						 .arg(QDir::toNativeSeparators(to));
			return false;
		}

		if (sourceIsSymlink) {
			// Recreate the link, not its target: lib/ is mostly soname links, and following them
			// would grow the installation by dozens of full copies.
			const QString linkTarget = source.symLinkTarget();
			const QString relativeTarget =
				QDir(source.absolutePath()).relativeFilePath(linkTarget);

			if (!QFile::link(relativeTarget, to)) {
				*error = QStringLiteral("could not link %1 -> %2")
							 .arg(QDir::toNativeSeparators(to),
								  relativeTarget);
				return false;
			}
			return true;
		}

		if (!QFile::copy(from, to)) {
			*error = QStringLiteral("could not copy %1 to %2")
						 .arg(QDir::toNativeSeparators(from),
							  QDir::toNativeSeparators(to));
			return false;
		}

		return true;
	}

	bool writeTextFile(const QString& path, const QString& contents)
	{
		if (!QDir().mkpath(QFileInfo(path).absolutePath()))
			return false;

		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
			return false;
		const QByteArray data = contents.toUtf8();
		const bool ok = file.write(data) == data.size();
		file.close();
		return ok;
	}

	QString readTextFile(const QString& path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return {};
		const QString contents = QString::fromUtf8(file.readAll());
		file.close();
		return contents;
	}

	bool appendTextFile(const QString& path, const QString& line)
	{
		if (!QDir().mkpath(QFileInfo(path).absolutePath()))
			return false;

		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
			return false;
		const QByteArray data = line.toUtf8();
		const bool ok = file.write(data) == data.size();
		file.close();
		return ok;
	}

	//! Recursively delete a directory. Best effort; failure is only logged.
	void removeDirectoryTree(const QString& path)
	{
		QDir dir(path);
		if (dir.exists() && !dir.removeRecursively())
			qWarning() << "Could not fully remove" << path;
	}

	/*! On Windows, keeps the shell from deciding that a program with "update" in its name wants
	 * administrator rights (it writes only where the launcher writes). */
	void applyInvokerCompatibility(QProcess* process)
	{
#if defined(Q_OS_WIN32)
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		env.insert(QStringLiteral("__COMPAT_LAYER"),
				   QStringLiteral("RUNASINVOKER"));
		process->setProcessEnvironment(env);
#else
		Q_UNUSED(process)
#endif
	}

	/*! Whether a downloaded file is to be unpacked and handed to the second pass rather than run as
	 * an installer. Not AssetMatcher's question, which is which file to fetch. */
	bool isArchiveName(const QString& lowerName)
	{
		return lowerName.endsWith(QLatin1String(".zip")) ||
			   lowerName.endsWith(QLatin1String(".tar.gz")) ||
			   lowerName.endsWith(QLatin1String(".tgz"));
	}

	/*! The name a first pass gives to a copy of itself. Deliberately not the updater's own name, so
	 * the copy is never mistaken for a release file and the second pass can delete it by name. */
	QString installStageAgentName()
	{
#if defined(Q_OS_WIN32)
		return QStringLiteral("kite-update-stage2.exe");
#else
		return QStringLiteral("kite-update-stage2");
#endif
	}

	/*! Where a copy of this updater runs on its own: under the data directory, not the installation,
	 * because the second pass overwrites the installation and a program cannot run from underneath
	 * itself. */
	inline constexpr auto kInstallStageDirName = "kite_update_stage2";

	constexpr int kSelfTestTimeoutMs = 15000;

	constexpr int kTakeoverTimeoutMs = 30000;

	constexpr int kFirstPassExitTimeoutMs = 30000;

	constexpr int kHandshakePollMs = 200;

	/*! Whether \a path contains \a needle as literal bytes. A gate before running a binary to ask
	 * whether it knows an option: an older updater may put up a dialog nobody dismisses and cost
	 * the full timeout. A match only earns the right to ask properly. */
	bool fileContainsBytes(const QString& path, const QByteArray& needle)
	{
		if (needle.isEmpty())
			return false;

		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return false;

		// Chunks overlap by just under the needle's length so a match across a boundary is found.
		constexpr qint64 kChunkSize = 1 << 20;
		const qint64 overlap = needle.size() - 1;
		QByteArray window;

		while (!file.atEnd()) {
			window.append(file.read(kChunkSize));
			if (window.contains(needle))
				return true;
			if (window.size() > overlap)
				window = window.right(overlap);
		}

		return false;
	}

	/*! Whether a process is still running. Tells a second pass that died before taking the lock
	 * (installation untouched) from one that has not got there yet. */
	bool processAlive(qint64 pid);

} // namespace

#if defined(Q_OS_WIN32)
#include <windows.h>

namespace
{
	void attachParentConsole()
	{
		if (!AttachConsole(ATTACH_PARENT_PROCESS))
			return;

		// Only reopen the streams we are actually going to use.
		FILE* stream = nullptr;
		freopen_s(&stream, "CONOUT$", "w", stdout);
		freopen_s(&stream, "CONOUT$", "w", stderr);
	}
} // namespace
#else
#include <cerrno>
#include <csignal>
#endif

namespace
{
	bool processAlive(qint64 pid)
	{
		if (pid <= 0)
			return false;

#if defined(Q_OS_WIN32)
		HANDLE handle =
			OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
		if (!handle)
			return false;
		const bool alive = WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
		CloseHandle(handle);
		return alive;
#else
		// EPERM: the process exists but belongs to someone else, which still counts as running.
		return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
	}
} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

bool MeshUpdaterApp::handleSelfTest(int argc, char** argv)
{
	bool asked = false;
	for (int i = 1; i < argc; ++i) {
		if (qstrcmp(argv[i], kSelfTestOption) == 0) {
			asked = true;
			break;
		}
	}
	if (!asked)
		return false;

	// Answered before anything else, so a probe needs no display or writable data directory. The
	// version rides along with the token so a failed hand-off's log says which build answered.
	QTextStream out(stdout);
	out << kSelfTestToken << ' ' << BuildConfig.printableVersionString()
		<< '\n';
	out.flush();
	return true;
}

MeshUpdaterApp::MeshUpdaterApp(int& argc, char** argv) : QApplication(argc, argv)
{
	setOrganizationName(BuildConfig.KITE_NAME);
	setOrganizationDomain(BuildConfig.KITE_DOMAIN);
	setApplicationName(BuildConfig.KITE_NAME + QStringLiteral("Updater"));
	setApplicationVersion(BuildConfig.printableVersionString() +
						  QLatin1Char('\n') + BuildConfig.GIT_COMMIT);

	QCommandLineParser parser;
	parser.setApplicationDescription(
		tr("Installs %1 updates.").arg(BuildConfig.KITE_DISPLAYNAME));
	parser.addOptions({
		{{QStringLiteral("d"), QStringLiteral("dir")},
		 tr("Use a custom path as application root (use '.' for current "
			"directory)."),
		 tr("directory")},
		{{QStringLiteral("V"), QStringLiteral("installed-version")},
		 tr("Treat this as the installed launcher version instead of asking "
			"the launcher."),
		 tr("installed version")},
		{{QStringLiteral("I"), QStringLiteral("install-version")},
		 tr("Install a specific version."), tr("version name")},
		{{QStringLiteral("U"), QStringLiteral("update-url")},
		 tr("Update from the specified repository."), tr("github repo url")},
		{QStringLiteral("finish-update"),
		 tr("Install the release already unpacked in this directory. Used by "
			"the updater itself for the second pass of an install."),
		 tr("unpacked release directory")},
		{QStringLiteral("self-test"),
		 tr("Print a token and exit, to prove this binary can run.")},
		{{QStringLiteral("c"), QStringLiteral("check-only")},
		 tr("Only check whether an update is needed. Exit status 100 if it "
			"is, 0 if it is not, non-zero on error.")},
		{{QStringLiteral("p"), QStringLiteral("pre-release")},
		 tr("Allow updating to pre-releases.")},
		{{QStringLiteral("F"), QStringLiteral("force")},
		 tr("Update even when one is not needed.")},
		{{QStringLiteral("l"), QStringLiteral("list")},
		 tr("List the available releases.")},
		{QStringLiteral("debug"), tr("Log debug output to the console.")},
		{{QStringLiteral("S"), QStringLiteral("select-ui")},
		 tr("Select the version to install with a GUI.")},
		{{QStringLiteral("D"), QStringLiteral("allow-downgrade")},
		 tr("Allow downgrading to an earlier version.")},
	});
	parser.addHelpOption();
	parser.addVersionOption();
	parser.process(arguments());

	logToConsole = parser.isSet(QStringLiteral("debug"));
#if defined(Q_OS_WIN32)
	if (logToConsole)
		attachParentConsole();
#endif

	m_checkOnly = parser.isSet(QStringLiteral("check-only"));
	m_forceUpdate = parser.isSet(QStringLiteral("force"));
	m_printOnly = parser.isSet(QStringLiteral("list"));
	m_selectUI = parser.isSet(QStringLiteral("select-ui"));
	m_allowDowngrade = parser.isSet(QStringLiteral("allow-downgrade"));
	m_allowPreRelease = parser.isSet(QStringLiteral("pre-release"));

	const QString binPath = applicationDirPath();
	const QString dirOption = parser.value(QStringLiteral("dir"));

	if (!resolvePaths(dirOption))
		return;

	if (!startLogging())
		return;

	logStartupInfo(dirOption.isEmpty() ? tr("default") : tr("command line"),
				   binPath);

	// Second pass: decided before anything that describes an installation, since this pass may be a
	// copy in a directory of its own with no launcher beside it. It needs only the staged release
	// and the data directory it was given.
	m_finishUpdateDir = parser.value(QStringLiteral("finish-update"));
	if (!m_finishUpdateDir.isEmpty()) {
		m_finishUpdateDir = QDir(m_finishUpdateDir).absolutePath();
	} else if (const QString marker =
				   QDir(m_rootPath)
					   .absoluteFilePath(
						   QLatin1String(UpdateLockFile::kUnpackMarkerName));
			   QFileInfo::exists(marker)) {
		// No --finish-update but a marker at our root: a copy inside the unpacked release, started
		// by a first pass that predates --finish-update. The root, not the binary's directory: on
		// Linux the binary is in bin/ and the marker one level up.
		m_finishUpdateDir = m_rootPath;
	}

	if (!m_finishUpdateDir.isEmpty()) {
		const QString marker =
			QDir(m_finishUpdateDir)
				.absoluteFilePath(
					QLatin1String(UpdateLockFile::kUnpackMarkerName));
		const QString target = readTextFile(marker).trimmed();
		if (target.isEmpty()) {
			// Without a target there is nowhere to install; refuse rather than overwrite the wrong
			// directory.
			showFatalError(tr("Update Failed"),
						   tr("The update marker at %1 does not say where to "
							  "install to.")
							   .arg(QDir::toNativeSeparators(marker)));
			return;
		}

		m_status = Initialized;
		const QString source = m_finishUpdateDir;
		QMetaObject::invokeMethod(
			this, [this, source, target]() { finishUpdate(source, target); },
			Qt::QueuedConnection);
		return;
	}

	{
		m_network = std::make_unique<QNetworkAccessManager>();
		m_network->setProxy(QNetworkProxy::applicationProxy());
	}

#if defined(Q_OS_MAC)
	// macOS bundles are updated by Sparkle from inside the launcher; never replace files in a
	// signed bundle.
	showFatalError(tr("macOS Is Not Supported"),
				   tr("The updater does not support installations on macOS. "
					  "Updates are handled by the launcher itself."));
	return;
#endif

	// An AppImage is one read-only file mounted at a temporary path; updating means replacing the
	// file, which is AppImageUpdate's job.
	if (binPath.startsWith(QLatin1String("/tmp/.mount_"))) {
		m_isAppImage = true;
		m_appImagePath =
			QProcessEnvironment::systemEnvironment().value(
				QStringLiteral("APPIMAGE"));
		if (m_appImagePath.isEmpty()) {
			showFatalError(
				tr("Unsupported Installation"),
				tr("This looks like an AppImage, but the APPIMAGE environment "
				   "variable is missing, so there is no file to update."));
			return;
		}
	}

	m_isFlatpak = DesktopServices::isFlatpak();

	m_launcherExecutable = QDir(binPath).absoluteFilePath(launcherBinaryName());
	if (!QFileInfo(m_launcherExecutable).isFile()) {
		showFatalError(tr("Unsupported Installation"),
					   tr("The updater cannot find the main executable at:\n"
						  "%1")
						   .arg(QDir::toNativeSeparators(m_launcherExecutable)));
		return;
	}

	QString repositoryOption = parser.value(QStringLiteral("update-url"));
	if (repositoryOption.isEmpty())
		repositoryOption = BuildConfig.UPDATER_GITHUB_REPO;
	m_repositoryUrl = QUrl::fromUserInput(repositoryOption);

	m_userSelectedVersionRaw = parser.value(QStringLiteral("install-version"));

	if (const QString versionOverride =
			parser.value(QStringLiteral("installed-version"));
		!versionOverride.isEmpty()) {
		m_installedVersion = GitHub::normalizeVersionTag(versionOverride);
		qDebug() << "Installed version supplied on the command line:"
				 << m_installedVersion;
	}

	m_status = Initialized;

	// A lambda: the functor overload of invokeMethod wants something callable with no arguments.
	QMetaObject::invokeMethod(
		this, [this]() { loadReleaseList(); }, Qt::QueuedConnection);
}

MeshUpdaterApp::~MeshUpdaterApp()
{
	qDebug() << "The updater is shutting down.";
	// Stop routing messages through a log file that is about to close.
	qInstallMessageHandler(nullptr);
}

QString MeshUpdaterApp::launcherBinaryName() const
{
	QString name = BuildConfig.KITE_BINARY;
#if defined(Q_OS_WIN32)
	name.append(QLatin1String(".exe"));
#endif
	return name;
}

QString MeshUpdaterApp::updaterBinaryName() const
{
#if defined(Q_OS_WIN32)
	return QStringLiteral("kite-updater.exe");
#else
	return QStringLiteral("bin/kite-updater");
#endif
}

bool MeshUpdaterApp::resolvePaths(const QString& dirOption)
{
	const QString binPath = applicationDirPath();

	// The root is the top of the directory layout, which is not the binary's directory on every
	// platform.
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
	m_rootPath = QDir(binPath + QLatin1String("/..")).absolutePath();
#elif defined(Q_OS_WIN32)
	m_rootPath = binPath;
#elif defined(Q_OS_MAC)
	m_rootPath = QDir(binPath + QLatin1String("/../..")).absolutePath();
#else
	m_rootPath = binPath;
#endif

	// portable.txt beside the installation means the data lives inside it, and decides whether a
	// portable artifact is installed.
	const QString portableMarker =
		QDir(m_rootPath).absoluteFilePath(QStringLiteral("portable.txt"));
	m_isPortable = QFileInfo::exists(portableMarker);

	if (!dirOption.isEmpty()) {
		// The launcher passes its data directory; only it knows which one is in use.
		m_dataPath = QDir(dirOption).absolutePath();
	} else if (const QString fromEnvironment =
				   QProcessEnvironment::systemEnvironment().value(
					   QStringLiteral("%1_DATA_DIR")
						   .arg(BuildConfig.KITE_NAME.toUpper()));
			   !fromEnvironment.isEmpty()) {
		m_dataPath = QDir(fromEnvironment).absolutePath();
	} else if (m_isPortable) {
		m_dataPath = m_rootPath;
	} else {
		m_dataPath = QStandardPaths::writableLocation(
			QStandardPaths::AppDataLocation);
		if (m_dataPath.isEmpty())
			m_dataPath = m_rootPath;
	}

	m_updateLogPath = UpdateLockFile::updateLogPath(m_dataPath);
	return true;
}

bool MeshUpdaterApp::startLogging()
{
	const QString logDir = QDir(m_dataPath).absoluteFilePath("logs");
	if (!QDir().mkpath(logDir)) {
		showFatalError(
			tr("The launcher data folder is not writable!"),
			tr("The updater could not create its log directory at:\n"
			   "%1\n"
			   "\n"
			   "Make sure you have write permissions to the data folder.\n"
			   "\n"
			   "The updater cannot continue until this is fixed.")
				.arg(QDir::toNativeSeparators(logDir)));
		return false;
	}

	// Check and install logs are kept apart so an install log is not buried under hourly checks.
	const QString base =
		QDir(logDir).absoluteFilePath(BuildConfig.KITE_NAME +
									  QStringLiteral("Updater") +
									  (m_checkOnly ? QStringLiteral("-CheckOnly")
												   : QString()) +
									  QStringLiteral("-%1.log"));

	// Two generations back is enough to hold both passes of one install.
	QFile::remove(base.arg(2));
	QFile::rename(base.arg(1), base.arg(2));
	QFile::rename(base.arg(0), base.arg(1));

	logFile = std::make_unique<QFile>(base.arg(0));
	if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text |
					   QIODevice::Truncate)) {
		const QString reason = logFile->errorString();
		logFile.reset();
		showFatalError(
			tr("The launcher data folder is not writable!"),
			tr("The updater could not create a log file - %1.\n"
			   "\n"
			   "Make sure you have write permissions to the data folder.\n"
			   "(%2)\n"
			   "\n"
			   "The updater cannot continue until this is fixed.")
				.arg(reason, QDir::toNativeSeparators(m_dataPath)));
		return false;
	}

	qInstallMessageHandler(updaterMessageHandler);
	qSetMessagePattern(QStringLiteral(
		"%{time process} "
		"%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}"
		"%{if-critical}C%{endif}%{if-fatal}F%{endif}"
		" | %{if-category}[%{category}]: %{endif}%{message}"));

	// The launcher's logging rules apply to the updater too.
	const QString rulesPath =
		QDir(m_dataPath).absoluteFilePath(QStringLiteral("qtlogging.ini"));
	if (QFileInfo::exists(rulesPath)) {
		QSettings rules(rulesPath, QSettings::IniFormat);
		rules.beginGroup(QStringLiteral("Rules"));
		QStringList filters;
		const QStringList keys = rules.childKeys();
		for (const QString& key : keys) {
			filters.append(QStringLiteral("%1=%2").arg(
				key, rules.value(key).toString()));
		}
		rules.endGroup();
		if (!filters.isEmpty())
			QLoggingCategory::setFilterRules(filters.join(QLatin1Char('\n')));
	}

	qDebug() << "<> Log initialized.";
	return true;
}

void MeshUpdaterApp::logStartupInfo(const QString& adjustedBy,
									const QString& binPath)
{
	qDebug().noquote() << BuildConfig.KITE_DISPLAYNAME << "Updater";
	qDebug().noquote() << "Version               :"
					   << BuildConfig.printableVersionString();
	qDebug().noquote() << "Git commit            :" << BuildConfig.GIT_COMMIT;
	qDebug().noquote() << "Git refspec           :" << BuildConfig.GIT_REFSPEC;
	qDebug().noquote() << "Compiled for          :" << BuildConfig.systemID();
	qDebug().noquote() << "Compiled by           :" << BuildConfig.compilerID();
	qDebug().noquote() << "Build artifact        :"
					   << BuildConfig.BUILD_ARTIFACT;
	qDebug().noquote() << "Binary path           :" << binPath;
	qDebug().noquote() << "Application root path :" << m_rootPath;
	qDebug().noquote() << "Data path             :" << m_dataPath;
	qDebug().noquote() << "Data path chosen by   :" << adjustedBy;
	qDebug().noquote() << "Portable install      :" << m_isPortable;
	qDebug().noquote() << "Work dir              :" << QDir::currentPath();
	qDebug() << "<> Paths set.";
}

// ---------------------------------------------------------------------------
// Release list
// ---------------------------------------------------------------------------

void MeshUpdaterApp::loadReleaseList()
{
	QString error;
	const QString apiUrl = GitHub::releasesApiUrl(m_repositoryUrl, &error);
	if (apiUrl.isEmpty()) {
		fail(tr("Cannot list releases: %1").arg(error));
		return;
	}

	qDebug() << "Fetching the release list from" << apiUrl;
	requestReleasePage(apiUrl, 1);
}

void MeshUpdaterApp::requestReleasePage(const QString& apiUrl, int page)
{
	const QString pageUrl = QStringLiteral("%1?per_page=%2&page=%3")
								.arg(apiUrl)
								.arg(kReleasesPerPage)
								.arg(page);
	m_currentUrl = pageUrl;

	// Braces: QNetworkRequest request(QUrl(pageUrl)) parses as a function declaration.
	QNetworkRequest request{QUrl(pageUrl)};
	request.setRawHeader("Accept", "application/vnd.github+json");
	request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
	request.setHeader(QNetworkRequest::UserAgentHeader,
					  BuildConfig.USER_AGENT_UNCACHED);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
						 QNetworkRequest::NoLessSafeRedirectPolicy);

	QNetworkReply* reply = m_network->get(request);
	connect(reply, &QNetworkReply::finished, this,
			[this, reply, apiUrl, page]() {
				reply->deleteLater();

				if (reply->error() != QNetworkReply::NoError) {
					fail(tr("Could not fetch %1: %2")
							 .arg(m_currentUrl, reply->errorString()));
					return;
				}

				QString error;
				const int found = GitHub::parseReleasePage(reply->readAll(),
														   &m_releases, &error);
				if (found < 0) {
					fail(tr("Could not read the release list from %1: %2")
							 .arg(m_currentUrl, error));
					return;
				}

				// A full page may not be the last. The Link header exists, but "the page was short"
				// needs no parsing and survives proxies that rewrite headers.
				if (found == kReleasesPerPage) {
					requestReleasePage(apiUrl, page + 1);
					return;
				}

				run();
			});
}

// ---------------------------------------------------------------------------
// Deciding what to do
// ---------------------------------------------------------------------------

void MeshUpdaterApp::useBuiltInVersion()
{
	m_installedVersion = BuildConfig.printableVersionString();
	m_installedGitCommit = BuildConfig.GIT_COMMIT;
}

bool MeshUpdaterApp::loadInstalledVersionFromExe(const QString& exePath)
{
	QProcess proc;
	proc.setProcessChannelMode(QProcess::MergedChannels);
	proc.start(exePath, {QStringLiteral("--version")});

	if (!proc.waitForStarted(kVersionQueryTimeoutMs)) {
		qWarning() << "Could not start the launcher to ask its version:"
				   << proc.errorString();
		return false;
	}
	if (!proc.waitForFinished(kVersionQueryTimeoutMs)) {
		proc.kill();
		proc.waitForFinished(1000);
		qWarning() << "The launcher did not report its version in time.";
		return false;
	}

	// The launcher prints labelled lines ("Version 7.19.0", "Git 0123abc"); parsed by label so
	// extra stderr warnings cannot shift the answer.
	QString version;
	QString commit;
	const QStringList lines =
		QString::fromUtf8(proc.readAll()).split(QLatin1Char('\n'));
	for (const QString& raw : lines) {
		const QString line = raw.trimmed();
		if (line.startsWith(QLatin1String("Version "), Qt::CaseInsensitive)) {
			version = line.mid(8).trimmed();
		} else if (line.startsWith(QLatin1String("Git "),
								   Qt::CaseInsensitive)) {
			commit = line.mid(4).trimmed();
		}
	}

	if (version.isEmpty()) {
		qWarning() << "The launcher did not report a version.";
		return false;
	}

	m_installedVersion = GitHub::normalizeVersionTag(version);
	m_installedGitCommit = commit;
	return true;
}

QList<GitHubRelease> MeshUpdaterApp::publishedReleases() const
{
	QList<GitHubRelease> published;
	for (const GitHubRelease& release : m_releases) {
		if (!release.isValid() || release.draft)
			continue;
		if (release.prerelease && !m_allowPreRelease)
			continue;
		published.append(release);
	}
	return published;
}

QList<GitHubRelease> MeshUpdaterApp::newerReleases() const
{
	const Version installed(m_installedVersion);
	QList<GitHubRelease> newer;
	for (const GitHubRelease& release : publishedReleases()) {
		if (installed < release.version)
			newer.append(release);
	}
	return newer;
}

GitHubRelease MeshUpdaterApp::latestRelease() const
{
	GitHubRelease latest;
	for (const GitHubRelease& release : publishedReleases()) {
		// Ordered by version, not publication date: a hotfix for an older line can be published
		// after a newer release.
		if (!latest.isValid() || latest.version < release.version)
			latest = release;
	}
	return latest;
}

bool MeshUpdaterApp::needsUpdate(const GitHubRelease& release) const
{
	if (!release.isValid())
		return false;
	return Version(m_installedVersion) < release.version;
}

GitHubRelease MeshUpdaterApp::releaseForVersion(const QString& wanted) const
{
	const Version wantedVersion(GitHub::normalizeVersionTag(wanted));
	for (const GitHubRelease& release : m_releases) {
		// Match the tag first: it is what the launcher passed, and is unambiguous even when two
		// tags normalise to one version.
		if (release.tagName == wanted || release.version == wantedVersion)
			return release;
	}
	return {};
}

GitHubRelease MeshUpdaterApp::askWhichRelease()
{
	const QList<GitHubRelease> choices =
		m_allowDowngrade ? publishedReleases() : newerReleases();

	if (choices.isEmpty())
		return {};

	SelectReleaseDialog dialog(Version(m_installedVersion), choices);
	if (dialog.exec() == QDialog::Rejected)
		return {};

	return dialog.selectedRelease();
}

void MeshUpdaterApp::printReleases() const
{
	QTextStream out(stdout);
	for (const GitHubRelease& release : m_releases) {
		out << release.displayName() << " Version: " << release.tagName
			<< '\n';
	}
	out.flush();
}

void MeshUpdaterApp::run()
{
	qDebug() << "Found" << m_releases.length() << "releases on GitHub.";

	if (m_printOnly) {
		printReleases();
		m_status = Succeeded;
		QCoreApplication::exit(ExitSuccess);
		return;
	}

	if (m_installedVersion.isEmpty() &&
		!loadInstalledVersionFromExe(m_launcherExecutable)) {
		// The updater ships with the launcher it updates, so its own build details are usually
		// right.
		qWarning() << "Falling back to the updater's own build version.";
		useBuiltInVersion();
	}

	qDebug() << "Installed version:" << m_installedVersion
			 << "commit:" << m_installedGitCommit;

	const GitHubRelease latest = latestRelease();
	if (!latest.isValid()) {
		fail(tr("The repository has no published releases%1.")
				 .arg(m_allowPreRelease ? QString()
										: tr(" (pre-releases excluded)")));
		return;
	}

	qDebug() << "Latest release:" << latest;
	const bool updateNeeded = needsUpdate(latest);

	if (m_checkOnly) {
		// The launcher parses exactly these three labelled lines and treats the rest of stdout as
		// release notes. Keep in step with KiteExternalUpdater::checkForUpdates().
		m_status = Succeeded;
		if (!updateNeeded) {
			qDebug() << "No update needed.";
			QCoreApplication::exit(ExitSuccess);
			return;
		}

		QTextStream out(stdout);
		out << "Name: " << latest.displayName() << '\n';
		out << "Version: " << latest.tagName << '\n';
		out << "TimeStamp: "
			<< (latest.publishedAt.isValid() ? latest.publishedAt
											 : latest.createdAt)
				   .toString(Qt::ISODate)
			<< '\n';
		out << latest.body << '\n';
		out.flush();

		qDebug() << "Reported update" << latest.tagName << "to the launcher.";
		QCoreApplication::exit(ExitUpdateAvailable);
		return;
	}

	if (m_isFlatpak) {
		// A Flatpak install is owned by Flatpak (read-only deployment); replacing files from
		// outside would fail or break its integrity.
		showFatalError(
			tr("Updating Flatpak Is Not Supported"),
			tr("This is the Flatpak build of %1, which is updated through "
			   "Flatpak itself.\n"
			   "\n"
			   "Run this to update:\n"
			   "flatpak update %2\n"
			   "\n"
			   "Checking for updates works, but installing them here does "
			   "not.")
				.arg(BuildConfig.KITE_DISPLAYNAME, BuildConfig.KITE_APPID));
		return;
	}

	if (m_isAppImage) {
		// One file, updated in place by AppImageUpdate over zsync, which transfers only the changed
		// blocks.
		if (!updateNeeded && !m_forceUpdate) {
			m_status = Succeeded;
			QCoreApplication::exit(ExitSuccess);
			return;
		}
		const bool started = updateAppImage();
		m_status = started ? Succeeded : Failed;
		QCoreApplication::exit(started ? ExitSuccess : ExitFailed);
		return;
	}

	if (!updateNeeded && !m_forceUpdate && m_userSelectedVersionRaw.isEmpty() &&
		!m_selectUI) {
		qDebug() << "Already up to date; nothing to do.";
		m_status = Succeeded;
		QCoreApplication::exit(ExitSuccess);
		return;
	}

	GitHubRelease target = latest;

	if (!m_userSelectedVersionRaw.isEmpty()) {
		target = releaseForVersion(m_userSelectedVersionRaw);
		if (!target.isValid()) {
			showFatalError(tr("No Release for That Version"),
						   tr("There is no GitHub release for version %1.")
							   .arg(m_userSelectedVersionRaw));
			return;
		}
	} else if (m_selectUI) {
		target = askWhichRelease();
		if (!target.isValid()) {
			// Cancelling the picker is a decision, not a failure.
			abortWith(tr("No version was selected."));
			return;
		}
	}

	performUpdate(target);
}

// ---------------------------------------------------------------------------
// Choosing an artifact
// ---------------------------------------------------------------------------

QList<GitHubReleaseAsset>
MeshUpdaterApp::matchingAssets(const GitHubRelease& release) const
{
	// The rules live in AssetMatcher, free of BuildConfig and the filesystem, so tests can cover
	// every platform's naming; this function describes the installation.
	AssetMatcher::Installation installation;
	installation.buildArtifact = BuildConfig.BUILD_ARTIFACT;
	installation.portable = m_isPortable;
	installation.appImage = m_isAppImage;
	installation.arm64 =
		QSysInfo::buildCpuArchitecture().contains(QLatin1String("arm64")) ||
		QSysInfo::buildCpuArchitecture().contains(QLatin1String("aarch64"));

	qDebug() << "Selecting an asset of" << release.tagName << "for"
			 << installation.buildArtifact
			 << "| portable:" << installation.portable
			 << "| AppImage:" << installation.appImage
			 << "| arm64:" << installation.arm64;

	QStringList decisions;
	const QList<GitHubReleaseAsset> matching =
		AssetMatcher::select(release.assets, installation, &decisions);

	// Every asset, accepted or not, with the reason: the log is the only answer to "why did it pick
	// that file".
	for (const QString& line : decisions) {
		qDebug().noquote() << " " << line;
	}

	return matching;
}

GitHubReleaseAsset
MeshUpdaterApp::askWhichAsset(const QList<GitHubReleaseAsset>& assets)
{
	SelectReleaseAssetDialog dialog(assets);
	if (dialog.exec() == QDialog::Rejected)
		return {};
	return dialog.selectedAsset();
}

// ---------------------------------------------------------------------------
// Installing
// ---------------------------------------------------------------------------

void MeshUpdaterApp::performUpdate(const GitHubRelease& release)
{
	m_installRelease = release;
	qDebug() << "Updating to" << release.tagName;

	QList<GitHubReleaseAsset> candidates = matchingAssets(release);

	if (candidates.isEmpty()) {
		showFatalError(
			tr("No Valid Release Assets"),
			tr("Release %1 has no files for this installation: %2, portable: "
			   "%3.")
				.arg(release.tagName,
					 BuildConfig.BUILD_ARTIFACT.isEmpty()
						 ? tr("unknown build")
						 : BuildConfig.BUILD_ARTIFACT,
					 m_isPortable ? tr("yes") : tr("no")));
		return;
	}

	GitHubReleaseAsset chosen;
	if (candidates.size() == 1) {
		chosen = candidates.first();
	} else {
		// More than one file fits; guessing would be guessing about something only the user can
		// see.
		qDebug() << "More than one asset matches; asking." << candidates;
		chosen = askWhichAsset(candidates);
	}

	if (!chosen.isValid()) {
		abortWith(tr("No file was selected."));
		return;
	}

	qDebug() << "Installing" << chosen;

	const QString downloaded = downloadAsset(chosen);
	if (downloaded.isEmpty()) {
		showFatalError(tr("Failed to Download"),
					   tr("The update file could not be downloaded."));
		return;
	}

	installFrom(downloaded);
}

QString MeshUpdaterApp::downloadAsset(const GitHubReleaseAsset& asset)
{
	// Downloads go to the data directory so a failed update leaves nothing in the install tree.
	const QString downloadDir =
		QDir(m_dataPath).absoluteFilePath(QStringLiteral("update-download"));
	if (!QDir().mkpath(downloadDir)) {
		qCritical() << "Could not create the download directory"
					<< downloadDir;
		return {};
	}

	const QString target = QDir(downloadDir).absoluteFilePath(asset.name);

	auto* download =
		new ReleaseDownload(m_network.get(), asset.downloadUrl, target);
	download->setExpectedSize(asset.size);

	qDebug().noquote() << "Downloading" << asset.name << "-"
					   << formatBytes(asset.size);

	ProgressDialog dialog;
	dialog.adjustSize();
	dialog.execWithTask(download);

	const bool ok = download->wasSuccessful();
	const QString failure = download->failReason();
	const bool aborted = download->wasAborted();
	download->deleteLater();

	if (!ok) {
		if (aborted)
			qDebug() << "The download was cancelled.";
		else
			qCritical() << "The download failed:" << failure;
		return {};
	}

	return target;
}

void MeshUpdaterApp::installFrom(const QString& archivePath)
{
	qDebug() << "Starting the install from" << archivePath;

	const QString lockPath = UpdateLockFile::lockPath(m_dataPath);
	if (QFileInfo::exists(lockPath)) {
		// Either another update is running or a previous one died and left the installation in an
		// unknown state; only the user can tell which.
		UpdateLockFile::Contents previous;
		UpdateLockFile::read(lockPath, &previous);

		QMessageBox box;
		box.setIcon(QMessageBox::Warning);
		box.setText(tr("An update is already in progress."));
		box.setInformativeText(
			tr("This installation has an update lock file at: %1\n"
			   "\n"
			   "Timestamp: %2\n"
			   "Updating from version %3 to %4\n"
			   "Target install path: %5\n"
			   "Data path: %6\n"
			   "\n"
			   "This usually means a previous update attempt failed. Please "
			   "make sure your installation still works before continuing.\n"
			   "The updater log at:\n"
			   "%7\n"
			   "has the details of the last attempt.\n"
			   "\n"
			   "To ignore this lock and update anyway, choose \"Ignore\".")
				.arg(QDir::toNativeSeparators(lockPath),
					 previous.timestamp.toString(Qt::ISODate), previous.from,
					 previous.to, previous.target, previous.dataPath,
					 QDir::toNativeSeparators(m_updateLogPath)));
		box.setStandardButtons(QMessageBox::Ignore | QMessageBox::Cancel);
		box.setDefaultButton(QMessageBox::Cancel);
		box.setMinimumWidth(460);
		box.adjustSize();

		if (box.exec() != QMessageBox::Ignore) {
			// A choice, not a failure, so no error dialog. The lock stays: it describes an earlier
			// attempt, and the launcher reports it at startup.
			abortWith(tr("The update was aborted: an update lock from an "
						 "earlier attempt is present at %1.")
						  .arg(lockPath));
			return;
		}
	}

	clearUpdateLog();

	// Kept next to the markers so the launcher can show what it updated to after the release list
	// is gone.
	writeTextFile(UpdateLockFile::markerPath(
					  m_dataPath, QLatin1String(UpdateLockFile::kChangelogName)),
				  m_installRelease.body);

	logUpdate(tr("Updating from %1 to %2")
				  .arg(m_installedVersion, m_installRelease.tagName));

	const QString lowerName = QFileInfo(archivePath).fileName().toLower();
	if (isArchiveName(lowerName)) {
		UpdateLockFile::Contents lock;
		lock.timestamp = QDateTime::currentDateTime();
		lock.from = m_installedVersion;
		lock.to = m_installRelease.tagName;
		lock.target = m_rootPath;
		lock.dataPath = m_dataPath;
		lock.stage = 1;
		lock.pid = QCoreApplication::applicationPid();
		UpdateLockFile::write(lockPath, lock);

		logUpdate(tr("Updating the installation at %1").arg(m_rootPath));
		unpackAndHandOff(archivePath);

		// The one place the lock is released. It means "the installation may be a mix of two
		// versions", true only once the second pass took over; every other exit left the
		// installation as it was. Exception: a second pass that was started, never confirmed and
		// still runs. What it did is unknown, so the lock stays.
		if (!m_handedOver && !m_installStateUnknown)
			releaseLockNothingChanged();

		// Only now, with the lock dealt with, is the user told (see deferFailure()).
		if (!m_deferredFailureTitle.isEmpty())
			showFatalError(m_deferredFailureTitle, m_deferredFailureText);
		return;
	}

	logUpdate(tr("Running the installer at %1").arg(archivePath));
	runInstaller(archivePath);
}

void MeshUpdaterApp::runInstaller(const QString& installerPath)
{
	QProcess proc;
	applyInvokerCompatibility(&proc);
	proc.setProgram(installerPath);

	const bool started = proc.startDetached();
	logUpdate(started ? tr("The installer was started.")
					  : tr("The installer could not be started: %1")
							.arg(proc.errorString()));

	// The installer owns the installation from here, including restarting the launcher.
	m_status = started ? Succeeded : Failed;
	QCoreApplication::exit(started ? ExitSuccess : ExitFailed);
}

void MeshUpdaterApp::unpackAndHandOff(const QString& archivePath)
{
	// Unpack and check the release before touching the installation: backing up first would waste
	// minutes and leave a backup directory behind for an update that never happened.
	const QString stagingDir = QDir(m_dataPath).absoluteFilePath(
		QLatin1String(UpdateLockFile::kStagingDirName));

	// A previous attempt's leftovers would mix two releases.
	removeDirectoryTree(stagingDir);

	const ReleaseArchive::Result unpacked =
		ReleaseArchive::extract(archivePath, stagingDir);
	if (!unpacked.ok) {
		logUpdate(tr("Failed to unpack %1: %2")
					  .arg(archivePath, unpacked.error));
		deferFailure(tr("Failed to Extract Archive"),
					   tr("Could not unpack %1 into %2:\n%3")
						   .arg(QDir::toNativeSeparators(archivePath),
								QDir::toNativeSeparators(stagingDir),
								unpacked.error));
		return;
	}

	logUpdate(tr("Unpacked %1 files and %2 links (%3) into %4")
				  .arg(QString::number(unpacked.fileCount),
					   QString::number(unpacked.linkCount),
					   formatBytes(unpacked.byteCount), stagingDir));

	// Archives have their contents at the top level or wrapped in one versioned directory.
	const QString releaseRoot =
		ReleaseArchive::descendIntoSingleRoot(stagingDir);

	// A release without a launcher is not a release. In a bundle the file is a hard link to the
	// sharun loader; if it is missing, an extractor dropped the links and the libraries too.
	const QString stagedLauncher = QDir(releaseRoot).absoluteFilePath(
#if defined(Q_OS_WIN32)
		launcherBinaryName()
#else
		QStringLiteral("bin/") + launcherBinaryName()
#endif
	);
	if (!QFileInfo(stagedLauncher).isFile()) {
		logUpdate(tr("The unpacked release has no launcher at %1")
					  .arg(stagedLauncher));
		deferFailure(
			tr("Update Failed"),
			tr("The downloaded release does not contain %1 at:\n"
			   "%2\n"
			   "\n"
			   "Your installation has not been changed.\n"
			   "The updater log has the details:\n"
			   "%3")
				.arg(BuildConfig.KITE_DISPLAYNAME,
					 QDir::toNativeSeparators(stagedLauncher),
					 QDir::toNativeSeparators(m_updateLogPath)));
		return;
	}

	// The binary that runs the second pass, proven to run before anything is committed. This
	// process cannot install: the files it would overwrite include the libraries it runs from.
	const QString stageProgram = prepareInstallStageProgram(releaseRoot);
	if (stageProgram.isEmpty()) {
		logUpdate(tr("No usable second pass was found for this release."));
		deferFailure(
			tr("Update Failed"),
			tr("The update cannot be installed, because no program that is "
			   "able to install it could be started.\n"
			   "\n"
			   "Your installation has not been changed. You can install this "
			   "release by downloading it yourself.\n"
			   "\n"
			   "The updater log has what was tried and why each attempt "
			   "failed:\n"
			   "%1")
				.arg(QDir::toNativeSeparators(m_updateLogPath)));
		return;
	}

	// The release is installable; from here the installation gets touched.
	logUpdate(tr("Backing up the current installation"));
	backupInstallation();

	// The marker tells the second pass that it is one, and where to install.
	const QString marker = QDir(releaseRoot).absoluteFilePath(
		QLatin1String(UpdateLockFile::kUnpackMarkerName));
	if (!writeTextFile(marker, m_rootPath)) {
		logUpdate(tr("Could not write the update marker at %1").arg(marker));
		deferFailure(tr("Update Failed"),
					   tr("Could not write the update marker at:\n%1")
						   .arg(QDir::toNativeSeparators(marker)));
		return;
	}

	QStringList arguments{QStringLiteral("--dir"), m_dataPath,
						  QStringLiteral("--finish-update"), releaseRoot};
	if (logToConsole) {
		// Keep logging to the terminal the hand-off was started from.
		arguments.append(QStringLiteral("--debug"));
	}

	QProcess next;
	applyInvokerCompatibility(&next);
	next.setProgram(stageProgram);
	next.setArguments(arguments);
	next.setWorkingDirectory(releaseRoot);

	logUpdate(tr("Starting the second pass: %1 %2")
				  .arg(stageProgram, arguments.join(QLatin1Char(' '))));

	qint64 stagePid = 0;
	if (!next.startDetached(&stagePid)) {
		logUpdate(tr("Could not start %1: %2")
					  .arg(stageProgram, next.errorString()));
		deferFailure(tr("Update Failed"),
					   tr("Could not start the second pass of the update:\n"
						  "%1\n"
						  "\n"
						  "%2\n"
						  "\n"
						  "Your installation has not been changed.")
						   .arg(QDir::toNativeSeparators(stageProgram),
								next.errorString()));
		return;
	}

	// Started is not running the update: startDetached() only reports that the process was created,
	// also for an older updater that exits at once on this command line. Calling that handed over
	// reports success with nothing installed.
	if (!waitForInstallStageTakeover(stagePid)) {
		const bool stillRunning = processAlive(stagePid);
		m_installStateUnknown = stillRunning;

		logUpdate(tr("The second pass (pid %1) did not take the update over "
					 "within %2 seconds and is %3.")
					  .arg(QString::number(stagePid),
						   QString::number(kTakeoverTimeoutMs / 1000),
						   stillRunning ? tr("still running")
										: tr("no longer running")));

		deferFailure(
			tr("Update Failed"),
			stillRunning
				? tr("The second pass of the update was started but never "
					 "reported that it had taken over, and it is still "
					 "running.\n"
					 "\n"
					 "Check that %1 still starts before using it. The "
					 "previous installation was backed up, and the updater "
					 "log has the details:\n"
					 "%2")
					  .arg(BuildConfig.KITE_DISPLAYNAME,
						   QDir::toNativeSeparators(m_updateLogPath))
				: tr("The second pass of the update stopped before it began "
					 "installing.\n"
					 "\n"
					 "Your installation has not been changed. The updater log "
					 "has the details:\n"
					 "%1")
					  .arg(QDir::toNativeSeparators(m_updateLogPath)));
		return;
	}

	// Exiting releases every file this process holds in the installation the second pass
	// overwrites.
	m_handedOver = true;
	logUpdate(tr("Handed the update over to the second pass (pid %1).")
				  .arg(stagePid));
	m_status = Succeeded;
	QCoreApplication::exit(ExitSuccess);
}

QString MeshUpdaterApp::realUpdaterBinaryIn(const QString& root) const
{
	const QString name = QFileInfo(updaterBinaryName()).fileName();

	// In a sharun bundle bin/<name> is a hard link to the loader, which holds none of the updater's
	// code: it dispatches by invoked name into shared/bin.
	const QString shared = QDir(root).absoluteFilePath(
		QStringLiteral("shared/bin/%1").arg(name));
	if (QFileInfo(shared).isFile())
		return shared;

	return QDir(root).absoluteFilePath(updaterBinaryName());
}

QString MeshUpdaterApp::ownUpdaterBinaryPath() const
{
	// Copying the loader would copy none of our code and, staged, would dispatch into the very
	// updater we try not to depend on.
	const QString real = realUpdaterBinaryIn(m_rootPath);
	if (QFileInfo(real).isFile())
		return real;

	return applicationFilePath();
}

bool MeshUpdaterApp::copyExecutable(const QString& from, const QString& to)
{
	QString error;
	if (!copyOverFile(from, to, &error)) {
		logUpdate(tr("Could not copy %1 to %2: %3").arg(from, to, error));
		return false;
	}

	// QFile::copy does not carry the executable bit over on Unix.
	if (!QFile::setPermissions(to, QFile::permissions(to) |
										QFileDevice::ExeOwner |
										QFileDevice::ExeUser |
										QFileDevice::ExeGroup |
										QFileDevice::ExeOther)) {
		logUpdate(tr("Could not make %1 executable.").arg(to));
		return false;
	}

	return true;
}

bool MeshUpdaterApp::probeUpdaterBinary(const QString& program)
{
	if (!QFileInfo(program).isFile()) {
		logUpdate(tr("There is no file at %1.").arg(program));
		return false;
	}

	QProcess probe;
	applyInvokerCompatibility(&probe);
	probe.setProgram(program);
	probe.setArguments({QString::fromLatin1(kSelfTestOption)});

	probe.start();
	if (!probe.waitForStarted(kSelfTestTimeoutMs)) {
		logUpdate(tr("%1 could not be started: %2")
					  .arg(program, probe.errorString()));
		return false;
	}
	if (!probe.waitForFinished(kSelfTestTimeoutMs)) {
		// An older updater given an unknown option may put up a dialog nobody dismisses.
		probe.kill();
		probe.waitForFinished(1000);
		logUpdate(tr("%1 did not answer the self test within %2 seconds.")
					  .arg(program, QString::number(kSelfTestTimeoutMs / 1000)));
		return false;
	}

	const QString answer =
		QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
	const QString complaint =
		QString::fromUtf8(probe.readAllStandardError()).trimmed();

	if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0 ||
		!answer.contains(QLatin1String(kSelfTestToken))) {
		logUpdate(tr("%1 failed the self test (exit code %2): %3")
					  .arg(program, QString::number(probe.exitCode()),
						   complaint.isEmpty() ? answer : complaint));
		return false;
	}

	logUpdate(tr("%1 passed the self test: %2").arg(program, answer));
	return true;
}

QString MeshUpdaterApp::prepareInstallStageProgram(const QString& releaseRoot)
{
	const QString agentName = installStageAgentName();
	const QString ownBinary = ownUpdaterBinaryPath();

	logUpdate(tr("Looking for a program to install the update with."));

	// 1. The updater that came with the release. When it understands this protocol its libraries
	// are the ones it was built against and it leaves nothing behind. Older releases take a
	// different command line and exit without doing anything, which the self test catches.
	const QString releaseUpdater =
		QDir(releaseRoot).absoluteFilePath(updaterBinaryName());
	logUpdate(tr("Trying the release's own updater at %1").arg(releaseUpdater));

	// Look at the code before running it: in a bundle the file above is a hard link to the loader,
	// which holds none of the updater's strings.
	const QString releaseUpdaterCode = realUpdaterBinaryIn(releaseRoot);
	if (!fileContainsBytes(releaseUpdaterCode,
						   QByteArrayLiteral("finish-update"))) {
		logUpdate(tr("%1 does not know --finish-update, so it is from before "
					 "the current updater; not running it.")
					  .arg(releaseUpdaterCode));
	} else if (probeUpdaterBinary(releaseUpdater)) {
		return releaseUpdater;
	}

	// 2. This updater copied into the unpacked release. It runs against the release's libraries:
	// fine for the usual upgrade direction, may fail for a release old enough to predate them. The
	// self test decides.
	const QString sharedBin =
		QDir(releaseRoot).absoluteFilePath(QStringLiteral("shared/bin"));
	if (QFileInfo(sharedBin).isDir()) {
		// A bundle runs everything through its loader, which finds the executable by invoked name,
		// so both halves go in place under the agent's name.
		QString loader = QDir(releaseRoot).absoluteFilePath(
			QStringLiteral("sharun"));
		if (!QFileInfo(loader).isFile())
			loader = QDir(releaseRoot).absoluteFilePath(updaterBinaryName());

		const QString agentLoader = QDir(releaseRoot).absoluteFilePath(
			QStringLiteral("bin/%1").arg(agentName));
		const QString agentBinary =
			QDir(sharedBin).absoluteFilePath(agentName);

		logUpdate(tr("Trying this updater inside the release, as %1")
					  .arg(agentLoader));
		if (copyExecutable(ownBinary, agentBinary) &&
			copyExecutable(loader, agentLoader) &&
			probeUpdaterBinary(agentLoader)) {
			return agentLoader;
		}
	} else {
		const QString agent =
			QDir(QFileInfo(releaseUpdater).absolutePath())
				.absoluteFilePath(agentName);
		logUpdate(tr("Trying this updater inside the release, as %1")
					  .arg(agent));
		if (copyExecutable(ownBinary, agent) && probeUpdaterBinary(agent))
			return agent;
	}

	// 3. This updater on its own in the data directory, for installations that carry no libraries
	// (e.g. a local build against system Qt); it depends on nothing the update replaces.
	const QString standaloneDir = QDir(m_dataPath).absoluteFilePath(
		QLatin1String(kInstallStageDirName));
	removeDirectoryTree(standaloneDir);

	const QString standalone =
		QDir(standaloneDir).absoluteFilePath(agentName);
	logUpdate(tr("Trying this updater on its own, as %1").arg(standalone));
	if (copyExecutable(ownBinary, standalone) &&
		probeUpdaterBinary(standalone)) {
		return standalone;
	}

	return {};
}

bool MeshUpdaterApp::waitForInstallStageTakeover(qint64 stagePid)
{
	const QString lockPath = UpdateLockFile::lockPath(m_dataPath);
	const qint64 ownPid = QCoreApplication::applicationPid();

	logUpdate(tr("Waiting for the second pass (pid %1) to take the update "
				 "over.")
				  .arg(stagePid));

	QElapsedTimer timer;
	timer.start();

	while (timer.elapsed() < kTakeoverTimeoutMs) {
		UpdateLockFile::Contents lock;
		if (UpdateLockFile::read(lockPath, &lock) &&
			lock.stage == UpdateLockFile::kInstallStage && lock.pid != 0 &&
			lock.pid != ownPid) {
			return true;
		}

		// A gone process never claims anything: stop waiting, so the caller can say nothing was
		// installed rather than nothing is known.
		if (stagePid > 0 && !processAlive(stagePid))
			return false;

		// The backup's progress dialog is still up.
		QCoreApplication::processEvents();
		QThread::msleep(kHandshakePollMs);
	}

	return false;
}

void MeshUpdaterApp::backupInstallation()
{
	const QDir root(m_rootPath);
	const QStringList fileList = installedFileList(root);

	static const QRegularExpression unsafeForFilenames(
		QStringLiteral("[\\\\/:*?\"<>|]"));
	QString stamp =
		QString(m_installedVersion).replace(unsafeForFilenames,
											QStringLiteral("_"));
	if (stamp.isEmpty())
		stamp = QStringLiteral("unknown");

	// The commit is missing when the version came from the command line; avoid a name like
	// "backup_9.0.0-".
	if (!m_installedGitCommit.isEmpty())
		stamp += QLatin1Char('-') + m_installedGitCommit.left(10);

	const QString backupDir =
		root.absoluteFilePath(QStringLiteral("backup_%1").arg(stamp));

	if (!QDir().mkpath(backupDir)) {
		logUpdate(tr("Could not create the backup directory %1")
					  .arg(backupDir));
		return;
	}

	// Written where the launcher can find it, for a manual roll-back.
	writeTextFile(
		UpdateLockFile::markerPath(
			m_dataPath, QLatin1String(UpdateLockFile::kBackupMarkerName)),
		backupDir);

	QProgressDialog progress(
		tr("Backing up the installation at %1").arg(m_rootPath), QString(), 0,
		fileList.length());
	progress.setCancelButton(nullptr);
	progress.setMinimumWidth(400);
	progress.adjustSize();
	progress.show();
	QCoreApplication::processEvents();

	logUpdate(tr("Backing up:\n  %1").arg(fileList.join(QLatin1String(",\n  "))));

	const auto backUp = [&](const QString& path) {
		const QString relative = root.relativeFilePath(path);
		const QString destination =
			QDir(backupDir).absoluteFilePath(relative);

		const QFileInfo info(path);
		if (info.isDir()) {
			// Walk directories rather than copying wholesale, so the user's own files inside are
			// not duplicated.
			QDirIterator iterator(path, QDir::Files | QDir::NoDotAndDotDot,
								  QDirIterator::Subdirectories);
			while (iterator.hasNext()) {
				const QString file = iterator.next();
				QString error;
				if (!copyOverFile(
						file,
						QDir(backupDir)
							.absoluteFilePath(root.relativeFilePath(file)),
						&error)) {
					logUpdate(tr("Could not back up %1: %2").arg(file, error));
				}
			}
			return;
		}

		QString error;
		if (!copyOverFile(path, destination, &error))
			logUpdate(tr("Could not back up %1: %2").arg(path, error));
	};

	int done = 0;
	for (const QString& glob : fileList) {
		progress.setValue(done++);
		QCoreApplication::processEvents();

		if (glob.isEmpty())
			continue;

		// Manifest entries may be globs ("Qt*.dll"), or plain names.
		QDirIterator iterator(m_rootPath, {glob},
							  QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
		if (!iterator.hasNext()) {
			const QFileInfo direct(root.absoluteFilePath(glob));
			if (direct.exists())
				backUp(direct.absoluteFilePath());
			else
				logUpdate(tr("Not present, ignoring: %1").arg(glob));
			continue;
		}
		while (iterator.hasNext())
			backUp(iterator.next());
	}

	progress.setValue(done);
	QCoreApplication::processEvents();

	logUpdate(tr("Backed the installation up to %1").arg(backupDir));

	// Last moment m_rootPath is the installation; the second pass runs from the staging directory,
	// where "backup_*" matches nothing.
	pruneBackups();
}

qint64 MeshUpdaterApp::claimLockForInstallStage(const QString& targetDir)
{
	const QString lockPath = UpdateLockFile::lockPath(m_dataPath);

	// Keep what the first pass wrote so a lock left by a failed install still says which versions
	// it was between.
	UpdateLockFile::Contents lock;
	UpdateLockFile::read(lockPath, &lock);
	const qint64 firstPass = lock.stage == 1 ? lock.pid : 0;

	lock.timestamp = QDateTime::currentDateTime();
	lock.stage = UpdateLockFile::kInstallStage;
	lock.pid = QCoreApplication::applicationPid();
	if (lock.target.isEmpty())
		lock.target = targetDir;
	if (lock.dataPath.isEmpty())
		lock.dataPath = m_dataPath;

	if (UpdateLockFile::write(lockPath, lock)) {
		logUpdate(tr("Took the update lock over (pid %1).").arg(lock.pid));
	} else {
		// Continue: the install does not depend on the lock. The first pass reports the unconfirmed
		// hand-off.
		logUpdate(tr("Could not take the update lock at %1 over.")
					  .arg(lockPath));
	}

	return firstPass;
}

void MeshUpdaterApp::removeInstallStageAgents(const QString& root)
{
	const QString agentName = installStageAgentName();
	const QStringList candidates = {
		QDir(root).absoluteFilePath(agentName),
		QDir(root).absoluteFilePath(QStringLiteral("bin/%1").arg(agentName)),
		QDir(root).absoluteFilePath(
			QStringLiteral("shared/bin/%1").arg(agentName)),
	};

	for (const QString& path : candidates) {
		if (!QFileInfo::exists(path))
			continue;
		// Deleting the running file is allowed on the platforms where a first pass resorts to this;
		// the copy is already mapped.
		if (QFile::remove(path))
			logUpdate(tr("Removed the second pass's copy at %1").arg(path));
		else
			logUpdate(tr("Could not remove %1; it may end up installed.")
						  .arg(path));
	}
}

void MeshUpdaterApp::finishUpdate(const QString& sourceDir,
								  const QString& targetDir)
{
	logUpdate(tr("Completing the update: installing %1 into %2, from %3")
				  .arg(QDir::toNativeSeparators(sourceDir),
					   QDir::toNativeSeparators(targetDir),
					   QDir::toNativeSeparators(applicationFilePath())));

	// Before copying or waiting: the first pass's only proof that its hand-off worked.
	const qint64 firstPass = claimLockForInstallStage(targetDir);

	// The first pass must be gone before its files are replaced; a mapped executable cannot be
	// deleted on Windows, which is why installing takes two passes.
	if (firstPass > 0 && firstPass != QCoreApplication::applicationPid()) {
		QElapsedTimer timer;
		timer.start();
		while (processAlive(firstPass) &&
			   timer.elapsed() < kFirstPassExitTimeoutMs) {
			QCoreApplication::processEvents();
			QThread::msleep(kHandshakePollMs);
		}

		if (processAlive(firstPass)) {
			logUpdate(tr("The first pass (pid %1) is still running after %2 "
						 "seconds; installing anyway.")
						  .arg(QString::number(firstPass),
							   QString::number(kFirstPassExitTimeoutMs /
											   1000)));
		} else {
			logUpdate(tr("The first pass (pid %1) has exited.")
						  .arg(firstPass));
		}
	}

	// Give the OS a moment to release the files even after the process is gone.
	QThread::sleep(1);

	// A first pass's updater copies in the staged release are not part of it and must not be
	// installed.
	removeInstallStageAgents(sourceDir);

	// The whole unpacked tree is the source, passed in because this binary may be a copy beside the
	// tree.
	const QDir source(sourceDir);
	const QDir target(targetDir);
	const QStringList fileList = installedFileList(source);

	logUpdate(tr("Installing the following into %1:\n  %2")
				  .arg(target.absolutePath(),
					   fileList.join(QLatin1String(",\n  "))));

	QProgressDialog progress(
		tr("Installing from %1").arg(source.absolutePath()), QString(), 0,
		fileList.length());
	progress.setCancelButton(nullptr);
	progress.setMinimumWidth(400);
	progress.adjustSize();
	progress.show();
	QCoreApplication::processEvents();

	bool failed = false;

	const auto install = [&](const QString& path) {
		const QString relative = source.relativeFilePath(path);
		const QFileInfo info(path);

		if (info.isDir()) {
			QDirIterator iterator(path, QDir::Files | QDir::NoDotAndDotDot,
								  QDirIterator::Subdirectories);
			while (iterator.hasNext()) {
				const QString file = iterator.next();
				const QString destination = target.absoluteFilePath(
					source.relativeFilePath(file));
				QString error;
				if (!copyOverFile(file, destination, &error)) {
					logUpdate(tr("Failed to install %1: %2").arg(file, error));
					failed = true;
				}
			}
			return;
		}

		const QString destination = target.absoluteFilePath(relative);
		QString error;
		if (!copyOverFile(path, destination, &error)) {
			logUpdate(tr("Failed to install %1: %2").arg(path, error));
			failed = true;
		}
	};

	int done = 0;
	for (const QString& glob : fileList) {
		progress.setValue(done++);
		QCoreApplication::processEvents();

		if (glob.isEmpty())
			continue;

		QDirIterator iterator(source.absolutePath(), {glob},
							  QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
		if (!iterator.hasNext()) {
			const QFileInfo direct(source.absoluteFilePath(glob));
			if (direct.exists())
				install(direct.absoluteFilePath());
			else
				logUpdate(tr("Not present, ignoring: %1").arg(glob));
			continue;
		}
		while (iterator.hasNext())
			install(iterator.next());
	}

	progress.setValue(done);
	QCoreApplication::processEvents();

	// The launcher reports both outcomes on its next start with this log attached; otherwise a
	// failed update would be silent.
	const QString marker = UpdateLockFile::markerPath(
		m_dataPath,
		QLatin1String(failed ? UpdateLockFile::kFailMarkerName
							 : UpdateLockFile::kSuccessMarkerName));
	logUpdate(failed ? tr("There were errors installing the update.")
					 : tr("The update succeeded."));
	QString copyError;
	copyOverFile(m_updateLogPath, marker, &copyError);

	QFile::remove(UpdateLockFile::lockPath(m_dataPath));

	QProcess launcher;
	applyInvokerCompatibility(&launcher);
	launcher.setProgram(target.absoluteFilePath(
#if defined(Q_OS_WIN32)
		launcherBinaryName()
#else
		QStringLiteral("bin/") + launcherBinaryName()
#endif
			));

	if (!launcher.startDetached()) {
		logUpdate(tr("Could not restart the launcher: %1")
					  .arg(launcher.errorString()));
		failed = true;
	}

	m_status = failed ? Failed : Succeeded;
	QCoreApplication::exit(failed ? ExitFailed : ExitSuccess);
}

bool MeshUpdaterApp::updateAppImage()
{
	// AppImageUpdate reads the update information embedded in the AppImage and fetches only changed
	// blocks over zsync (we publish a .zsync next to every .AppImage).
	QStringList candidates = {
		QDir(m_rootPath).absoluteFilePath(
			QStringLiteral("bin/AppImageUpdate.AppImage")),
		QDir(m_rootPath).absoluteFilePath(QStringLiteral("bin/AppImageUpdate")),
	};

	QString program;
	for (const QString& candidate : candidates) {
		if (QFileInfo(candidate).isExecutable()) {
			program = candidate;
			break;
		}
	}
	if (program.isEmpty()) {
		// Not bundled, but commonly installed system-wide.
		program = QStandardPaths::findExecutable(
			QStringLiteral("AppImageUpdate"));
	}

	if (program.isEmpty()) {
		showFatalError(
			tr("AppImageUpdate Not Found"),
			tr("This AppImage is updated with AppImageUpdate, which could not "
			   "be found.\n"
			   "\n"
			   "Install AppImageUpdate, or download the new version "
			   "manually."));
		return false;
	}

	QProcess proc;
	proc.setProgram(program);
	proc.setArguments({m_appImagePath});

	qDebug() << "Calling" << program << "for" << m_appImagePath;

	if (!proc.startDetached()) {
		qCritical() << "Could not start AppImageUpdate:" << proc.errorString();
		showFatalError(tr("Update Failed"),
					   tr("Could not start AppImageUpdate:\n%1")
						   .arg(proc.errorString()));
		return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QStringList MeshUpdaterApp::installedFileList(const QDir& root) const
{
	const QString manifestPath =
		root.absoluteFilePath(QStringLiteral("manifest.txt"));

	if (QFileInfo(manifestPath).isFile()) {
		// The packaging writes manifest.txt, so it knows which files belong to Kite and which to
		// the user.
		QStringList entries;
		const QStringList lines =
			readTextFile(manifestPath).split(QLatin1Char('\n'));
		for (const QString& line : lines) {
			const QString trimmed = line.trimmed();
			if (!trimmed.isEmpty())
				entries.append(trimmed);
		}
		if (!entries.isEmpty()) {
			qDebug() << "Read" << entries.size() << "entries from"
					 << manifestPath;
			return entries;
		}
	}

	qWarning() << manifestPath
			   << "is missing or empty; falling back to a per-platform guess.";

#if defined(Q_OS_WIN32)
	return {
		QStringLiteral("jars"),
		launcherBinaryName(),
		QStringLiteral("kite-updater.exe"),
		QStringLiteral("kite-crashreporter.exe"),
		QStringLiteral("qtlogging.ini"),
		QStringLiteral("qt.conf"),
		QStringLiteral("imageformats"),
		QStringLiteral("iconengines"),
		QStringLiteral("networkinformation"),
		QStringLiteral("platforms"),
		QStringLiteral("styles"),
		QStringLiteral("tls"),
		// Qt, the MSVC runtime and the OpenSSL copies all live at the root.
		QStringLiteral("*.dll"),
	};
#else
	return {
		QStringLiteral("bin"),
		QStringLiteral("lib"),
		// Fedora and openSUSE install libraries into lib64.
		QStringLiteral("lib64"),
		QStringLiteral("share"),
		QStringLiteral("plugins"),
		QStringLiteral("shared"),
		QStringLiteral("sharun"),
		BuildConfig.KITE_DISPLAYNAME,
	};
#endif
}

void MeshUpdaterApp::clearUpdateLog()
{
	QFile::remove(m_updateLogPath);
}

void MeshUpdaterApp::releaseLockNothingChanged()
{
	const QString lockPath = UpdateLockFile::lockPath(m_dataPath);
	if (QFile::remove(lockPath)) {
		logUpdate(tr("The installation was not modified; dropped the update "
					 "lock."));
	}
}

void MeshUpdaterApp::pruneBackups()
{
	// Two is enough to roll back the last update and the one before.
	constexpr int kBackupsToKeep = 2;

	QDir root(m_rootPath);
	QFileInfoList backups =
		root.entryInfoList({QStringLiteral("backup_*")},
						   QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

	// QDir::Time sorts newest first, so everything past the first few goes.
	for (int i = kBackupsToKeep; i < backups.size(); ++i) {
		const QString path = backups.at(i).absoluteFilePath();
		logUpdate(tr("Removing the old backup at %1").arg(path));
		if (!QDir(path).removeRecursively())
			logUpdate(tr("Could not fully remove %1").arg(path));
	}
}

void MeshUpdaterApp::logUpdate(const QString& message)
{
	qDebug().noquote() << message;
	appendTextFile(m_updateLogPath, message + QLatin1Char('\n'));
}

void MeshUpdaterApp::fail(const QString& reason)
{
	qCritical().noquote() << reason;
	m_status = Failed;

	if (m_checkOnly) {
		// The launcher maps exit status 1 to "Update Check Error" and shows stderr behind "Show
		// Details"; no dialog for an unattended check.
		QTextStream errorStream(stderr);
		errorStream << reason << '\n';
		errorStream.flush();
		QCoreApplication::exit(ExitFailed);
		return;
	}

	showFatalError(tr("Update Failed"), reason);
}

void MeshUpdaterApp::abortWith(const QString& reason)
{
	qWarning().noquote() << reason;
	m_status = Aborted;
	QCoreApplication::exit(ExitAborted);
}

void MeshUpdaterApp::deferFailure(const QString& title, const QString& text)
{
	qCritical().noquote() << title << "-" << text;
	m_status = Failed;
	m_deferredFailureTitle = title;
	m_deferredFailureText = text;
}

void MeshUpdaterApp::showFatalError(const QString& title, const QString& text)
{
	m_status = Failed;

	if (m_checkOnly) {
		// A check runs unattended: report and exit.
		QTextStream errorStream(stderr);
		errorStream << title << ": " << text << '\n';
		errorStream.flush();
		QCoreApplication::exit(ExitFailed);
		return;
	}

	QMessageBox box;
	box.setWindowTitle(title);
	box.setText(text);
	box.setIcon(QMessageBox::Critical);
	box.setStandardButtons(QMessageBox::Ok);
	box.setDefaultButton(QMessageBox::Ok);
	// Users paste these paths and versions into bug reports.
	box.setTextInteractionFlags(Qt::TextSelectableByMouse |
								Qt::TextBrowserInteraction);
	box.setMinimumWidth(460);
	box.adjustSize();
	box.exec();

	QCoreApplication::exit(ExitFailed);
}
