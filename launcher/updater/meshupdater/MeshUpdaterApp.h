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
#include <QDir>
#include <QFile>
#include <QList>
#include <QString>
#include <QUrl>

#include <memory>

#include "GitHubRelease.h"
#include "Version.h"

class QNetworkAccessManager;

/*!
 * The standalone updater.
 *
 * Runs in one of three modes, all driven from the same release list so that
 * they can never disagree with each other:
 *
 *   --check-only        report whether an update exists, and exit. Used by
 *                       the launcher; the answer is the exit status (see
 *                       ExitCode) and the details go to stdout.
 *   --list              print the available releases and exit.
 *   (default)           install an update, then restart the launcher.
 *
 * Installing is itself two passes through this binary. The first runs from
 * the installation being replaced: it downloads a release, backs the current
 * files up, unpacks the new ones into a staging directory, and then starts
 * the *unpacked* updater and exits -- which is what releases the libraries
 * this process had mapped. The second pass finds the marker file left in the
 * staging directory, copies everything into the installation (nothing there
 * is mapped by it), and launches the launcher again.
 *
 * A single-process updater cannot do this on Windows: the files it has to
 * replace include the Qt libraries it is running from, and a mapped image
 * cannot be deleted. It would die partway through and leave an installation
 * that is neither version.
 */
class MeshUpdaterApp : public QApplication
{
	Q_OBJECT

  public:
	//! How far this run got. main() turns it into a process exit code.
	enum Status {
		Starting,	 //!< Still setting up; the event loop should run.
		Initialized, //!< Set up, work queued; the event loop should run.
		Succeeded,	 //!< Finished; nothing left to do.
		Failed,		 //!< Something went wrong.
		Aborted,	 //!< The user backed out.
	};

	/*!
	 * Process exit codes.
	 *
	 * The launcher reads these, so they are part of an interface and not an
	 * implementation detail. 100 rather than 2 for "update available", so it
	 * cannot be confused with a crash or with Qt's own failure paths.
	 */
	enum ExitCode {
		ExitSuccess = 0,
		ExitFailed = 1,
		ExitAborted = 2,
		ExitUpdateAvailable = 100,
	};

	MeshUpdaterApp(int& argc, char** argv);
	~MeshUpdaterApp() override;

	/*!
	 * Answer --self-test, if that is what was asked, and say so.
	 *
	 * The first pass has to know whether a candidate for the second pass is
	 * a binary that understands this protocol at all -- the updater that
	 * ships in an older release does not, and starting it silently does
	 * nothing. So candidates are run once with --self-test first, and are
	 * only used if they answer with the token below.
	 *
	 * Handled from main(), before a QApplication exists: a probe must work
	 * without a display, and constructing QApplication without one aborts.
	 */
	static bool handleSelfTest(int argc, char** argv);

	//! The option a probe passes, and the answer it expects on stdout.
	static constexpr auto kSelfTestOption = "--self-test";
	static constexpr auto kSelfTestToken = "KITE_UPDATER_SELF_TEST_OK";

	Status status() const
	{
		return m_status;
	}

	//! Open log file, for the installed message handler. Public by necessity.
	std::unique_ptr<QFile> logFile;

	//! Whether --debug was given: mirror the log to whatever console we have.
	bool logToConsole = false;

  private slots:
	//! Fetch the release list, one page at a time, then run().
	void loadReleaseList();

  private:
	// -- setup ------------------------------------------------------------

	bool resolvePaths(const QString& dirOption);
	bool startLogging();
	void logStartupInfo(const QString& adjustedBy, const QString& binPath);

	// -- release list -----------------------------------------------------

	void requestReleasePage(const QString& apiUrl, int page);

	// -- the actual decision ----------------------------------------------

	/*!
	 * Everything after the release list is known.
	 *
	 * Named for what it is: the point where the updater stops gathering and
	 * starts acting. Every path out of it exits the process.
	 */
	void run();

	/*!
	 * Ask the installed launcher what version it is, by running it with
	 * --version.
	 *
	 * The updater is built from the same source as the launcher, so its own
	 * BuildConfig is usually right -- but not after the updater has already
	 * been replaced once, or when it is run by hand against a different
	 * installation. Asking the binary is the only answer that is true in all
	 * three cases. Falls back to BuildConfig when the launcher cannot be
	 * asked.
	 */
	bool loadInstalledVersionFromExe(const QString& exePath);
	void useBuiltInVersion();

	//! Releases that are neither drafts nor (unless allowed) pre-releases.
	QList<GitHubRelease> publishedReleases() const;

	//! publishedReleases(), minus everything not newer than what is installed.
	QList<GitHubRelease> newerReleases() const;

	//! The newest published release, or an invalid one if there is none.
	GitHubRelease latestRelease() const;

	bool needsUpdate(const GitHubRelease& release) const;

	//! Find a release by tag or by version, for --install-version.
	GitHubRelease releaseForVersion(const QString& wanted) const;

	GitHubRelease askWhichRelease();
	void printReleases() const;

	// -- installing -------------------------------------------------------

	/*!
	 * The assets of \a release that belong on this installation.
	 *
	 * See the implementation for the rules; the short version is that the
	 * asset has to carry every token of this build's artifact name, agree
	 * about the CPU architecture, and be the right kind of package for how
	 * this copy was installed.
	 */
	QList<GitHubReleaseAsset>
	matchingAssets(const GitHubRelease& release) const;

	GitHubReleaseAsset
	askWhichAsset(const QList<GitHubReleaseAsset>& assets);

	void performUpdate(const GitHubRelease& release);

	//! Returns the downloaded file, or an empty string on failure.
	QString downloadAsset(const GitHubReleaseAsset& asset);

	void installFrom(const QString& archivePath);

	//! Run a downloaded installer and let it take over.
	void runInstaller(const QString& installerPath);

	//! Unpack, then hand the install over to a second pass.
	void unpackAndHandOff(const QString& archivePath);

	/*!
	 * Pick -- and if need be create -- the binary that will run the second
	 * pass from \a releaseRoot, and prove it can run before anything is
	 * committed to it.
	 *
	 * Three candidates, in the order they are preferred:
	 *
	 *   1. the updater inside the release being installed. Its libraries are
	 *      the ones it was built against and it leaves nothing behind, so
	 *      when it understands this protocol it is the right answer.
	 *   2. this updater, copied into the unpacked release. Used when the
	 *      release predates this protocol; it runs against that release's
	 *      libraries, which works upwards but not always backwards.
	 *   3. this updater, copied into a directory of its own beside the data
	 *      directory. For installations that do not carry their libraries
	 *      (a local build against the system Qt), where a plain copy runs.
	 *
	 * Every candidate is probed with --self-test, so "it cannot even start"
	 * is found here rather than after the installation has been backed up
	 * and the release staged.
	 *
	 * Returns the program to start, or an empty string if none of them ran.
	 */
	QString prepareInstallStageProgram(const QString& releaseRoot);

	//! Run \a program with --self-test and report whether it answered.
	bool probeUpdaterBinary(const QString& program);

	/*!
	 * The updater executable inside the installation (or unpacked release)
	 * at \a root, looking behind a bundle's loader.
	 */
	QString realUpdaterBinaryIn(const QString& root) const;

	//! This updater's own executable, resolving a sharun bundle's loader.
	QString ownUpdaterBinaryPath() const;

	//! Copy \a from to \a to, making the result executable.
	bool copyExecutable(const QString& from, const QString& to);

	/*!
	 * Wait for the second pass to take the lock over, and say whether it
	 * did.
	 *
	 * This is the difference between an update that happened and one that
	 * only appeared to: the second pass rewrites the lock with its own stage
	 * and process id as its first action, so seeing that is proof it is
	 * running our code and past its own startup.
	 */
	bool waitForInstallStageTakeover(qint64 stagePid);

	/*!
	 * Take the lock over, as the second pass, and report the first pass's
	 * process id so it can be waited for.
	 *
	 * Returns 0 when the lock says nothing about a first pass, which is what
	 * an updater from before this handshake left behind.
	 */
	qint64 claimLockForInstallStage(const QString& targetDir);

	/*!
	 * Delete the copies of this updater that a first pass may have put into
	 * the staged release.
	 *
	 * They are not part of the release, and the file list a release without
	 * a manifest falls back to copies whole directories -- so leaving them
	 * would install a stray updater under a name nothing ever runs.
	 */
	void removeInstallStageAgents(const QString& root);

	/*!
	 * Copy everything the update will overwrite into a timestamped directory
	 * inside the installation root.
	 *
	 * Only what is about to be replaced is copied. A portable installation
	 * keeps instances, saves and configuration in the same tree, and none of
	 * that is the updater's to duplicate.
	 */
	void backupInstallation();

	/*!
	 * Second pass: copy the release staged in \a sourceDir into \a targetDir
	 * and restart the launcher.
	 *
	 * Runs from outside the installation, so nothing it is about to overwrite
	 * is mapped by this process. \a sourceDir is passed rather than derived
	 * from this binary's own location, because the binary may be a copy that
	 * lives beside the staged release instead of inside it.
	 */
	void finishUpdate(const QString& sourceDir, const QString& targetDir);

	//! Hand an AppImage install over to AppImageUpdate (which speaks zsync).
	bool updateAppImage();

	// -- helpers ----------------------------------------------------------

	/*!
	 * The files an update touches, from manifest.txt when the installation
	 * has one, or a per-platform guess when it does not.
	 *
	 * Entries may be globs. manifest.txt is written by the packaging, so it
	 * is the authority on what belongs to Kite and what the user put there.
	 */
	QStringList installedFileList(const QDir& root) const;

	void fail(const QString& reason);
	void abortWith(const QString& reason);

	/*!
	 * Record a failure of the hand-off for installFrom() to report.
	 *
	 * The report has to come after the lock file has been dealt with, not
	 * before: showFatalError() puts up a modal dialog and does not return
	 * until it is answered, and a user who closes that dialog any other way
	 * -- or gives up and kills the updater -- would be left with a lock file
	 * for an update that never touched their installation. That is the
	 * "an update is already in progress" loop this exists to avoid.
	 */
	void deferFailure(const QString& title, const QString& text);

	//! Tell the user, then give up. Never returns to the caller's logic.
	void showFatalError(const QString& title, const QString& text);

	void clearUpdateLog();

	/*!
	 * Drop the update lock, for a failure that never reached the
	 * installation.
	 *
	 * The lock means "this installation may be a mix of two versions", and
	 * the launcher stops at startup to say so. Until the second pass has
	 * begun copying, that is not true: a download that failed, or a release
	 * that turned out to be unusable, leaves everything as it was. Making the
	 * user dismiss an "Update In Progress" warning for that teaches them to
	 * click through the one warning that matters.
	 */
	void releaseLockNothingChanged();

	/*!
	 * Delete all but the newest few backup directories.
	 *
	 * A backup is a full copy of the installation, so keeping one per update
	 * grows the installation without limit -- and on a portable install that
	 * directory is also the user's data directory.
	 */
	void pruneBackups();

	//! Log a line to both the debug log and the user-facing update log.
	void logUpdate(const QString& message);

	QString updaterBinaryName() const;
	QString launcherBinaryName() const;

	// -- state ------------------------------------------------------------

	QString m_rootPath; //!< Installation root that gets updated.
	QString m_dataPath; //!< Launcher data directory; markers and logs live here.
	QString m_updateLogPath;

	bool m_isPortable = false;
	bool m_isAppImage = false;
	bool m_isFlatpak = false;
	QString m_appImagePath;

	QString m_launcherExecutable;
	QUrl m_repositoryUrl;

	// Command line
	QString m_userSelectedVersionRaw;
	QString m_finishUpdateDir; //!< --finish-update: run as the second pass.
	bool m_checkOnly = false;
	bool m_forceUpdate = false;
	bool m_printOnly = false;
	bool m_selectUI = false;
	bool m_allowDowngrade = false;
	bool m_allowPreRelease = false;

	// Installed launcher identity
	QString m_installedVersion;
	QString m_installedGitCommit;

	QList<GitHubRelease> m_releases;
	GitHubRelease m_installRelease;

	/*!
	 * Whether the second pass was seen to take the update over.
	 *
	 * Only then does the lock file stop being this process's to clean up --
	 * and only then may this process report success.
	 */
	bool m_handedOver = false;

	/*!
	 * Whether a second pass was started that neither took the update over
	 * nor died.
	 *
	 * Nothing can be said about the installation in that case, so the lock
	 * stays where it is: the launcher's startup warning is the only thing
	 * that will tell the user their installation needs a look.
	 */
	bool m_installStateUnknown = false;

	// Set by deferFailure(), reported by installFrom().
	QString m_deferredFailureTitle;
	QString m_deferredFailureText;

	std::unique_ptr<QNetworkAccessManager> m_network;
	QString m_currentUrl;

	Status m_status = Starting;
};
