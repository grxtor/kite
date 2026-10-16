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

#include "models/JavaDownloadController.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>

#include "BuildConfig.h"
#include "FileSystem.h"
#include "core/LauncherContext.h"
#include "java/JavaUtils.h"
#include "models/JavaController.h"
#include "net/Download.h"
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
#include "java/download/JavaDownloadTask.h"
#endif

JavaDownloadController::JavaDownloadController(QObject* parent)
	: QObject(parent), m_vendors(JavaController::vendorList())
{
}

JavaDownloadController::~JavaDownloadController()
{
	stopFetch();
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
	if (m_task && m_task->isRunning()) {
		m_task->abort();
	}
#endif
}

QVariantList JavaDownloadController::vendors() const
{
	return m_vendors;
}
QString JavaDownloadController::vendor() const
{
	return m_vendor;
}
QVariantList JavaDownloadController::versions() const
{
	return m_versions;
}
QString JavaDownloadController::version() const
{
	return m_version;
}
QVariantList JavaDownloadController::builds() const
{
	return m_builds;
}
int JavaDownloadController::build() const
{
	return m_build;
}
bool JavaDownloadController::loading() const
{
	return m_loading;
}
bool JavaDownloadController::downloading() const
{
	return m_downloading;
}
double JavaDownloadController::progress() const
{
	return m_progress;
}
QString JavaDownloadController::status() const
{
	return m_status;
}
QString JavaDownloadController::error() const
{
	return m_error;
}
QString JavaDownloadController::installedPath() const
{
	return m_installedPath;
}

bool JavaDownloadController::alreadyInstalled() const
{
	if (m_build < 0 || m_build >= m_runtimes.size()) {
		return false;
	}
	return QDir(targetDirFor(m_runtimes[m_build])).exists();
}

QString JavaDownloadController::defaultVersion(const QVariantList& versions)
{
	for (const auto& row : versions) {
		if (row.toMap().value(QStringLiteral("recommended")).toBool()) {
			return row.toMap().value(QStringLiteral("value")).toString();
		}
	}
	return versions.isEmpty()
			   ? QString()
			   : versions.first().toMap().value(QStringLiteral("value"))
					 .toString();
}

QString JavaDownloadController::versionLabel(const QString& versionId)
{
	if (versionId.startsWith(QStringLiteral("java"))) {
		return QStringLiteral("Java ") + versionId.mid(4);
	}
	return versionId;
}

QVariantList JavaDownloadController::versionsFromJson(const QByteArray& json,
													  const QString& uid,
													  QString* error)
{
	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(json, &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		if (error) {
			*error = tr("The list of Java versions could not be read.");
		}
		return {};
	}
	QVariantList list;
	const auto infos = JavaDownload::parseVersionIndex(doc.object(), uid);
	for (const auto& info : infos) {
		list.append(QVariantMap{
			{QStringLiteral("value"), info.versionId},
			{QStringLiteral("label"), versionLabel(info.versionId)},
			{QStringLiteral("recommended"), info.recommended}});
	}
	return list;
}

QList<JavaDownload::RuntimeEntry>
JavaDownloadController::runtimesFromJson(const QByteArray& json,
										 const QString& runtimeOS,
										 QString* error)
{
	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(json, &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		if (error) {
			*error = tr("The list of Java builds could not be read.");
		}
		return {};
	}
	QList<JavaDownload::RuntimeEntry> matching;
	const auto all = JavaDownload::parseRuntimes(doc.object());
	for (const auto& runtime : all) {
		if (runtime.runtimeOS == runtimeOS) {
			matching.append(runtime);
		}
	}
	return matching;
}

QString JavaDownloadController::targetDirFor(
	const JavaDownload::RuntimeEntry& runtime)
{
	const QString dirName =
		QStringLiteral("%1-%2").arg(runtime.name, runtime.version.toString());
	return FS::PathCombine(JavaUtils::managedJavaRoot(), runtime.vendor,
						   dirName);
}

void JavaDownloadController::setLoading(bool loading)
{
	if (m_loading != loading) {
		m_loading = loading;
		emit loadingChanged();
	}
}

void JavaDownloadController::setError(const QString& error)
{
	if (m_error != error) {
		m_error = error;
		emit errorChanged();
	}
}

void JavaDownloadController::stopFetch()
{
	// Detach first: aborting makes the job say "failed", and that must not
	// look like a failure of the pick that replaced it.
	if (auto job = std::move(m_fetchJob)) {
		m_fetchJob.reset();
		job->abort();
	}
	m_fetchData.reset();
	setLoading(false);
}

void JavaDownloadController::fetch(
	const QString& relativeUrl, std::function<void(const QByteArray&)> onData)
{
	stopFetch();
	setError(QString());
	if (!LAUNCHER) {
		setError(tr("Downloads are not available right now."));
		return;
	}
	auto data = std::make_shared<QByteArray>();
	m_fetchData = data;
	auto job = NetJob::Ptr(new NetJob(tr("Fetch Java versions"),
									  LAUNCHER->network()));
	job->addNetAction(Net::Download::makeByteArray(
		QUrl(BuildConfig.META_URL + relativeUrl), data.get()));
	m_fetchJob = job;
	setLoading(true);
	// Raw pointer, not the shared one: the job owns these connections, so
	// capturing it by value would keep it alive forever.
	const NetJob* jobId = job.get();

	// A newer fetch replaces m_fetchJob; an answer from an older one is
	// dropped instead of overwriting the newer pick.
	connect(job.get(), &NetJob::succeeded, this,
			[this, jobId, data, onData]() {
				if (m_fetchJob.get() != jobId) {
					return;
				}
				m_fetchJob.reset();
				setLoading(false);
				onData(*data);
			});
	connect(job.get(), &NetJob::failed, this, [this, jobId](QString reason) {
		if (m_fetchJob.get() != jobId) {
			return;
		}
		m_fetchJob.reset();
		setLoading(false);
		setError(tr("Could not load the list: %1").arg(reason));
	});
	job->start();
}

void JavaDownloadController::reset()
{
	stopFetch();
	m_versionInfos.clear();
	m_versions.clear();
	m_version.clear();
	m_runtimes.clear();
	m_builds.clear();
	m_build = -1;
	setError(QString());
	emit versionsChanged();
	emit versionChanged();
	emit buildsChanged();
	emit buildChanged();
	if (!m_downloading) {
		m_progress = 0;
		m_status.clear();
		emit progressChanged();
		emit statusChanged();
	}
	if (!m_vendors.isEmpty()) {
		selectVendor(m_vendors.first().toMap().value("value").toString());
	}
}

void JavaDownloadController::selectVendor(const QString& uid)
{
	if (m_downloading) {
		return;
	}
	m_vendor = uid;
	m_versions.clear();
	m_version.clear();
	m_runtimes.clear();
	m_builds.clear();
	m_build = -1;
	emit vendorChanged();
	emit versionsChanged();
	emit versionChanged();
	emit buildsChanged();
	emit buildChanged();

	fetch(uid + QStringLiteral("/index.json"), [this, uid](const QByteArray& data) {
		QString parseError;
		m_versions = versionsFromJson(data, uid, &parseError);
		setError(parseError);
		emit versionsChanged();
		if (parseError.isEmpty() && m_versions.isEmpty()) {
			setError(tr("This vendor has no Java versions to download."));
		}
		// Start from the best choice instead of an empty picker.
		const QString preselected = defaultVersion(m_versions);
		if (!preselected.isEmpty()) {
			selectVersion(preselected);
		}
	});
}

void JavaDownloadController::selectVersion(const QString& versionId)
{
	if (m_downloading || m_vendor.isEmpty()) {
		return;
	}
	m_version = versionId;
	m_runtimes.clear();
	m_builds.clear();
	m_build = -1;
	emit versionChanged();
	emit buildsChanged();
	emit buildChanged();

	const QString uid = m_vendor;
	fetch(uid + QStringLiteral("/") + versionId + QStringLiteral(".json"),
		  [this](const QByteArray& data) {
			  QString parseError;
			  m_runtimes = runtimesFromJson(
				  data, JavaDownload::currentRuntimeOS(), &parseError);
			  m_builds.clear();
			  for (const auto& runtime : m_runtimes) {
				  m_builds.append(QVariantMap{
					  {QStringLiteral("label"), runtime.version.toString()},
					  {QStringLiteral("name"), runtime.name},
					  {QStringLiteral("version"), runtime.version.toString()},
					  {QStringLiteral("checksum"), runtime.checksumType}});
			  }
			  setError(parseError);
			  emit buildsChanged();
			  if (parseError.isEmpty() && m_runtimes.isEmpty()) {
				  setError(tr("There is no build of this Java for your "
							  "computer."));
			  } else if (!m_runtimes.isEmpty()) {
				  selectBuild(0);
			  }
		  });
}

void JavaDownloadController::selectBuild(int row)
{
	if (m_downloading) {
		return;
	}
	m_build = row >= 0 && row < m_runtimes.size() ? row : -1;
	emit buildChanged();
}

void JavaDownloadController::download()
{
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
	if (m_downloading || m_build < 0 || m_build >= m_runtimes.size()) {
		return;
	}
	const auto runtime = m_runtimes[m_build];
	const QString targetDir = targetDirFor(runtime);
	if (QDir(targetDir).exists()) {
		QDir(targetDir).removeRecursively();
	}

	setError(QString());
	m_installedPath.clear();
	m_progress = 0;
	m_status = tr("Starting");
	m_downloading = true;
	emit progressChanged();
	emit statusChanged();
	emit downloadingChanged();

	m_task = std::make_unique<JavaDownloadTask>(runtime, targetDir);
	auto* task = m_task.get();
	connect(task, &Task::progress, this, [this](qint64 current, qint64 total) {
		if (total > 0) {
			m_progress = double(current) / double(total);
			emit progressChanged();
		}
	});
	connect(task, &Task::status, this, [this](const QString& status) {
		m_status = status;
		emit statusChanged();
	});
	connect(task, &Task::succeeded, this, [this, task]() {
		m_installedPath = task->installedJavaPath();
		m_progress = 1;
		emit progressChanged();
		emit installed();
	});
	connect(task, &Task::failed, this, [this](const QString& reason) {
		setError(tr("Download failed: %1").arg(reason));
	});
	connect(task, &Task::finished, this, [this]() {
		m_downloading = false;
		emit downloadingChanged();
	});
	task->start();
#endif
}

void JavaDownloadController::cancel()
{
	stopFetch();
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
	if (m_task && m_task->isRunning()) {
		m_task->abort();
	}
#endif
}
