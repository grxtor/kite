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

#include "modplatform/modrinth/ModrinthPackExportTask.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPair>
#include <QUrl>
#include <QtConcurrentRun>

#include <memory>
#include <utility>

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
	/* The folders an mrpack manifest is allowed to name files in. A pack
	 * that lists anything else is one our own importer would refuse, so
	 * files outside these travel inside `overrides/` regardless of
	 * whether Modrinth happens to know them. */
	const QStringList EXPORTABLE_PREFIXES = {
		QStringLiteral("mods/"), QStringLiteral("coremods/"),
		QStringLiteral("resourcepacks/"), QStringLiteral("texturepacks/"),
		QStringLiteral("shaderpacks/")};

	/* Only these are worth hashing. Everything else in those folders -
	 * a README, a leftover .bak, a config the user dropped next to a
	 * mod - is not a thing the catalogue has a version for. */
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

	/*
	 * Both digests an mrpack entry carries, in one pass over the file.
	 *
	 * The manifest names sha1 and sha512 together, and the file has to
	 * be read to produce either, so reading it twice to produce one each
	 * would double the cost of the slowest part of an export.
	 *
	 * These are always computed from the bytes on disk, never taken from
	 * a sidecar that claims them: a sidecar records what was downloaded,
	 * and a file that has been touched since would ship with a hash that
	 * no longer describes it - which turns into a corruption error for
	 * whoever installs the pack, blamed on their download.
	 */
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

		/* `.disabled` is accepted alongside the bare suffix: a disabled
		 * mod is still a mod the manifest can name, and marking it
		 * optional is exactly what the optional-files option is for. */
		for (const QString& extension : EXPORTABLE_EXTENSIONS) {
			if (relative.endsWith(QLatin1Char('.') + extension) ||
				relative.endsWith(QLatin1Char('.') + extension +
								  QStringLiteral(".disabled"))) {
				return true;
			}
		}
		return false;
	}

	/* Whether a recorded download URL is one an mrpack may name, and
	 * therefore one we are willing to publish. See the header. */
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
	/* Indeterminate: the file count is the first thing the scan finds
	 * out, so until it comes back there is no total to show. */
	setProgress(0, 0);

	connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished, this,
			&ModrinthPackExportTask::onScanFinished);
	m_scanFuture = QtConcurrent::run([this] { return scanFiles(); });
	m_scanWatcher.setFuture(m_scanFuture);
}

bool ModrinthPackExportTask::abort()
{
	if (m_aborted.exchange(true)) {
		/* Already aborting. The verdict is on its way from whichever
		 * phase latched it first; a second one would be a second
		 * finished() for the same task. */
		return true;
	}

	/* Copied out and cleared before any of them is touched: aborting a
	 * job can settle it synchronously, and its handler removes itself
	 * from this very list. */
	const QList<QPointer<NetJob>> jobs = m_activeJobs;
	m_activeJobs.clear();
	for (const QPointer<NetJob>& job : jobs) {
		if (job) {
			job->abort();
		}
	}

	if (m_zipTask) {
		/* The zip is the only phase that can stop cleanly on its own and
		 * report it, so let it: it also has a half-written archive to
		 * remove, which we must not race. */
		m_zipTask->abort();
		return true;
	}

	if (m_scanWatcher.isRunning()) {
		/* The scan cannot be interrupted inside a file, but it checks
		 * the flag between them. onScanFinished() sees it and emits, so
		 * emitting here as well would be the second verdict. */
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
		 * files silently missing from it. */
		return result;
	}
	result.files = files;

	/* One sidecar index per folder that turned out to hold candidates,
	 * loaded once. Reading `mods/.index` again for every jar in `mods/`
	 * would be the dominant cost of the scan for a large pack. */
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
			/* Unreadable, so it cannot be named - but it is still the
			 * user's file and still belongs in the pack. Left out of
			 * both maps, which is what puts it in `overrides/`. */
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

	/* Every lookup is started now rather than one after another. They
	 * are independent, and the network layer queues what it cannot send
	 * at once; walking them in sequence would make an export of a pack
	 * with a hundred hand-installed mods a hundred round trips long. */
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
				/* abort() has already reported, or is about to. Nothing
				 * here may add a second verdict. */
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
					/* Not an error: a mod that is not on Modrinth simply
					 * travels inside the pack. Settled with an empty
					 * reply so it takes the "unresolved" path rather
					 * than being dropped and leaving the count short. */
					qDebug() << "Modrinth export lookup failed:" << reason;
					settle(QByteArray());
				});

		job->start();
	}
}

void ModrinthPackExportTask::recordLookupResult(const PendingFile& pending,
												const QByteArray& bytes)
{
	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Could not parse the Modrinth version for"
				   << pending.path << ":" << parseError.errorString();
		return;
	}

	const QJsonArray files = doc.object().value("files").toArray();
	for (const QJsonValue& value : files) {
		const QJsonObject file = value.toObject();
		const QJsonObject hashes = file.value("hashes").toObject();

		/* The version owns several files - the mod, its sources, a
		 * javadoc jar - and only one of them is the file on disk. Picked
		 * by our own digest rather than by the `primary` flag, which
		 * points at whatever the author uploaded first. */
		if (hashes.value("sha1").toString().compare(
				pending.sha1, Qt::CaseInsensitive) != 0) {
			continue;
		}

		const QString url = file.value("url").toString();
		if (!isNameableUrl(url)) {
			/* Answered with a file served from somewhere we would not
			 * install from. Ships in `overrides/` instead. */
			return;
		}

		ResolvedFile resolved;
		resolved.url = url;
		resolved.sha1 = pending.sha1;
		resolved.sha512 = pending.sha512;
		resolved.size = pending.size;
		/* No side: a version says nothing about the environment it runs
		 * in - that lives on the project - and guessing "both" here
		 * would be indistinguishable from having been told. */
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
	/* Everything the manifest named is downloaded on install, so
	 * carrying it as well would double the size of the pack for no
	 * benefit. */
	zip->setExcludeFiles(m_resolved.keys());

	m_zipTask.reset(zip);

	/* There is no separate "aborted" signal to listen for: a Task that
	 * stops on request reports it as a failure whose reason says so. So
	 * the flag we latched in abort() is what tells the two apart, and it
	 * has to be consulted on both paths - the archive may well have
	 * succeeded in the instant before the user's click arrived. */
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

	/* The archive is the long part, so its progress is this task's
	 * progress rather than a step inside it. */
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
	/* Omitted rather than written empty: the field is optional, and an
	 * empty summary displayed as the pack's description is worse than no
	 * description at all. */
	if (!m_summary.isEmpty()) {
		index["summary"] = m_summary;
	}

	if (m_instance) {
		auto profile = m_instance->getPackProfile();

		/* The loaders an mrpack can express, under the names it knows
		 * them by. A loader the instance does not have contributes
		 * nothing: an empty version string here would be read as "this
		 * pack needs Forge" by whoever installs it. */
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
			/* Named under the enabled name, and marked optional. The
			 * `.disabled` suffix is this launcher's way of turning a mod
			 * off; shipped verbatim it would land as a file no loader
			 * reads, in a pack whose author meant to offer a choice. */
			path = pathInfo.dir().filePath(pathInfo.completeBaseName());
			env["client"] = "optional";
			env["server"] = "optional";
		} else {
			env["client"] = "required";
			env["server"] = "required";
		}

		/* A client-only mod is still a mod a client needs. Only the
		 * server side is narrowed, because a file marked server-only in
		 * an mrpack is one a client install skips entirely. */
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
