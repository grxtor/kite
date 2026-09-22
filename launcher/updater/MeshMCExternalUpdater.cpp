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

#include "MeshMCExternalUpdater.h"

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QSettings>

#include <algorithm>
#include <climits>

#include "BuildConfig.h"
#include "core/LauncherContext.h"
#include "core/UiHost.h"

namespace
{

	// Config keys, in one place so the updater binary and the launcher cannot
	// drift apart on their spelling.
	constexpr auto kConfigFileName = "meshmc_update.cfg";
	constexpr auto kKeyAllowBeta = "allow_beta";
	constexpr auto kKeyAutoCheck = "auto_check";
	constexpr auto kKeyUpdateInterval = "update_interval";
	constexpr auto kKeyLastCheck = "last_check";
	constexpr auto kGroupSkip = "skip";

	//! Once a day, when the config says nothing.
	constexpr int kDefaultIntervalSeconds = 86400;

	//! The check is a child process; give it a chance to start, then to finish.
	constexpr int kStartTimeoutMs = 5000;
	constexpr int kFinishTimeoutMs = 60000;

	/*!
	 * Exit codes of `meshmc-updater --check-only`.
	 *
	 * 100 is used for "yes" rather than 2 so that it cannot be confused with
	 * a crash, a signal, or Qt's own failure paths.
	 */
	enum CheckExitCode {
		NoUpdate = 0,
		CheckError = 1,
		UpdateAvailable = 100,
	};

	/*!
	 * Folds raw child output into the message body.
	 *
	 * UiHost::message() has no "Show Details" affordance to tuck this
	 * behind, so it goes in the body instead when there is any.
	 */
	QString withDetails(const QString& text, const QString& details = QString())
	{
		if (details.isEmpty())
			return text;
		return text + QLatin1String("\n\n") + details;
	}

	/*!
	 * Split off the first line of \a text, returning it and leaving the rest
	 * in \a text.
	 *
	 * The check protocol is three header lines followed by release notes that
	 * are themselves multi-line, so the parser consumes exactly three lines
	 * and treats everything after them as the body.
	 */
	QString takeLine(QString& text)
	{
		const int newline = text.indexOf(QLatin1Char('\n'));
		if (newline < 0) {
			const QString line = text;
			text.clear();
			return line;
		}
		const QString line = text.left(newline);
		text.remove(0, newline + 1);
		return line;
	}

	//! Value of a `Key: value` header line. Empty when the line is malformed.
	QString headerValue(const QString& line)
	{
		const int separator = line.indexOf(QLatin1String(": "));
		if (separator < 0)
			return {};
		return line.mid(separator + 2).trimmed();
	}

} // namespace

QString MeshMCExternalUpdater::updaterBinaryRelativePath()
{
#if defined(Q_OS_WIN32)
	// Windows keeps the executables at the installation root.
	return QStringLiteral("meshmc-updater.exe");
#else
	// Everywhere else the root is a prefix, with the binaries under bin/.
	return QStringLiteral("bin/meshmc-updater");
#endif
}

MeshMCExternalUpdater::MeshMCExternalUpdater(const QString& appDir,
											 const QString& dataDir,
											 bool autoCheckDefault)
	: m_appDir(appDir), m_dataDir(dataDir)
{
	m_settings = std::make_unique<QSettings>(
		m_dataDir.absoluteFilePath(QLatin1String(kConfigFileName)),
		QSettings::IniFormat);

	m_allowBeta = m_settings->value(kKeyAllowBeta, false).toBool();

	// First run against this config: adopt whatever the launcher's own
	// setting used to say, then record it so this only happens once.
	if (!m_settings->contains(kKeyAutoCheck)) {
		m_autoCheck = autoCheckDefault;
		m_settings->setValue(kKeyAutoCheck, m_autoCheck);
	} else {
		m_autoCheck = m_settings->value(kKeyAutoCheck).toBool();
	}

	bool intervalOk = false;
	m_updateInterval =
		m_settings->value(kKeyUpdateInterval, kDefaultIntervalSeconds)
			.toInt(&intervalOk);
	if (!intervalOk) {
		// A hand-edited config with garbage in it must not disable updates
		// forever; fall back rather than treating it as "On Launch".
		qWarning() << "Updater: unreadable" << kKeyUpdateInterval
				   << "in the update config; using the default.";
		m_updateInterval = kDefaultIntervalSeconds;
	}

	if (const QVariant stored = m_settings->value(kKeyLastCheck);
		stored.isValid() && !stored.isNull()) {
		m_lastCheck = QDateTime::fromString(stored.toString(), Qt::ISODate);
	}

	// Single-shot: every path out of a check re-arms the timer itself, so a
	// repeating timer would only add the risk of a check storm if one of them
	// ever failed to.
	m_updateTimer.setSingleShot(true);
	connect(&m_updateTimer, &QTimer::timeout, this,
			&MeshMCExternalUpdater::autoCheckTimerFired);

	resetAutoCheckTimer();

	if (m_autoCheck && qFuzzyIsNull(m_updateInterval)) {
		// "On Launch": check right now, quietly.
		checkForUpdates(false);
	}
}

MeshMCExternalUpdater::~MeshMCExternalUpdater()
{
	m_updateTimer.stop();
	disconnect(&m_updateTimer, &QTimer::timeout, this,
			   &MeshMCExternalUpdater::autoCheckTimerFired);
	m_settings->sync();
}

QString MeshMCExternalUpdater::updaterExecutablePath() const
{
	return m_appDir.absoluteFilePath(updaterBinaryRelativePath());
}

QStringList MeshMCExternalUpdater::commonArguments() const
{
	// The updater resolves its own paths from the data directory, so it is
	// told which one to use rather than guessing -- a portable install and a
	// system-wide one disagree about that, and only the launcher knows which
	// this is.
	QStringList args = {QStringLiteral("--dir"), m_dataDir.absolutePath()};
	if (m_allowBeta)
		args.append(QStringLiteral("--pre-release"));
	return args;
}

void MeshMCExternalUpdater::checkForUpdates()
{
	checkForUpdates(true);
}

void MeshMCExternalUpdater::checkForUpdates(bool triggeredByUser)
{
	if (m_checking) {
		qDebug() << "Updater: a check is already running; ignoring.";
		return;
	}

	m_checking = true;
	emit canCheckForUpdatesChanged(false);

	// The check blocks, so the progress dialog exists to prove the launcher
	// has not simply frozen. An automatic check gets none: nobody asked, and
	// a window stealing focus during startup is worse than no feedback.
	QProgressDialog progress(tr("Checking for updates..."), QString(), 0, 0,
							 nullptr);
	progress.setWindowTitle(tr("Checking for updates..."));
	progress.setMinimumDuration(0);
	progress.setCancelButton(nullptr);
	progress.adjustSize();
	if (triggeredByUser)
		progress.show();
	QCoreApplication::processEvents();

	QProcess proc;
#if defined(Q_OS_WIN32)
	// Keep Windows from deciding an executable called "*updater*" needs to be
	// elevated; the check only reads.
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("__COMPAT_LAYER"),
			   QStringLiteral("RUNASINVOKER"));
	proc.setProcessEnvironment(env);
#endif

	QStringList args = {QStringLiteral("--check-only")};
	args.append(commonArguments());
	args.append(QStringLiteral("--debug"));

	const QString program = updaterExecutablePath();
	qDebug() << "Updater: running check:" << program << args;
	proc.start(program, args);

	if (!proc.waitForStarted(kStartTimeoutMs)) {
		qWarning() << "Updater: the check did not start within"
				   << kStartTimeoutMs / 1000 << "seconds:" << proc.error()
				   << proc.errorString();
		progress.cancel();
		LAUNCHER->uiHost()->message(
			tr("Update Check Failed"),
			tr("Failed to start after 5 seconds\nReason: %1.")
				.arg(proc.errorString()),
			UiHost::Severity::Information);
		noteCheckCompleted();
		return;
	}
	QCoreApplication::processEvents();

	if (!proc.waitForFinished(kFinishTimeoutMs)) {
		const QByteArray output = proc.readAll();
		proc.kill();
		proc.waitForFinished(1000);
		qWarning() << "Updater: the check did not finish within"
				   << kFinishTimeoutMs / 1000 << "seconds:" << proc.error()
				   << proc.errorString();
		progress.cancel();
		LAUNCHER->uiHost()->message(
			tr("Update Check Failed"),
			withDetails(tr("Updater failed to close 60 seconds\nReason: %1.")
							.arg(proc.errorString()),
						QString::fromUtf8(output)),
			UiHost::Severity::Information);
		noteCheckCompleted();
		return;
	}

	const int exitCode = proc.exitCode();
	const QByteArray stdOutput = proc.readAllStandardOutput();
	const QByteArray stdError = proc.readAllStandardError();

	progress.cancel();
	QCoreApplication::processEvents();

	switch (exitCode) {
		case CheckExitCode::NoUpdate:
			qDebug() << "Updater: no update available.";
			if (triggeredByUser) {
				LAUNCHER->uiHost()->message(tr("No Update Available"),
											tr("You are running the latest "
											   "version."),
											UiHost::Severity::Information);
			}
			break;

		case CheckExitCode::CheckError:
			qWarning() << "Updater: the check reported an error:"
					   << qPrintable(QString::fromUtf8(stdError));
			LAUNCHER->uiHost()->message(
				tr("Update Check Error"),
				withDetails(tr("There was an error running the update check."),
							QString::fromUtf8(stdError)),
				UiHost::Severity::Warning);
			break;

		case CheckExitCode::UpdateAvailable: {
			// Three header lines, then the release notes verbatim:
			//
			//     Name: MeshMC 7.20.0
			//     Version: v7.20.0
			//     TimeStamp: 2026-09-06T12:00:00Z
			//     <markdown release notes, to end of output>
			QString remainder = QString::fromUtf8(stdOutput);
			const QString versionName = headerValue(takeLine(remainder));
			const QString versionTag = headerValue(takeLine(remainder));
			const QString timestamp = headerValue(takeLine(remainder));
			const QString releaseNotes = remainder;

			qDebug() << "Updater: update available:" << versionName
					 << versionTag << timestamp;

			if (versionTag.isEmpty()) {
				// Without a tag there is nothing to install and nothing to
				// remember as skipped, so this is an error, not an offer.
				qWarning() << "Updater: the check reported an update but no "
							  "version tag.";
				LAUNCHER->uiHost()->message(
					tr("Update Check Error"),
					withDetails(
						tr("There was an error running the update check."),
						tr("StdOut: %1\nStdErr: %2")
							.arg(QString::fromUtf8(stdOutput),
								 QString::fromUtf8(stdError))),
					UiHost::Severity::Warning);
				break;
			}

			offerUpdate(versionName.isEmpty() ? versionTag : versionName,
						versionTag, releaseNotes, triggeredByUser);
			break;
		}

		default:
			qWarning() << "Updater: the check exited with an unknown code"
					   << exitCode;
			LAUNCHER->uiHost()->message(
				tr("Unknown Update Error"),
				withDetails(
					tr("The updater exited with an unknown condition.\nExit "
					   "Code: %1")
						.arg(QString::number(exitCode)),
					tr("StdOut: %1\nStdErr: %2")
						.arg(QString::fromUtf8(stdOutput),
							 QString::fromUtf8(stdError))),
				UiHost::Severity::Information);
	}

	noteCheckCompleted();
}

void MeshMCExternalUpdater::noteCheckCompleted()
{
	m_lastCheck = QDateTime::currentDateTime();
	m_settings->setValue(kKeyLastCheck, m_lastCheck.toString(Qt::ISODate));
	m_settings->sync();

	m_checking = false;
	resetAutoCheckTimer();
	emit canCheckForUpdatesChanged(true);
}

void MeshMCExternalUpdater::offerUpdate(const QString& versionName,
										const QString& versionTag,
										const QString& releaseNotes,
										bool triggeredByUser)
{
	m_settings->beginGroup(kGroupSkip);
	const bool skipped = m_settings->value(versionTag, false).toBool();
	m_settings->endGroup();

	// A skip only silences the automatic checks. Asking explicitly overrides
	// it -- otherwise "Check for Updates" would answer nothing at all and
	// look broken.
	if (skipped && !triggeredByUser) {
		qDebug() << "Updater:" << versionTag
				 << "was skipped by the user; staying quiet.";
		return;
	}

	const UiHost::UpdateChoice choice = LAUNCHER->uiHost()->offerUpdate(
		BuildConfig.printableVersionString(), versionName, releaseNotes);

	m_settings->beginGroup(kGroupSkip);
	switch (choice) {
		case UiHost::UpdateChoice::Skip:
			qDebug() << "Updater: remembering" << versionTag << "as skipped.";
			m_settings->setValue(versionTag, true);
			break;

		case UiHost::UpdateChoice::Install:
			// Forget any earlier skip: the user just chose to install this
			// very version.
			m_settings->remove(versionTag);
			m_settings->endGroup();
			m_settings->sync();
			performUpdate(versionTag);
			return;

		case UiHost::UpdateChoice::Later:
			// "Remind Me Later", or the window was simply closed.
			qDebug() << "Updater: leaving" << versionTag << "for later.";
			m_settings->remove(versionTag);
			break;
	}
	m_settings->endGroup();
	m_settings->sync();
}

void MeshMCExternalUpdater::performUpdate(const QString& versionTag)
{
	QProcess proc;
#if defined(Q_OS_WIN32)
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("__COMPAT_LAYER"),
			   QStringLiteral("RUNASINVOKER"));
	proc.setProcessEnvironment(env);
#endif

	QStringList args = commonArguments();
	args.append({QStringLiteral("--install-version"), versionTag});

	proc.setProgram(updaterExecutablePath());
	proc.setArguments(args);

	qDebug() << "Updater: starting install of" << versionTag << ":"
			 << proc.program() << args;

	if (!proc.startDetached()) {
		qCritical() << "Updater: failed to start the updater:" << proc.error()
					<< proc.errorString();
		LAUNCHER->uiHost()->message(
			tr("Update Failed"),
			tr("Could not start the updater.\nReason: %1.")
				.arg(proc.errorString()),
			UiHost::Severity::Warning);
		return;
	}

	// The updater has to replace the files this process is running from, so
	// there is nothing left to do here but leave. It waits for us to be gone
	// before it touches anything.
	QCoreApplication::exit();
}

bool MeshMCExternalUpdater::getAutomaticallyChecksForUpdates()
{
	return m_autoCheck;
}

double MeshMCExternalUpdater::getUpdateCheckInterval()
{
	return m_updateInterval;
}

bool MeshMCExternalUpdater::getBetaAllowed()
{
	return m_allowBeta;
}

void MeshMCExternalUpdater::setAutomaticallyChecksForUpdates(bool check)
{
	m_autoCheck = check;
	m_settings->setValue(kKeyAutoCheck, check);
	m_settings->sync();
	resetAutoCheckTimer();
}

void MeshMCExternalUpdater::setUpdateCheckInterval(double seconds)
{
	m_updateInterval = seconds;
	m_settings->setValue(kKeyUpdateInterval, seconds);
	m_settings->sync();
	resetAutoCheckTimer();
}

void MeshMCExternalUpdater::setBetaAllowed(bool allowed)
{
	m_allowBeta = allowed;
	m_settings->setValue(kKeyAllowBeta, allowed);
	m_settings->sync();
}

void MeshMCExternalUpdater::resetAutoCheckTimer()
{
	if (!m_autoCheck || m_updateInterval <= 0.0) {
		// Disabled, or "On Launch" -- which is handled at startup and needs
		// no timer at all.
		m_updateTimer.stop();
		return;
	}

	qint64 timeoutMs = 0;
	if (m_lastCheck.isValid()) {
		const qint64 elapsed = m_lastCheck.secsTo(QDateTime::currentDateTime());
		const qint64 remaining = std::max<qint64>(
			static_cast<qint64>(m_updateInterval) - elapsed, 0);
		timeoutMs = remaining * 1000;
	}
	// No record of a previous check leaves timeoutMs at 0, i.e. check as soon
	// as the event loop turns. That is intentional: a fresh install should
	// find out it is out of date without waiting a day first.

	timeoutMs = std::min<qint64>(timeoutMs, INT_MAX);

	qDebug() << "Updater: next automatic check in" << timeoutMs / 1000
			 << "seconds.";
	m_updateTimer.start(static_cast<int>(timeoutMs));
}

void MeshMCExternalUpdater::autoCheckTimerFired()
{
	qDebug() << "Updater: automatic check due.";
	checkForUpdates(false);
}
