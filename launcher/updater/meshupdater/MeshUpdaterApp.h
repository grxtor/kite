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
#include "updater/UpdateManifest.h"

class QNetworkAccessManager;

/*! The standalone updater. Modes: --check-only (the exit status says whether an update exists, see
 * ExitCode; details go to stdout), --list (print the releases) and the default (install, then restart
 * the launcher).
 *
 * Installing takes two passes through this binary. The first, from the installation being replaced,
 * downloads and backs up, unpacks into a staging directory, then starts the *unpacked* updater and
 * exits, which releases the mapped libraries. The second finds the marker in the staging directory,
 * copies everything into the installation and relaunches. One process cannot do this on Windows: a
 * mapped image (the Qt libraries it runs from) cannot be deleted. */
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

	/*! Process exit codes. The launcher reads them, so they are an interface. 100 rather than 2 for
	 * "update available" so it cannot be confused with a crash or Qt's failure paths. */
	enum ExitCode {
		ExitSuccess = 0,
		ExitFailed = 1,
		ExitAborted = 2,
		ExitUpdateAvailable = 100,
	};

	MeshUpdaterApp(int& argc, char** argv);
	~MeshUpdaterApp() override;

	/*! Answers --self-test if that was asked. The first pass probes candidates for the second pass with
	 * it, since an updater from an older release does not understand this protocol. Called from main()
	 * before a QApplication exists: a probe must work without a display. */
	static bool handleSelfTest(int argc, char** argv);

	//! The option a probe passes, and the answer it expects on stdout.
	static constexpr auto kSelfTestOption = "--self-test";
	static constexpr auto kSelfTestToken = "KITE_UPDATER_SELF_TEST_OK";

	Status status() const
	{
		return m_status;
	}

	//! Open log file; public for the installed message handler.
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

	/*! Everything after the release list is known. Every path out of it exits the process. */
	void run();

	/*! Asks the installed launcher its version by running it with --version. The updater's own
	 * BuildConfig can be stale (updater replaced once, or run by hand against another installation).
	 * Falls back to BuildConfig when the launcher cannot be asked. */
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

	/*! The assets of \a release that belong on this installation: every token of this build's artifact
	 * name, the same CPU architecture, and the right package kind for how this copy was installed. */
	QList<GitHubReleaseAsset>
	matchingAssets(const GitHubRelease& release) const;

	/*! Fetches \a release's kite-update.json and its signature and keeps the manifest when the
	 * signature verifies against BuildConfig.UPDATE_PUBLIC_KEY and it names that release. Nothing is
	 * installed from a release without one. */
	bool loadManifest(const GitHubRelease& release, QString* error);

	/*! matchingAssets() narrowed to the files the loaded manifest lists, which are the only ones the
	 * updater can check. */
	QList<GitHubReleaseAsset>
	installableAssets(const GitHubRelease& release) const;

	//! A small file (the manifest, its signature) into memory; empty with \a error set on failure.
	QByteArray fetchSmallFile(const QUrl& url, qint64 maxBytes, QString* error);

	GitHubReleaseAsset
	askWhichAsset(const QList<GitHubReleaseAsset>& assets);

	void performUpdate(const GitHubRelease& release);

	//! Returns the downloaded file, or an empty string on failure.
	QString downloadAsset(const GitHubReleaseAsset& asset);

	void installFrom(const QString& archivePath);

	void runInstaller(const QString& installerPath);

	//! Unpack, then hand the install over to a second pass.
	void unpackAndHandOff(const QString& archivePath);

	/*! Picks, creating it if need be, the binary that runs the second pass from \a releaseRoot, and
	 * proves it runs before anything is committed. Candidates in order: (1) the updater inside the
	 * release being installed, (2) this updater copied into the unpacked release (the release predates
	 * this protocol), (3) this updater copied beside the data directory (installations that carry no
	 * libraries of their own). Each is probed with --self-test.
	 * Returns the program to start, or an empty string if none ran. */
	QString prepareInstallStageProgram(const QString& releaseRoot);

	//! Run \a program with --self-test and report whether it answered.
	bool probeUpdaterBinary(const QString& program);

	/*! The updater executable inside the installation (or unpacked release) at \a root, behind a bundle's
	 * loader. */
	QString realUpdaterBinaryIn(const QString& root) const;

	//! This updater's own executable, resolving a sharun bundle's loader.
	QString ownUpdaterBinaryPath() const;

	bool copyExecutable(const QString& from, const QString& to);

	/*! Waits for the second pass to take the lock over. It rewrites the lock with its own stage and pid as
	 * its first action, so seeing that proves it runs our code and is past its startup. */
	bool waitForInstallStageTakeover(qint64 stagePid);

	/*! Takes the lock over as the second pass and returns the first pass's process id to wait for; 0 when
	 * the lock names no first pass (an updater from before this handshake). */
	qint64 claimLockForInstallStage(const QString& targetDir);

	/*! Deletes the copies of this updater a first pass may have put into the staged release; the
	 * manifest-less file list copies whole directories and would install them. */
	void removeInstallStageAgents(const QString& root);

	/*! Copies only what the update will overwrite into a timestamped directory inside the installation
	 * root; a portable installation keeps user data in the same tree. */
	void backupInstallation();

	/*! Second pass: copies the release staged in \a sourceDir into \a targetDir and restarts the launcher.
	 * Runs from outside the installation, so nothing it overwrites is mapped. \a sourceDir is passed,
	 * not derived from this binary's location, because the binary may be a copy beside the staged release. */
	void finishUpdate(const QString& sourceDir, const QString& targetDir);

	/*! Puts the downloaded AppImage in the place of the running one (a rename, so the old file stays
	 * valid for whoever has it open) and starts it. */
	void installAppImage(const QString& downloadedPath);

	//! Writes the success or failure marker the launcher reports at its next start.
	void writeOutcomeMarker(bool failed);

	// -- helpers ----------------------------------------------------------

	/*! The files an update touches: manifest.txt (written by the packaging, entries may be globs) when
	 * present, else a per-platform guess. */
	QStringList installedFileList(const QDir& root) const;

	void fail(const QString& reason);
	void abortWith(const QString& reason);

	/*! Records a hand-off failure for installFrom() to report once the lock file is dealt with.
	 * showFatalError() blocks on a modal dialog; a user killing the updater there would be left with a lock
	 * for an update that never touched the installation (the "update already in progress" loop). */
	void deferFailure(const QString& title, const QString& text);

	//! Tell the user, then give up. Never returns to the caller's logic.
	void showFatalError(const QString& title, const QString& text);

	void clearUpdateLog();

	/*! Drops the update lock for a failure that never reached the installation. Until the second pass
	 * copies, everything is as it was; a needless "Update In Progress" warning teaches users to click
	 * through the one that matters. */
	void releaseLockNothingChanged();

	/*! Deletes all but the newest few backups: each is a full copy, and on a portable install that
	 * directory is also the user's data directory. */
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
	//! The package manager that owns this installation (UpdateLockFile::kManagedByFileName), if any.
	QString m_managedBy;
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
	//! --no-relaunch: the user closed the launcher and it should stay closed after the update.
	bool m_noRelaunch = false;

	// Installed launcher identity
	QString m_installedVersion;
	QString m_installedGitCommit;

	QList<GitHubRelease> m_releases;
	GitHubRelease m_installRelease;
	//! The signed file list of the release being checked or installed; see loadManifest().
	UpdateManifest::Manifest m_manifest;

	//! Whether the second pass was seen to take the update over; only then is the lock not this
	//! process's to clean up and success reportable.
	bool m_handedOver = false;

	//! Whether a second pass was started that neither took over nor died. The installation's state
	//! is unknown, so the lock stays for the launcher's startup warning.
	bool m_installStateUnknown = false;

	// Set by deferFailure(), reported by installFrom().
	QString m_deferredFailureTitle;
	QString m_deferredFailureText;

	std::unique_ptr<QNetworkAccessManager> m_network;
	QString m_currentUrl;

	Status m_status = Starting;
};
