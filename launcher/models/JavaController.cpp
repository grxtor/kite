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

#include "models/JavaController.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <algorithm>

#include "FileSystem.h"
#include "core/LauncherContext.h"
#include "java/JavaArgs.h"
#include "java/JavaInstallList.h"
#include "java/JavaUtils.h"
#include "java/JavaVersion.h"
#include "java/download/JavaRuntime.h"

namespace
{
	QString javaBinaryName()
	{
#if defined(Q_OS_WIN)
		return QStringLiteral("javaw.exe");
#else
		return QStringLiteral("java");
#endif
	}

	/* The first <anything>/bin/java under @p versionDir, or an empty string.
	 * Archives differ in depth (macOS puts it in Contents/Home/bin), so look
	 * through the whole folder, as JavaPage::refreshInstalledJavas() did. */
	QString findJavaBinary(const QString& versionDir)
	{
		QDirIterator it(versionDir, QStringList() << javaBinaryName(),
						QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			if (it.filePath().contains(QStringLiteral("/bin/"))) {
				return it.filePath();
			}
		}
		return QString();
	}
} // namespace

JavaController::JavaController(QObject* parent) : QObject(parent)
{
	refreshManaged();
}

JavaController::~JavaController() = default;

QVariantList JavaController::vendors() const
{
	return vendorList();
}

QVariantList JavaController::vendorList()
{
	QVariantList list;
	const auto providers =
		JavaDownload::JavaProviderInfo::availableProviders();
	for (const auto& provider : providers) {
		list.append(QVariantMap{{QStringLiteral("value"), provider.uid},
								{QStringLiteral("label"), provider.name}});
	}
	return list;
}

bool JavaController::downloaderAvailable() const
{
#ifdef MeshMC_DISABLE_JAVA_DOWNLOADER
	return false;
#else
	return true;
#endif
}

QObject* JavaController::installs() const
{
	if (!LAUNCHER) {
		return nullptr;
	}
	return LAUNCHER->javalist().get();
}

bool JavaController::detecting() const
{
	return m_detecting;
}

QString JavaController::testingToken() const
{
	return m_testingToken;
}

QVariantList JavaController::managed() const
{
	return m_managed;
}

QString JavaController::managedRoot() const
{
	return JavaUtils::managedJavaRoot();
}

void JavaController::detect()
{
	if (!LAUNCHER) {
		return;
	}
	auto task = LAUNCHER->javalist()->getLoadTask();
	if (!task) {
		return;
	}
	if (!m_detecting) {
		m_detecting = true;
		emit detectingChanged();
	}
	connect(task.get(), &Task::finished, this, [this]() {
		m_detecting = false;
		emit detectingChanged();
	});
	if (!task->isRunning()) {
		task->start();
	}
}

QString JavaController::argsWarning(const QString& args) const
{
	if (!JavaArgs::setsMemoryFlags(args)) {
		return QString();
	}
	return tr("Memory flags such as -Xmx and -Xms belong in the Memory "
			  "settings above. Remove them from these arguments.");
}

bool JavaController::requiresPermGen(const QString& version) const
{
	if (version.trimmed().isEmpty()) {
		return false;
	}
	JavaVersion parsed(version);
	return parsed.major() > 0 && parsed.requiresPermGen();
}

QString JavaController::pickedJava(const QUrl& url) const
{
	const QString raw = url.toLocalFile();
	if (raw.isEmpty()) {
		return QString();
	}
	const QString cooked = FS::NormalizePath(raw);
	const QFileInfo info(cooked);
	if (!info.exists() || !info.isFile() || !info.isExecutable()) {
		return QString();
	}
	return cooked;
}

QVariantMap JavaController::describeCheck(const JavaCheckResult& result)
{
	QVariantMap map;
	map[QStringLiteral("path")] = result.path;
	const bool ok = result.validity == JavaCheckResult::Validity::Valid;
	map[QStringLiteral("ok")] = ok;
	map[QStringLiteral("error")] = QString();
	map[QStringLiteral("version")] = result.javaVersion.toString();
	map[QStringLiteral("architecture")] = result.realPlatform;
	map[QStringLiteral("vendor")] = result.javaVendor;
	map[QStringLiteral("warnings")] =
		ok ? result.errorLog.trimmed() : QString();
	map[QStringLiteral("requiresPermGen")] =
		ok && result.javaVersion.requiresPermGen();
	return map;
}

void JavaController::test(const QString& token, const QString& path,
						  const QString& args, int minMem, int maxMem,
						  int permGen)
{
	if (!m_testingToken.isEmpty()) {
		return;
	}
	m_testingToken = token;
	m_args = args;
	m_minMem = minMem;
	m_maxMem = maxMem;
	m_permGen = permGen;
	emit testingChanged();

	if (path.trimmed().isEmpty() || !LAUNCHER) {
		QVariantMap map;
		map[QStringLiteral("path")] = path;
		map[QStringLiteral("ok")] = false;
		map[QStringLiteral("error")] =
			tr("Choose a Java executable first.");
		finishTest(map);
		return;
	}

	m_checker = std::make_shared<JavaChecker>();
	connect(m_checker.get(), &JavaChecker::checkFinished, this,
			[this](JavaCheckResult result) {
				if (result.validity != JavaCheckResult::Validity::Valid) {
					QVariantMap map = describeCheck(result);
					map[QStringLiteral("error")] =
						tr("This file is not a working Java.");
					finishTest(map);
					return;
				}
				startSecondStage(result);
			});
	m_checker->m_path = path;
	m_checker->performCheck();
}

void JavaController::startSecondStage(const JavaCheckResult& first)
{
	// The checker that just answered may still be on the stack: hand it to
	// the event loop to delete instead of dropping it from its own signal.
	auto previous = m_checker;
	QMetaObject::invokeMethod(
		this, [previous]() {}, Qt::QueuedConnection);

	m_checker = std::make_shared<JavaChecker>();
	connect(m_checker.get(), &JavaChecker::checkFinished, this,
			[this](JavaCheckResult result) {
				QVariantMap map = describeCheck(result);
				if (result.validity != JavaCheckResult::Validity::Valid) {
					map[QStringLiteral("error")] =
						tr("This Java does not start with your JVM "
						   "arguments.\n%1")
							.arg(result.errorLog.trimmed());
				}
				finishTest(map);
			});
	m_checker->m_path = first.path;
	m_checker->m_args = m_args;
	m_checker->m_minMem = m_minMem;
	m_checker->m_maxMem = m_maxMem;
	if (first.javaVersion.requiresPermGen()) {
		m_checker->m_permGen = m_permGen;
	}
	m_checker->performCheck();
}

void JavaController::finishTest(const QVariantMap& result)
{
	const QString token = m_testingToken;
	auto previous = m_checker;
	m_checker.reset();
	QMetaObject::invokeMethod(
		this, [previous]() {}, Qt::QueuedConnection);
	m_testingToken.clear();
	emit testingChanged();
	emit testFinished(token, result);
}

QVariantList JavaController::scanManaged(const QString& rootDir)
{
	struct Row {
		QString vendor;
		QString name;
		QString javaPath;
		QString root;
	};
	std::vector<Row> rows;

	QDirIterator vendors(rootDir, QDir::Dirs | QDir::NoDotAndDotDot);
	while (vendors.hasNext()) {
		vendors.next();
		QDirIterator versions(vendors.filePath(),
							  QDir::Dirs | QDir::NoDotAndDotDot);
		while (versions.hasNext()) {
			versions.next();
			const QString javaPath = findJavaBinary(versions.filePath());
			if (javaPath.isEmpty()) {
				continue;
			}
			rows.push_back({vendors.fileName(), versions.fileName(), javaPath,
							versions.filePath()});
		}
	}
	std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
		if (a.vendor != b.vendor) {
			return a.vendor.localeAwareCompare(b.vendor) < 0;
		}
		return a.name.localeAwareCompare(b.name) > 0;
	});

	QVariantList list;
	for (const auto& row : rows) {
		list.append(QVariantMap{{QStringLiteral("name"), row.name},
								{QStringLiteral("vendor"), row.vendor},
								{QStringLiteral("javaPath"), row.javaPath},
								{QStringLiteral("root"), row.root}});
	}
	return list;
}

void JavaController::refreshManaged()
{
	m_managed = scanManaged(managedRoot());
	emit managedChanged();
}

QString JavaController::removeManagedUnder(const QString& rootDir,
										   const QString& root)
{
	const QFileInfo target(root);
	if (root.isEmpty() || !target.exists() || !target.isDir() ||
		target.isSymLink()) {
		return tr("That Java is already gone.");
	}
	// Exactly <rootDir>/<vendor>/<name>: nothing above or below, and
	// nothing reached through a link.
	QDir vendorDir(target.canonicalFilePath());
	if (!vendorDir.cdUp()) {
		return tr("MeshMC can only remove Java it downloaded itself.");
	}
	QDir baseDir(vendorDir.absolutePath());
	if (!baseDir.cdUp() ||
		baseDir.absolutePath() !=
			QFileInfo(rootDir).canonicalFilePath()) {
		return tr("MeshMC can only remove Java it downloaded itself.");
	}
	if (!QDir(target.canonicalFilePath()).removeRecursively()) {
		return tr("Some files could not be removed. Close anything using "
				  "this Java and try again.");
	}
	return QString();
}

QString JavaController::removeManaged(const QString& root)
{
	const QString error = removeManagedUnder(managedRoot(), root);
	refreshManaged();
	return error;
}
