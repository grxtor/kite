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

#include "modplatform/modrinth/ModrinthPackExportTask.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QUrl>
#include <QtConcurrentRun>

#include <memory>
#include <utility>

#include "Json.h"
#include "archive/ExportToZipTask.h"
#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/Download.h"
#include "net/NetJob.h"

namespace
{
	/* Folders an mrpack manifest may name files in; anything else travels
	 * in `overrides/`. */
	const QStringList EXPORTABLE_PREFIXES = {
		QStringLiteral("mods/"), QStringLiteral("coremods/"),
		QStringLiteral("resourcepacks/"), QStringLiteral("texturepacks/"),
		QStringLiteral("shaderpacks/")};

	/* Only these are worth hashing: the catalogue has no version for a
	 * README or a stray .bak. */
	const QStringList EXPORTABLE_EXTENSIONS = {QStringLiteral("jar"),
											   QStringLiteral("litemod"),
											   QStringLiteral("zip")};

	constexpr qint64 HASH_BLOCK_SIZE = 64 * 1024;

	struct Digests {
		QString sha1;
		QString sha512;
		qint64 size = 0;
		bool ok = false;
	};

	/* Both mrpack digests in one pass over the file. Always computed from
	 * the bytes on disk: a sidecar hash goes stale once the file is touched,
	 * and the installer would see a corrupt download. */
	Digests digestsOf(const QString& path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			qWarning() << "Could not open" << path
					   << "to hash it:" << file.errorString();
			return {};
		}

		QCryptographicHash sha1(QCryptographicHash::Sha1);
		QCryptographicHash sha512(QCryptographicHash::Sha512);

		Digests digests;
		while (!file.atEnd()) {
			const QByteArray block = file.read(HASH_BLOCK_SIZE);
			if (block.isEmpty()) {
				break;
			}
			sha1.addData(block);
			sha512.addData(block);
			digests.size += block.size();
		}

		if (file.error() != QFileDevice::NoError) {
			qWarning() << "Could not read" << path
					   << "to hash it:" << file.errorString();
			return {};
		}

		digests.sha1 = QString::fromLatin1(sha1.result().toHex());
		digests.sha512 = QString::fromLatin1(sha512.result().toHex());
		digests.ok = true;
		return digests;
	}

	bool isExportCandidate(const QString& relative)
	{
		bool underPrefix = false;
		for (const QString& prefix : EXPORTABLE_PREFIXES) {
			if (relative.startsWith(prefix)) {
				underPrefix = true;
				break;
			}
		}
		if (!underPrefix) {
			return false;
		}

		/* `.disabled` is accepted too: a disabled mod can be named and marked
		 * optional. */
		for (const QString& extension : EXPORTABLE_EXTENSIONS) {
			if (relative.endsWith(QLatin1Char('.') + extension) ||
				relative.endsWith(QLatin1Char('.') + extension +
								  QStringLiteral(".disabled"))) {
				return true;
			}
		}
		return false;
	}

	/* Whether a recorded download URL is one an mrpack may name. */
	bool isNameableUrl(const QString& url)
	{
		if (url.isEmpty()) {
			return false;
		}
		return ModrinthApi::isMrpackHost(QUrl(url));
	}
} // namespace

ModrinthPackExportTask::ModrinthPackExportTask(
	QString name, QString version, QString summary, bool optionalFiles,
	MinecraftInstance* instance, QString output,
	MMCZip::FilterFileFunction filter, QObject* parent)
	: Task(parent), m_name(std::move(name)), m_version(std::move(version)),
	  m_summary(std::move(summary)), m_optionalFiles(optionalFiles),
	  m_instance(instance), m_gameRoot(instance->gameRoot()),
	  m_output(std::move(output)), m_filter(std::move(filter))
{
}

void ModrinthPackExportTask::executeTask()
{
	setStatus(tr("Searching for files..."));
	/* Indeterminate until the scan knows the file count. */
	setProgress(0, 0);

	connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished, this,
			&ModrinthPackExportTask::onScanFinished);
	m_scanFuture = QtConcurrent::run([this] { return scanFiles(); });
	m_scanWatcher.setFuture(m_scanFuture);
}

bool ModrinthPackExportTask::abort()
{
	if (m_aborted.exchange(true)) {
		/* Already aborting; a second verdict would be a second finished(). */
		return true;
	}

	/* Copied and cleared first: aborting a job can settle it synchronously
	 * and its handler removes itself from this list. */
	const QList<QPointer<NetJob>> jobs = m_activeJobs;
	m_activeJobs.clear();
	for (const QPointer<NetJob>& job : jobs) {
		if (job) {
			job->abort();
		}
	}

	if (m_zipTask) {
		/* The zip stops cleanly on its own and removes its half-written
		 * archive, which must not be raced. */
		m_zipTask->abort();
		return true;
	}

	if (m_scanWatcher.isRunning()) {
		/* The scan checks the flag between files; onScanFinished() emits, so
		 * emitting here would be a second verdict. */
		return true;
	}

	emitAborted();
	return true;
}

ModrinthPackExportTask::ScanResult ModrinthPackExportTask::scanFiles() const
{
	ScanResult result;

	QFileInfoList files;
	if (!MMCZip::collectFileListRecursively(m_gameRoot.absolutePath(),
										   QString(), &files, m_filter)) {
		/* ok stays false: a partial list would export an instance with
		 * files missing. */
		return result;
	}
	result.files = files;

	/* One sidecar index per folder, loaded once. */
	QHash<QString, std::shared_ptr<ModMetadataIndex>> indices;

	for (const QFileInfo& file : files) {
		if (m_aborted.load()) {
			return {};
		}

		const QString relative =
			m_gameRoot.relativeFilePath(file.absoluteFilePath());
		if (!isExportCandidate(relative)) {
			continue;
		}

		const Digests digests = digestsOf(file.absoluteFilePath());
		if (!digests.ok) {
			/* Unreadable, so it cannot be named, but it still belongs in the
			 * pack: left out of both maps, it lands in `overrides/`. */
			continue;
		}

		const QString folder = file.absolutePath();
		auto index = indices.value(folder);
		if (!index) {
			index = std::make_shared<ModMetadataIndex>(QDir(folder));
			index->load();
			indices.insert(folder, index);
		}

		const ModMetadataIndex::Entry entry = index->get(file.fileName());
		if (entry.isValid() && isNameableUrl(entry.downloadUrl)) {
			ResolvedFile resolved;
			resolved.url = entry.downloadUrl;
			resolved.sha1 = digests.sha1;
			resolved.sha512 = digests.sha512;
			resolved.size = digests.size;
			resolved.side = entry.side;
			result.resolved.insert(relative, resolved);
			continue;
		}

		PendingFile pending;
		pending.path = relative;
		pending.sha1 = digests.sha1;
		pending.sha512 = digests.sha512;
		pending.size = digests.size;
		result.pending.append(pending);
	}

	result.ok = true;
	return result;
}

void ModrinthPackExportTask::onScanFinished()
{
	if (m_aborted.load()) {
		emitAborted();
		return;
	}

	const ScanResult result = m_scanFuture.result();
	if (!result.ok) {
		emitFailed(tr("Could not read the instance's files."));
		return;
	}

	m_files = result.files;
	m_resolved = result.resolved;
	m_pending = result.pending;

	if (m_pending.isEmpty()) {
		buildZip();
		return;
	}

	lookUpPendingFiles();
}

void ModrinthPackExportTask::lookUpPendingFiles()
{
	setStatus(tr("Looking up files on Modrinth..."));

	m_lookupsTotal = m_pending.size();
	m_lookupsDone = 0;
	m_lookupsOutstanding = m_lookupsTotal;
	setProgress(0, m_lookupsTotal);

	/* All lookups start now; the network layer queues what it cannot send
	 * at once. */
	for (const PendingFile& pending : m_pending) {
		auto response = std::make_shared<QByteArray>();
		auto* job = new NetJob(QString("MR::ExportLookup(%1)").arg(pending.path),
							   LAUNCHER->network());
		job->addNetAction(Net::Download::makeByteArray(
			ModrinthApi::versionByHashUrl(pending.sha1), response.get()));

		QPointer<NetJob> tracked(job);
		m_activeJobs.append(tracked);

		const PendingFile file = pending;
		auto settle = [this, tracked, file](const QByteArray& bytes) {
			m_activeJobs.removeAll(tracked);
			if (tracked) {
				tracked->deleteLater();
			}

			if (m_aborted.load()) {
				/* abort() has reported or will; no second verdict. */
				return;
			}

			if (!bytes.isEmpty()) {
				recordLookupResult(file, bytes);
			}
			onOneLookupDone();
		};

		connect(job, &NetJob::succeeded, this,
				[settle, response] { settle(*response); });
		connect(job, &NetJob::failed, this,
				[settle, response](const QString& reason) {
					/* Not an error: a mod that is not on Modrinth travels inside the
					 * pack. Settled with an empty reply so the count stays right. */
					qDebug() << "Modrinth export lookup failed:" << reason;
					settle(QByteArray());
				});

		job->start();
	}
}

void ModrinthPackExportTask::recordLookupResult(const PendingFile& pending,
												const QByteArray& bytes)
{
	QString parseError;
	const auto doc = Json::tryParse(bytes, nullptr, &parseError);
	if (!doc) {
		qWarning() << "Could not parse the Modrinth version for"
				   << pending.path << ":" << parseError;
		return;
	}

	const QJsonArray files = doc->object().value("files").toArray();
	for (const QJsonValue& value : files) {
		const QJsonObject file = value.toObject();
		const QJsonObject hashes = file.value("hashes").toObject();

		/* The version owns several files (mod, sources, javadoc); pick the
		 * one on disk by our digest, not by the `primary` flag. */
		if (hashes.value("sha1").toString().compare(
				pending.sha1, Qt::CaseInsensitive) != 0) {
			continue;
		}

		const QString url = file.value("url").toString();
		if (!isNameableUrl(url)) {
			/* Served from somewhere we would not install from: ships in
			 * `overrides/`. */
			return;
		}

		ResolvedFile resolved;
		resolved.url = url;
		resolved.sha1 = pending.sha1;
		resolved.sha512 = pending.sha512;
		resolved.size = pending.size;
		/* No side: a version says nothing about the environment (that lives
		 * on the project), and guessing "both" would pass as known. */
		m_resolved.insert(pending.path, resolved);
		return;
	}
}

void ModrinthPackExportTask::onOneLookupDone()
{
	m_lookupsDone++;
	m_lookupsOutstanding--;
	setProgress(m_lookupsDone, m_lookupsTotal);

	if (m_lookupsOutstanding > 0) {
		return;
	}

	m_pending.clear();
	buildZip();
}

void ModrinthPackExportTask::buildZip()
{
	setStatus(tr("Adding files..."));

	auto* zip = new MMCZip::ExportToZipTask(m_output, m_gameRoot, m_files,
										   QStringLiteral("overrides/"), true);
	zip->addExtraFile(QStringLiteral("modrinth.index.json"), generateIndex());
	/* The manifest's files are downloaded on install; carrying them too
	 * would double the pack. */
	zip->setExcludeFiles(m_resolved.keys());

	m_zipTask.reset(zip);

	/* A stopped Task reports a failure, so the flag latched in abort()
	 * tells abort from failure. Consulted on both paths: the archive may
	 * succeed just before the click. */
	connect(zip, &Task::succeeded, this, [this] {
		m_zipTask.reset();
		if (m_aborted.load()) {
			emitAborted();
			return;
		}
		emitSucceeded();
	});
	connect(zip, &Task::failed, this, [this](const QString& reason) {
		m_zipTask.reset();
		if (m_aborted.load()) {
			emitAborted();
			return;
		}
		emitFailed(reason);
	});

	connect(zip, &Task::progress, this, &Task::setProgress);
	connect(zip, &Task::status, this, &Task::setStatus);
	propagateStepsFrom(zip);

	zip->start();
}

QByteArray ModrinthPackExportTask::generateIndex() const
{
	QJsonObject index;
	index["formatVersion"] = 1;
	index["game"] = "minecraft";
	index["name"] = m_name;
	index["versionId"] = m_version;
	/* Omitted rather than empty: an empty summary would show as the
	 * description. */
	if (!m_summary.isEmpty()) {
		index["summary"] = m_summary;
	}

	if (m_instance) {
		auto profile = m_instance->getPackProfile();

		/* The loaders an mrpack can express. A missing loader contributes
		 * nothing: an empty version would read as "needs Forge". */
		const QList<QPair<QString, QString>> dependencyUids = {
			{QStringLiteral("minecraft"), QStringLiteral("net.minecraft")},
			{QStringLiteral("quilt-loader"),
			 QStringLiteral("org.quiltmc.quilt-loader")},
			{QStringLiteral("fabric-loader"),
			 QStringLiteral("net.fabricmc.fabric-loader")},
			{QStringLiteral("forge"), QStringLiteral("net.minecraftforge")},
			{QStringLiteral("neoforge"), QStringLiteral("net.neoforged")}};

		QJsonObject dependencies;
		for (const auto& dependency : dependencyUids) {
			const QString version =
				profile->getComponentVersion(dependency.second);
			if (!version.isEmpty()) {
				dependencies[dependency.first] = version;
			}
		}
		index["dependencies"] = dependencies;
	}

	QJsonArray files;
	for (auto it = m_resolved.constBegin(); it != m_resolved.constEnd(); it++) {
		const ResolvedFile& resolved = it.value();

		QString path = it.key();
		QJsonObject env;

		const QFileInfo pathInfo(path);
		if (m_optionalFiles && pathInfo.suffix() == QStringLiteral("disabled")) {
			/* Named under the enabled name and marked optional: `.disabled` is
			 * this launcher's off switch and no loader reads such a file. */
			path = pathInfo.dir().filePath(pathInfo.completeBaseName());
			env["client"] = "optional";
			env["server"] = "optional";
		} else {
			env["client"] = "required";
			env["server"] = "required";
		}

		/* Only the server side is narrowed: a client-only mod is still needed
		 * by a client, and server-only files are skipped on client installs. */
		if (resolved.side == QStringLiteral("client")) {
			env["server"] = "unsupported";
		}

		QJsonObject hashes;
		hashes["sha1"] = resolved.sha1;
		hashes["sha512"] = resolved.sha512;

		QJsonObject file;
		file["path"] = path;
		file["env"] = env;
		file["downloads"] = QJsonArray{resolved.url};
		file["hashes"] = hashes;
		file["fileSize"] = resolved.size;
		files << file;
	}
	index["files"] = files;

	return QJsonDocument(index).toJson(QJsonDocument::Compact);
}
