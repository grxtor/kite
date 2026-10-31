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

#include "modplatform/flame/FlamePackExportTask.h"

#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QtConcurrentRun>

#include <memory>
#include <utility>

#include "archive/ExportToZipTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/flame/FlameFingerprint.h"
#include "net/JsonPost.h"

namespace
{
	/* What a CurseForge pack can name: mods and resource packs. */
	enum class Candidate { Skip, Mod, ResourcePack };

	const QStringList EXPORTABLE_EXTENSIONS = {QStringLiteral("jar"),
											   QStringLiteral("zip")};

	bool hasExportableExtension(const QString& relative)
	{
		for (const QString& extension : EXPORTABLE_EXTENSIONS) {
			if (relative.endsWith(QLatin1Char('.') + extension) ||
				relative.endsWith(QLatin1Char('.') + extension +
								  QStringLiteral(".disabled"))) {
				return true;
			}
		}
		return false;
	}

	Candidate classify(const QString& relative)
	{
		if (!hasExportableExtension(relative)) {
			return Candidate::Skip;
		}
		if (relative.startsWith(QStringLiteral("mods/"))) {
			return Candidate::Mod;
		}
		if (relative.startsWith(QStringLiteral("resourcepacks/"))) {
			return Candidate::ResourcePack;
		}
		return Candidate::Skip;
	}

	/* This launcher's name for a switched-off file; only decides whether the
	 * manifest calls it required. */
	bool isEnabledName(const QString& relative)
	{
		return !relative.endsWith(QStringLiteral(".disabled"));
	}

	/* The loader id the manifest names. Only one can be named, so the more
	 * specific wins: a Quilt instance with Fabric mods must not become
	 * Fabric. */
	QString modLoaderId(const std::shared_ptr<PackProfile>& profile,
						const QString& minecraftVersion)
	{
		const QString quilt =
			profile->getComponentVersion(QStringLiteral("org.quiltmc.quilt-loader"));
		if (!quilt.isEmpty()) {
			return QStringLiteral("quilt-") + quilt;
		}

		const QString fabric = profile->getComponentVersion(
			QStringLiteral("net.fabricmc.fabric-loader"));
		if (!fabric.isEmpty()) {
			return QStringLiteral("fabric-") + fabric;
		}

		const QString forge =
			profile->getComponentVersion(QStringLiteral("net.minecraftforge"));
		if (!forge.isEmpty()) {
			return QStringLiteral("forge-") + forge;
		}

		const QString neoForge =
			profile->getComponentVersion(QStringLiteral("net.neoforged"));
		if (!neoForge.isEmpty()) {
			/* NeoForge's first release kept the id it was published under, with the
			 * Minecraft version in it. */
			if (minecraftVersion == QStringLiteral("1.20.1")) {
				return QStringLiteral("neoforge-1.20.1-") + neoForge;
			}
			return QStringLiteral("neoforge-") + neoForge;
		}

		return {};
	}
} // namespace

FlamePackExportTask::FlamePackExportTask(FlamePackExportOptions options,
										 QObject* parent)
	: Task(parent), m_options(std::move(options)),
	  m_gameRoot(m_options.instance->gameRoot())
{
}

void FlamePackExportTask::executeTask()
{
	setStatus(tr("Searching for files..."));
	setProgress(0, 0);

	connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished, this,
			&FlamePackExportTask::onScanFinished);
	m_scanFuture = QtConcurrent::run([this] { return scanFiles(); });
	m_scanWatcher.setFuture(m_scanFuture);
}

bool FlamePackExportTask::abort()
{
	if (m_aborted.exchange(true)) {
		return true;
	}

	if (m_request) {
		/* Its aborted signal reports the single verdict. */
		m_request->abort();
		return true;
	}

	if (m_zipTask) {
		m_zipTask->abort();
		return true;
	}

	if (m_scanWatcher.isRunning()) {
		/* Checked between files; onScanFinished() emits. */
		return true;
	}

	emitAborted();
	return true;
}

FlamePackExportTask::ScanResult FlamePackExportTask::scanFiles() const
{
	ScanResult result;

	QFileInfoList files;
	if (!MMCZip::collectFileListRecursively(m_gameRoot.absolutePath(),
										   QString(), &files,
										   m_options.filter)) {
		/* ok stays false: a partial list would export with files missing. */
		return result;
	}
	result.files = files;

	QHash<QString, std::shared_ptr<ModMetadataIndex>> indices;

	for (const QFileInfo& file : files) {
		if (m_aborted.load()) {
			return {};
		}

		const QString relative =
			m_gameRoot.relativeFilePath(file.absoluteFilePath());
		const Candidate candidate = classify(relative);
		if (candidate == Candidate::Skip) {
			continue;
		}

		const bool enabled = isEnabledName(relative);
		const bool isMod = candidate == Candidate::Mod;

		const QString folder = file.absolutePath();
		auto index = indices.value(folder);
		if (!index) {
			index = std::make_shared<ModMetadataIndex>(QDir(folder));
			index->load();
			indices.insert(folder, index);
		}

		const ModMetadataIndex::Entry entry = index->get(file.fileName());
		if (entry.isValid() &&
			entry.platform == QStringLiteral("curseforge")) {
			bool addonOk = false;
			bool fileOk = false;
			const int addonId = entry.projectId.toInt(&addonOk);
			const int fileId = entry.versionId.toInt(&fileOk);

			/* Both ids are numeric here; anything else came from elsewhere, and
			 * zero refers to nothing. */
			if (addonOk && fileOk && addonId > 0 && fileId > 0) {
				ResolvedFile resolved;
				resolved.addonId = addonId;
				resolved.fileId = fileId;
				resolved.enabled = enabled;
				resolved.isMod = isMod;
				resolved.name = entry.name;
				resolved.slug = entry.slug;
				result.resolved.insert(relative, resolved);
				continue;
			}
		}

		const std::optional<quint32> fingerprint =
			FlameFingerprint::ofFile(file.absoluteFilePath());
		if (!fingerprint) {
			/* Unreadable, so it cannot be named, but it belongs in the pack. */
			continue;
		}

		PendingFile pending;
		pending.path = relative;
		pending.fingerprint = *fingerprint;
		pending.enabled = enabled;
		pending.isMod = isMod;
		result.pending.append(pending);
	}

	result.ok = true;
	return result;
}

void FlamePackExportTask::onScanFinished()
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
		lookUpProjects();
		return;
	}

	matchFingerprints();
}

void FlamePackExportTask::matchFingerprints()
{
	setStatus(tr("Identifying files on CurseForge..."));
	setProgress(1, 4);

	QJsonArray fingerprints;
	QSet<quint32> seen;
	for (const PendingFile& pending : m_pending) {
		if (seen.contains(pending.fingerprint)) {
			continue;
		}
		seen.insert(pending.fingerprint);
		/* Widened: the fingerprint is unsigned and JSON numbers are signed,
		 * so values above 2^31 would turn negative. */
		fingerprints << QJsonValue(static_cast<qint64>(pending.fingerprint));
	}

	QJsonObject body;
	body["fingerprints"] = fingerprints;

	m_request.reset(new Net::JsonPost(
		tr("Identifying files on CurseForge..."),
		FlameApi::matchFingerprintsUrl(),
		QJsonDocument(body).toJson(QJsonDocument::Compact)));

	/* An aborted request reports as a failure, so that handler is also the
	 * abort handler. */
	connect(m_request.get(), &Task::succeeded, this,
			&FlamePackExportTask::onFingerprintsMatched);
	connect(m_request.get(), &Task::failed, this,
			&FlamePackExportTask::onFingerprintMatchFailed);

	m_request->start();
}

void FlamePackExportTask::onFingerprintsMatched()
{
	const QByteArray response = m_request->response();
	m_request.reset();

	if (m_aborted.load()) {
		emitAborted();
		return;
	}

	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		onFingerprintMatchFailed(parseError.errorString());
		return;
	}

	const QJsonArray matches = doc.object()
								   .value("data")
								   .toObject()
								   .value("exactMatches")
								   .toArray();

	for (const QJsonValue& value : matches) {
		const QJsonObject file = value.toObject().value("file").toObject();
		if (file.isEmpty()) {
			continue;
		}

		/* A file the platform will not serve cannot be fetched by an
		 * installer; it ships in `overrides/` instead. */
		if (!file.value("isAvailable").toBool()) {
			continue;
		}

		const int addonId = file.value("modId").toInt();
		const int fileId = file.value("id").toInt();
		if (addonId <= 0 || fileId <= 0) {
			continue;
		}

		const quint32 fingerprint = static_cast<quint32>(
			file.value("fileFingerprint").toVariant().toLongLong());

		/* Every pending file with this fingerprint: the same jar in two folders
		 * hashes the same. */
		for (const PendingFile& pending : m_pending) {
			if (pending.fingerprint != fingerprint) {
				continue;
			}

			ResolvedFile resolved;
			resolved.addonId = addonId;
			resolved.fileId = fileId;
			resolved.enabled = pending.enabled;
			resolved.isMod = pending.isMod;
			m_resolved.insert(pending.path, resolved);
		}
	}

	m_pending.clear();
	lookUpProjects();
}

void FlamePackExportTask::onFingerprintMatchFailed(const QString& reason)
{
	m_request.reset();
	if (m_aborted.load()) {
		/* Also the abort path: a stopped request reports as failed. */
		emitAborted();
		return;
	}

	qWarning() << "Could not identify files on CurseForge:" << reason
			   << "- they will be exported inside the pack instead.";
	m_pending.clear();
	lookUpProjects();
}

void FlamePackExportTask::lookUpProjects()
{
	QJsonArray addonIds;
	QSet<int> seen;
	for (const ResolvedFile& resolved : m_resolved) {
		if (seen.contains(resolved.addonId)) {
			continue;
		}
		seen.insert(resolved.addonId);
		addonIds << QJsonValue(resolved.addonId);
	}

	if (addonIds.isEmpty()) {
		buildZip();
		return;
	}

	setStatus(tr("Fetching project details from CurseForge..."));
	setProgress(2, 4);

	QJsonObject body;
	body["modIds"] = addonIds;

	m_request.reset(new Net::JsonPost(
		tr("Fetching project details from CurseForge..."),
		FlameApi::projectsUrl(),
		QJsonDocument(body).toJson(QJsonDocument::Compact)));

	connect(m_request.get(), &Task::succeeded, this,
			&FlamePackExportTask::onProjectsLookedUp);
	connect(m_request.get(), &Task::failed, this,
			&FlamePackExportTask::onProjectLookupFailed);

	m_request->start();
}

void FlamePackExportTask::onProjectsLookedUp()
{
	const QByteArray response = m_request->response();
	m_request.reset();

	if (m_aborted.load()) {
		emitAborted();
		return;
	}

	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		onProjectLookupFailed(parseError.errorString());
		return;
	}

	for (const QJsonValue& value : doc.object().value("data").toArray()) {
		const QJsonObject project = value.toObject();
		const int addonId = project.value("id").toInt();
		if (addonId <= 0) {
			continue;
		}

		const QString name = project.value("name").toString();
		const QString slug = project.value("slug").toString();

		QStringList authors;
		for (const QJsonValue& author : project.value("authors").toArray()) {
			const QString authorName = author.toObject().value("name").toString();
			if (!authorName.isEmpty()) {
				authors << authorName;
			}
		}

		/* Overwrites the sidecar name: the catalogue is authoritative for the
		 * project title, and the sidecar's goes stale after a rename. */
		for (auto it = m_resolved.begin(); it != m_resolved.end(); it++) {
			if (it.value().addonId != addonId) {
				continue;
			}
			if (!name.isEmpty()) {
				it.value().name = name;
			}
			if (!slug.isEmpty()) {
				it.value().slug = slug;
			}
			it.value().authors = authors.join(QStringLiteral(", "));
		}
	}

	buildZip();
}

void FlamePackExportTask::onProjectLookupFailed(const QString& reason)
{
	m_request.reset();
	if (m_aborted.load()) {
		emitAborted();
		return;
	}

	qWarning() << "Could not fetch project details from CurseForge:" << reason
			   << "- the mod list will be less complete.";
	buildZip();
}

void FlamePackExportTask::buildZip()
{
	setStatus(tr("Adding files..."));
	setProgress(3, 4);

	auto* zip =
		new MMCZip::ExportToZipTask(m_options.output, m_gameRoot, m_files,
									QStringLiteral("overrides/"), true);
	zip->addExtraFile(QStringLiteral("manifest.json"), generateManifest());
	zip->addExtraFile(QStringLiteral("modlist.html"), generateModList());
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

QByteArray FlamePackExportTask::generateManifest() const
{
	QJsonObject manifest;
	manifest["manifestType"] = "minecraftModpack";
	manifest["manifestVersion"] = 1;
	manifest["name"] = m_options.name;
	manifest["version"] = m_options.version;
	manifest["author"] = m_options.author;
	manifest["overrides"] = "overrides";

	QJsonObject minecraft;
	if (m_options.instance) {
		auto profile = m_options.instance->getPackProfile();
		const QString minecraftVersion =
			profile->getComponentVersion(QStringLiteral("net.minecraft"));
		if (!minecraftVersion.isEmpty()) {
			minecraft["version"] = minecraftVersion;
		}

		/* Always present, even empty: readers expect the key. */
		QJsonArray loaders;
		const QString loaderId = modLoaderId(profile, minecraftVersion);
		if (!loaderId.isEmpty()) {
			QJsonObject loader;
			loader["id"] = loaderId;
			loader["primary"] = true;
			loaders << loader;
		}
		minecraft["modLoaders"] = loaders;
	}

	if (m_options.recommendedRAM > 0) {
		minecraft["recommendedRam"] = m_options.recommendedRAM;
	}
	manifest["minecraft"] = minecraft;

	QJsonArray files;
	for (const ResolvedFile& resolved : m_resolved) {
		QJsonObject file;
		file["projectID"] = resolved.addonId;
		file["fileID"] = resolved.fileId;
		/* A disabled file is optional only when the user asked for that. */
		file["required"] = resolved.enabled || !m_options.optionalFiles;
		files << file;
	}
	manifest["files"] = files;

	return QJsonDocument(manifest).toJson(QJsonDocument::Compact);
}

QByteArray FlamePackExportTask::generateModList() const
{
	QString list = QStringLiteral("<ul>");

	for (auto it = m_resolved.constBegin(); it != m_resolved.constEnd(); it++) {
		const ResolvedFile& resolved = it.value();

		if (!resolved.isMod) {
			/* Resource packs are in the manifest but are not mods; crediting them
			 * here would credit a texture artist as a mod author. */
			continue;
		}

		/* The catalogue's title, else the file name; the title lookup may fail
		 * without failing the export. */
		QString name = resolved.name;
		if (name.isEmpty()) {
			name = QFileInfo(it.key()).completeBaseName();
		}

		const QString url =
			FlameApi::get()
				.projectPageUrl(QString::number(resolved.addonId))
				.toString();

		/* Escaped: project titles and author names come from a public
		 * catalogue and this file is opened in a browser. */
		QString entry = QStringLiteral("<li><a href=\"%1\">%2")
							.arg(url.toHtmlEscaped(), name.toHtmlEscaped());
		if (!resolved.authors.isEmpty()) {
			entry += QStringLiteral(" (by %1)").arg(
				resolved.authors.toHtmlEscaped());
		}
		entry += QStringLiteral("</a></li>\n");

		list += entry;
	}

	list += QStringLiteral("</ul>");
	return list.toUtf8();
}
