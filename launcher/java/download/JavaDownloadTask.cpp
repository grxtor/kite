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

#include "JavaDownloadTask.h"
#include "Logging.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <QDebug>

#include "FileSystem.h"
#include "Json.h"
#include "net/Download.h"
#include "net/ChecksumValidator.h"
#include "MMCZip.h"
#include "core/LauncherContext.h"
#include "java/JavaUtils.h"

namespace
{
	constexpr int TAR_TIMEOUT_MS = 300000;
	constexpr int TAR_POLL_MS = 250;

	QString findJava(const QString& dir)
	{
		const QString found = JavaUtils::findJavaBinary(dir);
		if (!found.isEmpty()) {
			return found;
		}
#if defined(Q_OS_WIN)
		const QString binaryName = "javaw.exe";
#else
		const QString binaryName = "java";
#endif
		// Fallback: any match
		QDirIterator it(dir, QStringList() << binaryName, QDir::Files,
						QDirIterator::Subdirectories);
		if (it.hasNext()) {
			it.next();
			return it.filePath();
		}
		return QString();
	}

	bool runTar(const QString& archivePath, const QString& targetDir,
				const std::atomic_bool& cancelled)
	{
		QProcess tarProcess;
		tarProcess.setWorkingDirectory(targetDir);
		tarProcess.start("tar", QStringList() << "xzf" << archivePath);
		QElapsedTimer timer;
		timer.start();
		while (tarProcess.state() != QProcess::NotRunning &&
			   !tarProcess.waitForFinished(TAR_POLL_MS)) {
			if (cancelled.load() || timer.hasExpired(TAR_TIMEOUT_MS)) {
				tarProcess.kill();
				tarProcess.waitForFinished();
				break;
			}
		}
		const bool success = tarProcess.exitCode() == 0 &&
							 tarProcess.exitStatus() == QProcess::NormalExit;
		if (!success) {
			qWarning() << "tar extraction failed:"
					   << tarProcess.readAllStandardError();
		}
		return success;
	}

	// Runs on a worker thread: touches no QObject.
	JavaInstallOutcome extractAndPrepare(const QString& archivePath,
										 const QString& targetDir,
										 const std::atomic_bool& cancelled)
	{
		using Failure = JavaInstallOutcome::Failure;
		JavaInstallOutcome outcome;

		bool success = false;
		const bool zipArchive = archivePath.endsWith(".zip");
		if (zipArchive) {
			// Use libarchive for zip files; it polls the flag between entries
			MMCZip::ExtractReporting reporting;
			reporting.isCancelled = [&cancelled] { return cancelled.load(); };
			auto result = MMCZip::extractSubDir(archivePath, QString(),
												targetDir, reporting);
			success = result.has_value();
		} else if (archivePath.endsWith(".tar.gz") ||
				   archivePath.endsWith(".tgz")) {
			// Use system tar for tar.gz files
			success = runTar(archivePath, targetDir, cancelled);
		} else {
			if (!QFile::remove(archivePath))
				qWarning() << "Failed to remove archive:" << archivePath;
			outcome.failure = Failure::UnsupportedArchive;
			return outcome;
		}

		// Clean up archive file
		if (!QFile::remove(archivePath))
			qWarning() << "Failed to remove archive:" << archivePath;

		if (!success) {
			outcome.failure = Failure::Extract;
			return outcome;
		}

		// Fix permissions on extracted files
		QDirIterator it(targetDir, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			if (cancelled.load()) {
				return outcome;
			}
			auto filepath = it.next();
			QFileInfo file(filepath);
			auto permissions = QFile::permissions(filepath);
			if (file.isDir()) {
				permissions |= QFileDevice::ReadUser | QFileDevice::WriteUser |
							   QFileDevice::ExeUser;
			} else {
				permissions |= QFileDevice::ReadUser | QFileDevice::WriteUser;
			}
#ifndef Q_OS_WIN
			// A zip brings no Unix permissions along, and a Java runs more than
			// bin/java: Java 8 starts every child process -- the game, with Kite's
			// launcher part -- through lib/jspawnhelper.
			if (zipArchive && !file.isDir() &&
				(file.dir().dirName() == QLatin1String("bin") ||
				 file.fileName() == QLatin1String("jspawnhelper") ||
				 file.fileName() == QLatin1String("jexec"))) {
				permissions |= QFileDevice::ExeUser | QFileDevice::ExeGroup |
							   QFileDevice::ExeOther;
			}
#endif
			QFile::setPermissions(filepath, permissions);
		}

		// Find the java binary
		outcome.javaPath = findJava(targetDir);
		if (outcome.javaPath.isEmpty()) {
			outcome.failure = Failure::NoJava;
			return outcome;
		}

		// Make java binary executable
		auto perms = QFile::permissions(outcome.javaPath) |
					 QFileDevice::ExeUser | QFileDevice::ExeGroup |
					 QFileDevice::ExeOther;
		QFile::setPermissions(outcome.javaPath, perms);
		return outcome;
	}

	// Runs on a worker thread: touches no QObject.
	JavaInstallOutcome
	finishManifest(const QString& targetDir, const QStringList& executableFiles,
				   const QList<QPair<QString, QString>>& linkEntries,
				   const std::atomic_bool& cancelled)
	{
		using Failure = JavaInstallOutcome::Failure;
		JavaInstallOutcome outcome;

		// Set executable permissions
		for (const auto& path : executableFiles) {
			QFile::setPermissions(
				path, QFile::permissions(path) | QFileDevice::ExeUser |
						  QFileDevice::ExeGroup | QFileDevice::ExeOther);
		}

		// Create symlinks
		const QString root = QDir::cleanPath(QDir(targetDir).absolutePath());
		for (const auto& link : linkEntries) {
			if (cancelled.load()) {
				return outcome;
			}
			const QString parent = QFileInfo(link.first).absolutePath();
			// A link made earlier in one of the folders would redirect this one.
			for (QString dir = parent; dir.size() > root.size();
				 dir = QFileInfo(dir).absolutePath()) {
				if (QFileInfo(dir).isSymLink()) {
					outcome.failure = Failure::UnsafeLink;
					return outcome;
				}
			}
			QDir().mkpath(parent);
			QFile::link(link.second, link.first);
		}

		// Find java binary
		outcome.javaPath = findJava(targetDir);
		if (outcome.javaPath.isEmpty()) {
			outcome.failure = Failure::NoJava;
		}
		return outcome;
	}
} // namespace

JavaDownloadTask::JavaDownloadTask(const JavaDownload::RuntimeEntry& runtime,
								   const QString& targetDir, QObject* parent)
	: Task(parent), m_runtime(runtime), m_targetDir(targetDir)
{
}

void JavaDownloadTask::runInBackground(
	std::function<JavaInstallOutcome()> work,
	std::function<void(const JavaInstallOutcome&)> onDone)
{
	auto* watcher = new QFutureWatcher<JavaInstallOutcome>(this);
	connect(watcher, &QFutureWatcher<JavaInstallOutcome>::finished, this,
			[this, watcher, onDone = std::move(onDone)]() {
				const JavaInstallOutcome outcome = watcher->result();
				watcher->deleteLater();
				if (m_cancelled->load()) {
					abortAndClean();
					return;
				}
				onDone(outcome);
			});
	watcher->setFuture(
		QtConcurrent::run(QThreadPool::globalInstance(), std::move(work)));
}

// A cancelled install leaves a half-written runtime that the Java lists would
// show as installed: the task stays running until the folder is gone.
void JavaDownloadTask::abortAndClean()
{
	auto* watcher = new QFutureWatcher<void>(this);
	connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
		watcher->deleteLater();
		emitAborted();
	});
	const QString targetDir = m_targetDir;
	watcher->setFuture(QtConcurrent::run(
		QThreadPool::globalInstance(),
		[targetDir]() { QDir(targetDir).removeRecursively(); }));
}

bool JavaDownloadTask::abort()
{
	if (!isRunning()) {
		return false;
	}
	m_cancelled->store(true);
	// The job answers with failed(), which downloadFailed() turns into an abort.
	if (m_downloadJob) {
		m_downloadJob->abort();
	}
	return true;
}

void JavaDownloadTask::executeTask()
{
	m_cancelled = std::make_shared<std::atomic_bool>(false);
	if (m_targetDir.isEmpty()) {
		emitFailed(tr("Failed to create target directory: %1").arg(m_targetDir));
		return;
	}

	const QString targetDir = m_targetDir;
	runInBackground(
		[targetDir]() {
			if (QDir(targetDir).exists()) {
				QDir(targetDir).removeRecursively();
			}
			return JavaInstallOutcome();
		},
		[this](const JavaInstallOutcome&) {
			if (m_runtime.downloadType == "manifest") {
				downloadManifest();
			} else {
				downloadArchive();
			}
		});
}

void JavaDownloadTask::downloadArchive()
{
	setStatus(tr("Downloading %1...").arg(m_runtime.name));

	// Determine archive extension and path
	QUrl url(m_runtime.url);
	QString filename = url.fileName();
	if (!JavaDownload::isSafeSegment(filename)) {
		emitFailed(tr("Unsupported archive format: %1").arg(filename));
		return;
	}
	m_archivePath = FS::PathCombine(m_targetDir, filename);

	// Create target directory
	if (!FS::ensureFolderPathExists(m_targetDir)) {
		emitFailed(
			tr("Failed to create target directory: %1").arg(m_targetDir));
		return;
	}

	m_downloadJob = new NetJob(tr("Java download"), LAUNCHER->network());

	auto dl = Net::Download::makeFile(m_runtime.url, m_archivePath);

	// Add checksum validation
	if (!m_runtime.checksumHash.isEmpty()) {
		QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256;
		if (m_runtime.checksumType == "sha1")
			algo = QCryptographicHash::Sha1;
		auto validator = new Net::ChecksumValidator(
			algo, QByteArray::fromHex(m_runtime.checksumHash.toLatin1()));
		dl->addValidator(validator);
	}

	m_downloadJob->addNetAction(dl);

	connect(m_downloadJob.get(), &NetJob::succeeded, this,
			&JavaDownloadTask::downloadFinished);
	connect(m_downloadJob.get(), &NetJob::failed, this,
			&JavaDownloadTask::downloadFailed);
	connect(m_downloadJob.get(), &NetJob::progress, this, &Task::setProgress);
	// Show the archive being fetched as its own step.
	propagateStepsFrom(m_downloadJob.get());

	m_downloadJob->start();
}

void JavaDownloadTask::downloadFinished()
{
	m_downloadJob.reset();
	extractArchive();
}

void JavaDownloadTask::downloadFailed(QString reason)
{
	m_downloadJob.reset();
	// Clean up partial download
	if (!m_archivePath.isEmpty() && !QFile::remove(m_archivePath))
		qWarning() << "Failed to remove partial download:" << m_archivePath;
	if (m_cancelled->load()) {
		abortAndClean();
		return;
	}
	emitFailed(tr("Failed to download Java: %1").arg(reason));
}

void JavaDownloadTask::extractArchive()
{
	setStatus(tr("Extracting %1...").arg(m_runtime.name));

	const QString archivePath = m_archivePath;
	const QString targetDir = m_targetDir;
	const auto cancelled = m_cancelled;
	runInBackground(
		[archivePath, targetDir, cancelled]() {
			return extractAndPrepare(archivePath, targetDir, *cancelled);
		},
		[this](const JavaInstallOutcome& outcome) {
			archiveExtracted(outcome);
		});
}

void JavaDownloadTask::archiveExtracted(const JavaInstallOutcome& outcome)
{
	using Failure = JavaInstallOutcome::Failure;
	switch (outcome.failure) {
		case Failure::UnsupportedArchive:
			emitFailed(tr("Unsupported archive format: %1").arg(m_archivePath));
			return;
		case Failure::Extract:
			emitFailed(tr("Failed to extract Java archive."));
			return;
		case Failure::NoJava:
		case Failure::UnsafeLink:
			emitFailed(tr("Could not find java binary in extracted archive."));
			return;
		case Failure::None:
			break;
	}

	m_installedJavaPath = outcome.javaPath;
	qCDebug(javaLog) << "Java installed successfully at:" << m_installedJavaPath;
	emitSucceeded();
}

void JavaDownloadTask::downloadManifest()
{
	setStatus(tr("Downloading manifest for %1...").arg(m_runtime.name));

	if (!FS::ensureFolderPathExists(m_targetDir)) {
		emitFailed(
			tr("Failed to create target directory: %1").arg(m_targetDir));
		return;
	}

	m_downloadJob =
		new NetJob(tr("Java manifest download"), LAUNCHER->network());
	auto dl =
		Net::Download::makeByteArray(QUrl(m_runtime.url), &m_manifestData);

	if (m_runtime.checksumType == "sha1" && !m_runtime.checksumHash.isEmpty()) {
		dl->addValidator(new Net::ChecksumValidator(
			QCryptographicHash::Sha1,
			QByteArray::fromHex(m_runtime.checksumHash.toLatin1())));
	}

	m_downloadJob->addNetAction(dl);

	connect(m_downloadJob.get(), &NetJob::succeeded, this,
			&JavaDownloadTask::manifestDownloaded);
	connect(m_downloadJob.get(), &NetJob::failed, this,
			&JavaDownloadTask::downloadFailed);

	m_downloadJob->start();
}

void JavaDownloadTask::manifestDownloaded()
{
	m_downloadJob.reset();

	QJsonDocument doc;
	try {
		doc = Json::requireDocument(m_manifestData);
	} catch (const Exception& e) {
		m_manifestData.clear();
		emitFailed(tr("Failed to parse Java manifest: %1").arg(e.cause()));
		return;
	}
	m_manifestData.clear();

	if (!doc.isObject()) {
		emitFailed(tr("Failed to parse Java manifest."));
		return;
	}

	auto files = doc.object()["files"].toObject();
	m_executableFiles.clear();
	m_linkEntries.clear();

	// The keys and link targets come from a remote document: check them all
	// before anything is written, and insist on a checksum for every file.
	for (auto it = files.begin(); it != files.end(); ++it) {
		const auto entry = it.value().toObject();
		const QString type = entry["type"].toString();
		const QString path = JavaDownload::joinInside(m_targetDir, it.key());
		bool safe = !path.isEmpty();
		if (safe && type == "file") {
			const auto raw = entry["downloads"].toObject()["raw"].toObject();
			safe = !raw["url"].toString().isEmpty() &&
				   !raw["sha1"].toString().isEmpty();
		} else if (safe && type == "link") {
			safe = JavaDownload::isSafeLinkTarget(
				m_targetDir, path, entry["target"].toString());
		}
		if (!safe) {
			emitFailed(tr("The Java manifest is not safe to install."));
			return;
		}
	}

	// Create directories first
	for (auto it = files.begin(); it != files.end(); ++it) {
		auto entry = it.value().toObject();
		if (entry["type"].toString() == "directory") {
			QDir().mkpath(JavaDownload::joinInside(m_targetDir, it.key()));
		}
	}

	// Queue file downloads
	setStatus(tr("Downloading %1 files...").arg(m_runtime.name));
	m_downloadJob =
		new NetJob(tr("Java runtime files"), LAUNCHER->network());

	for (auto it = files.begin(); it != files.end(); ++it) {
		auto entry = it.value().toObject();
		const QString filePath = JavaDownload::joinInside(m_targetDir, it.key());

		if (entry["type"].toString() == "file") {
			auto downloads = entry["downloads"].toObject();
			auto raw = downloads["raw"].toObject();

			QString url = raw["url"].toString();
			QString sha1 = raw["sha1"].toString();

			// Ensure parent directory exists
			QFileInfo fi(filePath);
			QDir().mkpath(fi.absolutePath());

			auto dl = Net::Download::makeFile(QUrl(url), filePath);
			dl->addValidator(new Net::ChecksumValidator(
				QCryptographicHash::Sha1,
				QByteArray::fromHex(sha1.toLatin1())));
			m_downloadJob->addNetAction(dl);

			if (entry["executable"].toBool()) {
				m_executableFiles.append(filePath);
			}
		} else if (entry["type"].toString() == "link") {
			m_linkEntries.append({filePath, entry["target"].toString()});
		}
	}

	connect(m_downloadJob.get(), &NetJob::succeeded, this,
			&JavaDownloadTask::manifestFilesDownloaded);
	connect(m_downloadJob.get(), &NetJob::failed, this,
			&JavaDownloadTask::downloadFailed);
	connect(m_downloadJob.get(), &NetJob::progress, this, &Task::setProgress);
	// One line per file being downloaded.
	propagateStepsFrom(m_downloadJob.get());

	m_downloadJob->start();
}

void JavaDownloadTask::manifestFilesDownloaded()
{
	m_downloadJob.reset();

	const QString targetDir = m_targetDir;
	const QStringList executableFiles = m_executableFiles;
	const auto linkEntries = m_linkEntries;
	const auto cancelled = m_cancelled;
	runInBackground(
		[targetDir, executableFiles, linkEntries, cancelled]() {
			return finishManifest(targetDir, executableFiles, linkEntries,
								  *cancelled);
		},
		[this](const JavaInstallOutcome& outcome) {
			using Failure = JavaInstallOutcome::Failure;
			if (outcome.failure == Failure::UnsafeLink) {
				emitFailed(tr("The Java manifest is not safe to install."));
				return;
			}
			if (outcome.failure != Failure::None) {
				emitFailed(
					tr("Could not find java binary in downloaded runtime."));
				return;
			}
			m_installedJavaPath = outcome.javaPath;
			qCDebug(javaLog)
				<< "Java installed successfully at:" << m_installedJavaPath;
			emitSucceeded();
		});
}
