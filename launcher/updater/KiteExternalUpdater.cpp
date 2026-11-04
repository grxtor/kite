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

#include "KiteExternalUpdater.h"

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>

#include <algorithm>
#include <climits>
#include <utility>

#include "BuildConfig.h"
#include "core/LauncherContext.h"
#include "core/UiHost.h"

namespace
{

	// Config keys in one place so the updater binary and the launcher agree on spelling.
	constexpr auto kConfigFileName = "kite_update.cfg";
	constexpr auto kKeyAllowBeta = "allow_beta";
	constexpr auto kKeyAutoCheck = "auto_check";
	constexpr auto kKeyLastCheck = "last_check";
	constexpr auto kGroupSkip = "skip";

	//! Automatic checks: once a day, after the one at start.
	constexpr qint64 kCheckIntervalSeconds = 86400;

	//! The check at start waits for the launcher's own start-up traffic to settle.
	constexpr int kStartupCheckDelayMs = 10000;

	constexpr int kStartTimeoutMs = 5000;
	constexpr int kFinishTimeoutMs = 60000;

	/*! Exit codes of `kite-updater --check-only`. 100 rather than 2 for "yes" so it cannot be
	 * confused with a crash, a signal or Qt's failure paths. */
	enum CheckExitCode {
		NoUpdate = 0,
		CheckError = 1,
		UpdateAvailable = 100,
	};

	/*! Folds raw child output into the message body: UiHost::message() has no "Show Details"
	 * affordance. */
	QString withDetails(const QString& text, const QString& details = QString())
	{
		if (details.isEmpty())
			return text;
		return text + QLatin1String("\n\n") + details;
	}

	/*! Splits off the first line of \a text and returns it. The check protocol is three header lines
	 * followed by multi-line release notes, so the parser takes exactly three lines and treats the
	 * rest as the body. */
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

QString KiteExternalUpdater::updaterBinaryRelativePath()
{
#if defined(Q_OS_WIN32)
	// Windows keeps the executables at the installation root.
	return QStringLiteral("kite-updater.exe");
#else
	// Everywhere else the root is a prefix, with the binaries under bin/.
	return QStringLiteral("bin/kite-updater");
#endif
}

KiteExternalUpdater::KiteExternalUpdater(const QString& appDir,
										 const QString& dataDir,
										 bool autoCheckDefault,
										 const QString& managedBy)
	: m_appDir(appDir), m_dataDir(dataDir), m_managedBy(managedBy)
{
	m_settings = std::make_unique<QSettings>(
		m_dataDir.absoluteFilePath(QLatin1String(kConfigFileName)),
		QSettings::IniFormat);

	m_allowBeta = m_settings->value(kKeyAllowBeta, false).toBool();

	// First run against this config: adopt the launcher's own setting, then record it so this
	// happens once.
	if (!m_settings->contains(kKeyAutoCheck)) {
		m_autoCheck = autoCheckDefault;
		m_settings->setValue(kKeyAutoCheck, m_autoCheck);
	} else {
		m_autoCheck = m_settings->value(kKeyAutoCheck).toBool();
	}

	if (const QVariant stored = m_settings->value(kKeyLastCheck);
		stored.isValid() && !stored.isNull()) {
		m_lastCheck = QDateTime::fromString(stored.toString(), Qt::ISODate);
	}

	// Single-shot: every path out of a check re-arms the timer, so a repeating timer would only
	// risk a check storm.
	m_updateTimer.setSingleShot(true);
	connect(&m_updateTimer, &QTimer::timeout, this,
			&KiteExternalUpdater::autoCheckTimerFired);

	m_checkTimeout.setSingleShot(true);
	connect(&m_checkTimeout, &QTimer::timeout, this,
			&KiteExternalUpdater::onCheckTimedOut);

	if (m_autoCheck) {
		// Every start checks, whenever the last check was; the daily schedule follows from it.
		m_updateTimer.start(kStartupCheckDelayMs);
	}
}

KiteExternalUpdater::~KiteExternalUpdater()
{
	m_updateTimer.stop();
	disconnect(&m_updateTimer, &QTimer::timeout, this,
			   &KiteExternalUpdater::autoCheckTimerFired);
	releaseCheckProcess();
	m_settings->sync();
}

QString KiteExternalUpdater::updaterExecutablePath() const
{
	return m_appDir.absoluteFilePath(updaterBinaryRelativePath());
}

QStringList KiteExternalUpdater::commonArguments() const
{
	// The updater is told the data directory rather than guessing: portable and system-wide
	// installs disagree, and only the launcher knows.
	QStringList args = {QStringLiteral("--dir"), m_dataDir.absolutePath()};
	if (m_allowBeta)
		args.append(QStringLiteral("--pre-release"));
	return args;
}

void KiteExternalUpdater::checkForUpdates()
{
	checkForUpdates(true);
}

void KiteExternalUpdater::checkForUpdates(bool triggeredByUser)
{
	if (m_checking) {
		// An automatic check is running and the user asked: let the running
		// one report its result to them.
		if (triggeredByUser && !m_checkByUser) {
			m_checkByUser = true;
			if (!m_busy)
				m_busy = LAUNCHER->uiHost()->showBusy(
					tr("Checking for updates..."));
		}
		qDebug() << "Updater: a check is already running; not starting another.";
		return;
	}

	m_checking = true;
	m_checkByUser = triggeredByUser;

	// A busy indication shows the launcher is not frozen. Automatic checks get none: a window
	// stealing focus at startup is worse than no feedback.
	if (triggeredByUser)
		m_busy = LAUNCHER->uiHost()->showBusy(tr("Checking for updates..."));

	auto* proc = new QProcess(this);
	m_checkProcess = proc;
#if defined(Q_OS_WIN32)
	// Keep Windows from deciding an executable called "*updater*" needs to be
	// elevated; the check only reads.
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("__COMPAT_LAYER"),
			   QStringLiteral("RUNASINVOKER"));
	proc->setProcessEnvironment(env);
#endif

	QStringList args = {QStringLiteral("--check-only")};
	args.append(commonArguments());
	args.append(QStringLiteral("--debug"));

	connect(proc, &QProcess::started, this,
			[this] { m_checkTimeout.start(kFinishTimeoutMs); });
	connect(proc, &QProcess::errorOccurred, this,
			[this, proc](QProcess::ProcessError error) {
				if (error != QProcess::FailedToStart)
					return;
				const QString reason = proc->errorString();
				qWarning() << "Updater: the check did not start:" << error
						   << reason;
				onCheckStartFailed(reason);
			});
	connect(proc, &QProcess::finished, this,
			[this](int exitCode, QProcess::ExitStatus) {
				onCheckFinished(exitCode);
			});

	const QString program = updaterExecutablePath();
	qDebug() << "Updater: running check:" << program << args;
	m_checkTimeout.start(kStartTimeoutMs);
	proc->start(program, args);
}

void KiteExternalUpdater::releaseCheckProcess()
{
	m_checkTimeout.stop();
	m_busy.reset();
	if (QProcess* proc = m_checkProcess.data()) {
		proc->disconnect(this);
		if (proc->state() != QProcess::NotRunning)
			proc->kill();
		proc->deleteLater();
	}
	m_checkProcess.clear();
}

void KiteExternalUpdater::onCheckStartFailed(const QString& reason)
{
	releaseCheckProcess();
	// An automatic check that fails says so in the log only: a dialog at start, or once a day,
	// about a network hiccup helps nobody.
	if (m_checkByUser) {
		LAUNCHER->uiHost()->message(
			tr("Update Check Failed"),
			tr("Failed to start the updater.\nReason: %1.").arg(reason),
			UiHost::Severity::Information);
	}
	noteCheckCompleted();
}

void KiteExternalUpdater::onCheckTimedOut()
{
	QProcess* proc = m_checkProcess.data();
	if (!proc)
		return;
	const bool started = proc->state() == QProcess::Running;
	const QByteArray output = proc->readAll();
	qWarning() << "Updater: the check did not"
			   << (started ? "finish" : "start") << "within"
			   << (started ? kFinishTimeoutMs : kStartTimeoutMs) / 1000
			   << "seconds.";
	releaseCheckProcess();
	// See onCheckStartFailed() for why an automatic check stays quiet.
	if (m_checkByUser && started) {
		LAUNCHER->uiHost()->message(
			tr("Update Check Failed"),
			withDetails(tr("The updater did not finish within 60 seconds."),
						QString::fromUtf8(output)),
			UiHost::Severity::Information);
	} else if (m_checkByUser) {
		LAUNCHER->uiHost()->message(
			tr("Update Check Failed"),
			tr("The updater did not start within 5 seconds."),
			UiHost::Severity::Information);
	}
	noteCheckCompleted();
}

void KiteExternalUpdater::onCheckFinished(int exitCode)
{
	QProcess* proc = m_checkProcess.data();
	if (!proc)
		return;
	const QByteArray stdOutput = proc->readAllStandardOutput();
	const QByteArray stdError = proc->readAllStandardError();
	releaseCheckProcess();
	handleCheckResult(exitCode, stdOutput, stdError);
	noteCheckCompleted();
}

void KiteExternalUpdater::handleCheckResult(int exitCode,
											const QByteArray& stdOutput,
											const QByteArray& stdError)
{
	const bool triggeredByUser = m_checkByUser;

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
			if (triggeredByUser) {
				LAUNCHER->uiHost()->message(
					tr("Update Check Error"),
					withDetails(
						tr("There was an error running the update check."),
						QString::fromUtf8(stdError)),
					UiHost::Severity::Warning);
			}
			break;

		case CheckExitCode::UpdateAvailable: {
			// Three header lines, then the release notes verbatim:
			//     Name: Kite 7.20.0
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
				// Without a tag there is nothing to install or to remember as skipped: an error,
				// not an offer.
				qWarning() << "Updater: the check reported an update but no "
							  "version tag.";
				if (!triggeredByUser)
					break;
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

			offerUpdate(versionTag, releaseNotes, triggeredByUser);
			break;
		}

		default:
			qWarning() << "Updater: the check exited with an unknown code"
					   << exitCode;
			if (!triggeredByUser)
				break;
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
}

void KiteExternalUpdater::noteCheckCompleted()
{
	m_lastCheck = QDateTime::currentDateTime();
	m_settings->setValue(kKeyLastCheck, m_lastCheck.toString(Qt::ISODate));
	m_settings->sync();

	m_checking = false;
	resetAutoCheckTimer();
	emit stateChanged();
}

void KiteExternalUpdater::offerUpdate(const QString& versionTag,
									  const QString& releaseNotes,
									  bool triggeredByUser)
{
	m_settings->beginGroup(kGroupSkip);
	const bool skipped = m_settings->value(versionTag, false).toBool();
	m_settings->endGroup();

	// A skip silences only automatic checks; an explicit "Check for Updates" overrides it, else it
	// would look broken. So does an update already waiting for Kite to close.
	if (!triggeredByUser && (skipped || versionTag == m_pendingTag)) {
		qDebug() << "Updater:" << versionTag
				 << (skipped ? "was skipped by the user" : "is already waiting")
				 << "; staying quiet.";
		return;
	}

	UiHost::UpdateOffer offer;
	offer.currentVersion = BuildConfig.printableVersionString();
	// "1.3.0" from the tag "v1.3.0": the release name ("Kite 1.3.0") would repeat the product name.
	offer.availableVersion =
		versionTag.startsWith(QLatin1Char('v')) ? versionTag.mid(1) : versionTag;
	offer.releaseNotes = releaseNotes;
	offer.canInstallNow = !m_gamesRunning;
	offer.managedHint = managedUpdateHint(m_managedBy);

	const UiHost::UpdateChoice choice = LAUNCHER->uiHost()->offerUpdate(offer);

	// A package manager installs it; whatever came back, there is nothing to start.
	if (!m_managedBy.isEmpty() && choice != UiHost::UpdateChoice::Skip) {
		qDebug() << "Updater:" << m_managedBy << "installs" << versionTag;
		return;
	}

	m_settings->beginGroup(kGroupSkip);
	switch (choice) {
		case UiHost::UpdateChoice::Skip:
			qDebug() << "Updater: remembering" << versionTag << "as skipped.";
			m_settings->setValue(versionTag, true);
			break;

		case UiHost::UpdateChoice::Install:
		case UiHost::UpdateChoice::InstallOnExit:
			// Forget any earlier skip: the user chose to install this version.
			m_settings->remove(versionTag);
			m_settings->endGroup();
			m_settings->sync();
			if (choice == UiHost::UpdateChoice::Install && !m_gamesRunning) {
				performUpdate(versionTag);
				return;
			}
			// A game started while the question was open counts as "when I close Kite".
			qDebug() << "Updater:" << versionTag << "installs when Kite closes.";
			m_pendingTag = versionTag;
			m_pendingVersion = offer.availableVersion;
			emit stateChanged();
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

bool KiteExternalUpdater::startInstall(const QString& versionTag, bool relaunch)
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
	if (!relaunch)
		args.append(QStringLiteral("--no-relaunch"));

	proc.setProgram(updaterExecutablePath());
	proc.setArguments(args);

	qDebug() << "Updater: starting install of" << versionTag << ":"
			 << proc.program() << args;

	if (!proc.startDetached()) {
		qCritical() << "Updater: failed to start the updater:" << proc.error()
					<< proc.errorString();
		return false;
	}
	return true;
}

void KiteExternalUpdater::performUpdate(const QString& versionTag)
{
	// Whatever waited for Kite to close is this install now; applicationExiting() must not start a
	// second one as Kite quits.
	m_pendingTag.clear();
	m_pendingVersion.clear();

	if (!startInstall(versionTag, true)) {
		emit stateChanged();
		LAUNCHER->uiHost()->message(
			tr("Update Failed"),
			tr("Could not start the updater.\nReason: %1.")
				.arg(tr("the program at %1 did not start")
						 .arg(QDir::toNativeSeparators(updaterExecutablePath()))),
			UiHost::Severity::Warning);
		return;
	}

	// The updater replaces the files this process runs from, so leave; it waits for us to be gone.
	QCoreApplication::exit();
}

QDateTime KiteExternalUpdater::lastCheck()
{
	return m_lastCheck;
}

QString KiteExternalUpdater::pendingInstall()
{
	return m_pendingVersion;
}

QString KiteExternalUpdater::managedBy()
{
	return managerDisplayName(m_managedBy);
}

void KiteExternalUpdater::installPendingNow()
{
	if (m_pendingTag.isEmpty() || m_gamesRunning)
		return;
	performUpdate(QString(m_pendingTag));
}

void KiteExternalUpdater::applicationExiting()
{
	if (m_pendingTag.isEmpty())
		return;
	if (m_gamesRunning) {
		// Quitting with a game still up (the Quit menu): its files stay; the update is offered
		// again next time.
		qWarning() << "Updater: a game is still running; not installing"
				   << m_pendingTag << "on exit.";
		return;
	}
	qDebug() << "Updater: Kite is closing; installing" << m_pendingTag;
	// No relaunch: the user closed Kite and expects it to stay closed.
	startInstall(std::exchange(m_pendingTag, QString()), false);
}

QString KiteExternalUpdater::managerDisplayName(const QString& managedBy)
{
	if (managedBy == QLatin1String("scoop"))
		return QStringLiteral("Scoop");
	if (managedBy == QLatin1String("flatpak"))
		return QStringLiteral("Flatpak");
	return managedBy;
}

QString KiteExternalUpdater::managedUpdateHint(const QString& managedBy)
{
	if (managedBy.isEmpty())
		return {};
	if (managedBy == QLatin1String("scoop"))
		return tr("Scoop keeps Kite up to date on this computer. To update "
				  "now, run:\nscoop update kite");
	if (managedBy == QLatin1String("pacman"))
		return tr("Your package manager keeps Kite (the AUR package "
				  "kite-bin) up to date. To update now, run for example:\n"
				  "yay -Syu kite-bin");
	if (managedBy == QLatin1String("flatpak"))
		return tr("Flatpak keeps Kite up to date. To update now, run:\n"
				  "flatpak update %1")
			.arg(BuildConfig.KITE_APPID);
	return tr("%1 keeps Kite up to date on this computer; update it from "
			  "there.")
		.arg(managedBy);
}

bool KiteExternalUpdater::getAutomaticallyChecksForUpdates()
{
	return m_autoCheck;
}

bool KiteExternalUpdater::getBetaAllowed()
{
	return m_allowBeta;
}

void KiteExternalUpdater::setAutomaticallyChecksForUpdates(bool check)
{
	m_autoCheck = check;
	m_settings->setValue(kKeyAutoCheck, check);
	m_settings->sync();
	resetAutoCheckTimer();
}

void KiteExternalUpdater::setBetaAllowed(bool allowed)
{
	m_allowBeta = allowed;
	m_settings->setValue(kKeyAllowBeta, allowed);
	m_settings->sync();
}

void KiteExternalUpdater::resetAutoCheckTimer()
{
	if (!m_autoCheck) {
		m_updateTimer.stop();
		return;
	}

	qint64 timeoutMs = 0;
	if (m_lastCheck.isValid()) {
		const qint64 elapsed = m_lastCheck.secsTo(QDateTime::currentDateTime());
		const qint64 remaining =
			std::max<qint64>(kCheckIntervalSeconds - elapsed, 0);
		timeoutMs = remaining * 1000;
	}
	// No record of a previous check leaves timeoutMs at 0: check as soon as the event loop turns,
	// so a fresh install learns it is out of date without waiting a day.

	timeoutMs = std::min<qint64>(timeoutMs, INT_MAX);

	qDebug() << "Updater: next automatic check in" << timeoutMs / 1000
			 << "seconds.";
	m_updateTimer.start(static_cast<int>(timeoutMs));
}

void KiteExternalUpdater::autoCheckTimerFired()
{
	qDebug() << "Updater: automatic check due.";
	checkForUpdates(false);
}
