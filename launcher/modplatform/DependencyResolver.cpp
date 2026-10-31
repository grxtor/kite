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

#include "DependencyResolver.h"
#include "Json.h"
#include "core/LauncherContext.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentType.h"
#include "modplatform/VersionPicker.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/Download.h"
#include "net/NetJob.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace
{

	/* CurseForge releaseType as the channel name; Modrinth states it
	 * directly in version_type. */
	QString curseForgeReleaseTypeName(int releaseType)
	{
		switch (releaseType) {
			case 1:
				return QStringLiteral("release");
			case 2:
				return QStringLiteral("beta");
			case 3:
				return QStringLiteral("alpha");
			default:
				return QString();
		}
	}

	/* Adds a requester's names to the entry, and the wanted id as slug if
	 * it has none. */
	void mergeNote(ModPlatform::DependencyInfo& dep,
				   const QStringList& requiredBy, const QString& slug)
	{
		for (const QString& name : requiredBy) {
			if (!name.isEmpty() && !dep.requiredBy.contains(name)) {
				dep.requiredBy.append(name);
			}
		}
		if (dep.slug.isEmpty()) {
			dep.slug = slug;
		}
	}

	/* Modrinth slug for mod ids that differ from it. Only certain ids
	 * belong here: a wrong entry installs the wrong mod. */
	QString modrinthSlugFor(const QString& modId)
	{
		static const QHash<QString, QString> slugs = {
			{QStringLiteral("fabric"), QStringLiteral("fabric-api")},
			{QStringLiteral("fabric_api"), QStringLiteral("fabric-api")},
			{QStringLiteral("architectury"), QStringLiteral("architectury-api")},
			{QStringLiteral("owo"), QStringLiteral("owo-lib")},
			{QStringLiteral("yet_another_config_lib_v3"), QStringLiteral("yacl")},
			{QStringLiteral("kotlinforforge"), QStringLiteral("kotlin-for-forge")},
			{QStringLiteral("cloth_config"), QStringLiteral("cloth-config")},
			{QStringLiteral("forgeconfigapiport"),
			 QStringLiteral("forge-config-api-port")},
			{QStringLiteral("geckolib3"), QStringLiteral("geckolib")},
			{QStringLiteral("puzzleslib"), QStringLiteral("puzzles-lib")},
			{QStringLiteral("resourcefullib"), QStringLiteral("resourceful-lib")},
			{QStringLiteral("roughlyenoughitems"), QStringLiteral("rei")},
			{QStringLiteral("cardinal-components"),
			 QStringLiteral("cardinal-components-api")},
			{QStringLiteral("fzzy_config"), QStringLiteral("fzzy-config")},
			{QStringLiteral("lambdynlights"), QStringLiteral("lambdynamiclights")},
		};
		return slugs.value(modId.toLower(), modId);
	}

} // namespace

DependencyResolver::DependencyResolver(
	const QList<ModPlatform::SelectedMod>& selectedMods,
	const QString& mcVersion, const QString& loader, QObject* parent)
	: Task(parent), m_selectedMods(selectedMods), m_mcVersion(mcVersion),
	  m_loader(loader)
{
	// Mods already selected need no dependency fetch.
	for (const auto& mod : m_selectedMods) {
		m_resolvedProjectIds.insert(mod.platform + ":" + mod.projectId);
		m_resolvedNames.insert(normalizeName(mod.name));
	}
}

void DependencyResolver::setInstalledIndex(
	std::shared_ptr<ModMetadataIndex> index)
{
	m_installed = std::move(index);

	// Not used to pre-populate the resolved sets: a library on disk at the
	// wrong version must still be offered. See acceptDependency().
}

bool DependencyResolver::isVersionInstalled(const QString& platform,
											const QString& projectId,
											const QString& versionId) const
{
	if (!m_installed || platform.isEmpty() || projectId.isEmpty() ||
		versionId.isEmpty()) {
		return false;
	}
	const auto entry = m_installed->findByPlatformProject(platform, projectId);
	return entry.isValid() && entry.versionId == versionId;
}

bool DependencyResolver::isProjectInstalled(const QString& platform,
											const QString& projectId,
											const QString& name) const
{
	if (!m_installed) {
		return false;
	}
	if (!platform.isEmpty() && !projectId.isEmpty() &&
		m_installed->findByPlatformProject(platform, projectId).isValid()) {
		return true;
	}
	/* By name too: catches the same library from the other site. */
	const QString normalized = normalizeName(name);
	return !normalized.isEmpty() &&
		   m_installed->findByNormalizedName(normalized).isValid();
}

void DependencyResolver::acceptDependency(ModPlatform::DependencyInfo dep)
{
	if (dep.downloadUrl.isEmpty()) {
		qWarning() << "Dependency" << dep.name << "on" << dep.platform
				   << "has no download URL, skipping";
		return;
	}

	if (isVersionInstalled(dep.platform, dep.projectId, dep.versionId)) {
		qDebug() << "Dependency" << dep.name
				 << "is already installed at this version, dropping";
		return;
	}

	/* On disk at another version: offered, unticked. */
	dep.maybeInstalled =
		isProjectInstalled(dep.platform, dep.projectId, dep.name);

	m_resolvedNames.insert(normalizeName(dep.name));
	/* Requesters that arrived while it was in flight. */
	const auto late = m_lateNotes.take(dep.platform + ":" + dep.projectId);
	mergeNote(dep, late.requiredBy, late.slug);
	m_dependencies.append(dep);
}

void DependencyResolver::noteAlsoRequiredBy(const QString& platform,
											const QString& projectId,
											const QStringList& requiredBy,
											const QString& slug)
{
	if (requiredBy.isEmpty() && slug.isEmpty()) {
		return;
	}
	for (auto& dep : m_dependencies) {
		if (dep.platform == platform && dep.projectId == projectId) {
			mergeNote(dep, requiredBy, slug);
			return;
		}
	}
	/* Its lookup is still out: keep this for acceptDependency(). */
	mergeNote(m_lateNotes[platform + ":" + projectId], requiredBy, slug);
}

QString DependencyResolver::normalizeName(const QString& name)
{
	/* The index owns the rule, so resolver and index agree on "same". */
	return ModMetadataIndex::normalizeName(name);
}

void DependencyResolver::addWantedProjects(const QList<WantedProject>& wanted)
{
	m_wanted = wanted;
}

void DependencyResolver::executeTask()
{
	if (m_selectedMods.isEmpty() && m_wanted.isEmpty()) {
		emitSucceeded();
		return;
	}
	setStatus(tr("Resolving dependencies..."));
	m_currentModIndex = 0;
	/* Wanted lookups must be in flight and counted before resolveNextMod()
	 * checks for completion. */
	resolveWantedProjects();
	resolveNextMod();
}

bool DependencyResolver::abort()
{
	if (m_aborted) {
		return true;
	}
	m_aborted = true;

	/* Call off what is in the air; each job's failed() is dropped via
	 * m_aborted. */
	for (const QPointer<NetJob>& job : m_activeJobs) {
		if (job && job->isRunning()) {
			job->abort();
		}
	}
	m_activeJobs.clear();

	/* emitAborted() warns unless the task is running. */
	if (isRunning()) {
		emitAborted();
	}
	return true;
}

void DependencyResolver::request(const QString& name, const QUrl& url,
								 std::function<void(const QByteArray&)> onDone)
{
	if (m_aborted) {
		return;
	}

	/* Shared by both handlers; freed with the job if neither fires. */
	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(name, LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(url, response.get()));

	m_pendingRequests++;
	m_activeJobs.append(job);

	/* An aborted job reports after its download unwinds, possibly after
	 * this resolver is gone, and may still be handed data until then: the
	 * buffer lives as long as the job. */
	connect(job, &QObject::destroyed, job, [response] {});
	connect(job, &Task::finished, job, &QObject::deleteLater);

	auto finish = [this, job](const std::function<void(const QByteArray&)>& fn,
							  const QByteArray& bytes) {
		m_activeJobs.removeAll(QPointer<NetJob>(job));
		job->deleteLater();

		if (m_aborted) {
			m_pendingRequests--;
			return;
		}

		fn(bytes);
		m_pendingRequests--;
		checkCompletion();
	};

	connect(job, &NetJob::succeeded, this,
			[finish, onDone, response] { finish(onDone, *response); });
	connect(job, &NetJob::failed, this,
			[finish, onDone, response, name](QString reason) {
				qWarning() << "Lookup failed:" << name << ":" << reason;
				/* Empty reply: the handler takes its "could not resolve" path. */
				finish(onDone, QByteArray());
			});

	propagateStepsFrom(job);
	job->start();
}

void DependencyResolver::resolveWantedProjects()
{
	for (const WantedProject& wanted : m_wanted) {
		if (wanted.id.isEmpty()) {
			continue;
		}

		/* A jar-supplied id: percent-encoded, not trusted as a path
		 * segment. */
		ModPlatform::VersionQuery query;
		query.projectId = QString::fromLatin1(
			QUrl::toPercentEncoding(modrinthSlugFor(wanted.id)));
		query.contentType = ModPlatform::ContentType::Mod;
		query.mcVersions = ModPlatform::singleVersionList(m_mcVersion);
		query.loaders = ModPlatform::singleLoaderList(m_loader);

		request(QString("MR::WantedProject(%1)").arg(wanted.id),
				ModrinthApi::get().projectVersionsUrl(query),
				[this, wanted](const QByteArray& bytes) {
					onWantedProjectResolved(wanted, bytes);
				});
	}
}

void DependencyResolver::onWantedProjectResolved(const WantedProject& wanted,
												 const QByteArray& data)
{
	/* A 404 (empty reply) and an empty version list both go to the other
	 * provider. */
	const QJsonArray versions = QJsonDocument::fromJson(data).array();
	const QJsonObject newest =
		ModPlatform::newestModrinthVersion(versions, m_loader);
	if (newest.isEmpty()) {
		qWarning() << "No Modrinth version of wanted project" << wanted.id
				   << "- attempting cross resolve to CurseForge";
		executeCrossResolve(QStringLiteral("curseforge"), wanted.id, wanted.id,
							wanted.requiredBy, wanted.id);
		return;
	}

	/* Recorded by id so other dependents do not fetch it again. */
	QString projectId = newest.value("project_id").toString();
	if (projectId.isEmpty()) {
		projectId = wanted.id;
	}
	const QString key = "modrinth:" + projectId;
	if (m_resolvedProjectIds.contains(key)) {
		noteAlsoRequiredBy(QStringLiteral("modrinth"), projectId,
						   wanted.requiredBy, wanted.id);
		return;
	}
	m_resolvedProjectIds.insert(key);

	onDependencyProjectResolved(QStringLiteral("modrinth"), projectId, data,
								wanted.requiredBy, wanted.id);
	nameAfterProject(projectId);
}

void DependencyResolver::nameAfterProject(const QString& projectId)
{
	/* The project's own title: version names are tagged freely by authors. */
	request(QString("MR::WantedTitle(%1)").arg(projectId),
			ModrinthApi::get().projectUrl(projectId),
			[this, projectId](const QByteArray& bytes) {
				const QString title = QJsonDocument::fromJson(bytes)
										  .object()
										  .value("title")
										  .toString();
				if (title.isEmpty()) {
					return;
				}
				for (auto& dep : m_dependencies) {
					if (dep.platform == QLatin1String("modrinth") &&
						dep.projectId == projectId) {
						dep.name = title;
						return;
					}
				}
			});
}

void DependencyResolver::resolveNextMod()
{
	if (m_aborted) {
		return;
	}

	if (m_currentModIndex >= m_selectedMods.size()) {
		checkCompletion();
		return;
	}

	const auto& mod = m_selectedMods[m_currentModIndex];
	if (mod.platform == "curseforge") {
		resolveCurseForgeDependencies(mod);
	} else if (mod.platform == "modrinth") {
		resolveModrinthDependencies(mod);
	} else {
		m_currentModIndex++;
		resolveNextMod();
	}
}

void DependencyResolver::resolveCurseForgeDependencies(
	const ModPlatform::SelectedMod& mod)
{
	if (mod.versionId.isEmpty()) {
		m_currentModIndex++;
		resolveNextMod();
		return;
	}

	const auto currentMod = mod;
	request(QString("CF::DepResolve(%1)").arg(mod.name),
			FlameApi::fileUrl(mod.projectId, mod.versionId),
			[this, currentMod](const QByteArray& bytes) {
				if (!bytes.isEmpty()) {
					onCurseForgeVersionResolved(currentMod, bytes);
				}
				/* An empty reply must not stall the queue. */
				m_currentModIndex++;
				resolveNextMod();
			});
}

void DependencyResolver::resolveModrinthDependencies(
	const ModPlatform::SelectedMod& mod)
{
	/* A stored version id may be a version number (mrpack installs keep
	 * only the hash and URL), which 404s like a mod with no dependencies;
	 * the file hash addresses the same version object. */
	QUrl lookup;
	if (ModrinthApi::isVersionId(mod.versionId)) {
		lookup = ModrinthApi::versionUrl(mod.versionId);
	} else if (!mod.sha1.isEmpty()) {
		lookup = ModrinthApi::versionByHashUrl(mod.sha1);
	} else {
		if (!mod.versionId.isEmpty()) {
			qWarning() << "Not resolving dependencies for" << mod.name
					   << ": recorded version" << mod.versionId
					   << "is not a Modrinth version id and no file hash "
						  "is known";
		}
		m_currentModIndex++;
		resolveNextMod();
		return;
	}

	const auto currentMod = mod;
	request(QString("MR::DepResolve(%1)").arg(mod.name), lookup,
			[this, currentMod](const QByteArray& bytes) {
				if (!bytes.isEmpty()) {
					onModrinthVersionResolved(currentMod, bytes);
				}
			});

	/* Unlike CurseForge, does not wait for the reply. */
	m_currentModIndex++;
	resolveNextMod();
}

void DependencyResolver::onCurseForgeVersionResolved(
	const ModPlatform::SelectedMod& mod, const QByteArray& data)
{
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error != QJsonParseError::NoError)
		return;

	QJsonObject fileObj;
	if (doc.isObject() && doc.object().contains("data")) {
		fileObj = doc.object().value("data").toObject();
	} else {
		fileObj = doc.object();
	}

	processCFFileDeps(fileObj, {mod.name});
}

void DependencyResolver::onModrinthVersionResolved(
	const ModPlatform::SelectedMod& mod, const QByteArray& data)
{
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error != QJsonParseError::NoError)
		return;

	processMRVersionDeps(doc.object(), {mod.name});
}

void DependencyResolver::onDependencyProjectResolved(
	const QString& platform, const QString& projectId, const QByteArray& data,
	const QStringList& requiredBy, const QString& wantedId)
{
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error != QJsonParseError::NoError)
		return;

	if (platform == "curseforge") {
		// /v1/mods/{id}/files response
		QJsonArray filesArr;
		if (doc.isObject() && doc.object().contains("data")) {
			auto dataVal = doc.object().value("data");
			if (dataVal.isArray()) {
				filesArr = dataVal.toArray();
			} else if (dataVal.isObject()) {
				filesArr.append(dataVal);
			}
		}

		if (filesArr.isEmpty()) {
			qWarning() << "No compatible files found for CurseForge dependency"
					   << projectId << "- attempting cross resolve to Modrinth";
			noUsableCurseForgeFile(projectId, requiredBy, wantedId);
			return;
		}

		/* Newest file for this loader; CurseForge may list a Forge build
		 * first for a Fabric instance. */
		const auto fileObj =
			ModPlatform::newestCurseForgeFile(filesArr, m_loader);
		if (fileObj.isEmpty()) {
			qWarning() << "No file for loader" << m_loader
					   << "among CurseForge dependency" << projectId
					   << "files - attempting cross resolve to Modrinth";
			noUsableCurseForgeFile(projectId, requiredBy, wantedId);
			return;
		}

		ModPlatform::DependencyInfo dep;
		dep.projectId = projectId;
		dep.platform = "curseforge";
		dep.isRequired = true;
		dep.requiredBy = requiredBy;
		dep.slug = wantedId;
		dep.versionId = QString::number(Json::ensureInteger(fileObj, "id", 0));
		dep.fileName = Json::ensureString(fileObj, "fileName", "");
		dep.downloadUrl = Json::ensureString(fileObj, "downloadUrl", "");
		dep.fileSize = Json::ensureInteger(fileObj, "fileLength", 0);
		dep.sha1 = ModPlatform::curseForgeSha1FromFileObject(fileObj);
		dep.name = Json::ensureString(fileObj, "displayName", dep.fileName);
		dep.versionType = curseForgeReleaseTypeName(
			Json::ensureInteger(fileObj, "releaseType", 0));

		// Restricted downloads have a null downloadUrl.
		if (dep.downloadUrl.isEmpty()) {
			int fileId = Json::ensureInteger(fileObj, "id", 0);
			if (fileId > 0 && !dep.fileName.isEmpty()) {
				dep.downloadUrl = FlameApi::browserDownloadUrl(
					projectId, QString::number(fileId));
				dep.browserDownloadOnly = true;
				qWarning() << "CurseForge dependency" << projectId
						   << "has restricted download, using fallback URL";
			}
		}

		if (dep.downloadUrl.isEmpty()) {
			qWarning() << "CurseForge dependency" << projectId
					   << "has no download URL, skipping";
			return;
		}

		qDebug() << "Resolved CurseForge dependency:" << dep.name
				 << "file:" << dep.fileName << "url:" << dep.downloadUrl;
		const QString depName = dep.name;
		acceptDependency(dep);
		processCFFileDeps(fileObj, {depName});
	} else if (platform == "modrinth") {
		QJsonArray arr;
		if (doc.isArray()) {
			arr = doc.array();
		} else {
			return;
		}

		if (arr.isEmpty()) {
			qWarning() << "No compatible files found for Modrinth dependency"
					   << projectId
					   << "- attempting cross resolve to CurseForge";
			crossResolveFromModrinth(projectId, requiredBy);
			return;
		}

		const auto vObj = ModPlatform::newestModrinthVersion(arr, m_loader);
		if (vObj.isEmpty()) {
			qWarning() << "No version for loader" << m_loader
					   << "among Modrinth dependency" << projectId
					   << "versions - attempting cross resolve to CurseForge";
			crossResolveFromModrinth(projectId, requiredBy);
			return;
		}

		ModPlatform::DependencyInfo dep;
		dep.projectId = projectId;
		dep.versionId = Json::ensureString(vObj, "id", "");
		dep.name = Json::ensureString(vObj, "name", projectId);
		dep.platform = "modrinth";
		dep.isRequired = true;
		dep.requiredBy = requiredBy;
		dep.slug = wantedId;
		dep.versionType = Json::ensureString(vObj, "version_type", "");

		auto files = Json::ensureArray(vObj, "files");
		for (auto fileRaw : files) {
			auto fileObj = fileRaw.toObject();
			bool primary = Json::ensureBoolean(fileObj, QStringLiteral("primary"), false);
			if (primary || files.size() == 1) {
				dep.downloadUrl = Json::ensureString(fileObj, "url", "");
				dep.fileName = Json::ensureString(fileObj, "filename", "");
				dep.fileSize = Json::ensureInteger(fileObj, "size", 0);
				auto hashes = Json::ensureObject(fileObj, "hashes");
				dep.sha1 = Json::ensureString(hashes, "sha1", "");
				break;
			}
		}

		if (dep.downloadUrl.isEmpty()) {
			qWarning() << "Modrinth dependency" << projectId
					   << "has no downloadable file, skipping";
			return;
		}

		const QString depName = dep.name;
		acceptDependency(dep);
		processMRVersionDeps(vObj, {depName});
	}
}

void DependencyResolver::processCFFileDeps(const QJsonObject& fileObj,
										   const QStringList& requiredBy)
{
	auto deps = Json::ensureArray(fileObj, "dependencies");
	for (auto depRaw : deps) {
		auto depObj = depRaw.toObject();
		int relationType = Json::ensureInteger(depObj, "relationType", 0);

		int modId = Json::ensureInteger(depObj, "modId", 0);
		if (modId == 0)
			continue;

		QString depProjectId = QString::number(modId);
		QString key = "curseforge:" + depProjectId;

		// Embedded/included deps are not downloaded again for other mods.
		if (relationType == 1 ||
			relationType == 6) { // 1=EmbeddedLibrary, 6=Include
			m_resolvedProjectIds.insert(key);
			continue;
		}

		if (relationType != 3) {
			/* 2 Optional, 4 Tool, 5 Incompatible: only required dependencies
			 * are followed. */
			continue;
		}

		if (m_resolvedProjectIds.contains(key)) {
			/* Already on its way in: this requester just joins the list. */
			noteAlsoRequiredBy(QStringLiteral("curseforge"), depProjectId,
							   requiredBy);
			continue;
		}
		m_resolvedProjectIds.insert(key);

		ModPlatform::VersionQuery cfQuery;
		cfQuery.projectId = depProjectId;
		cfQuery.contentType = ModPlatform::ContentType::Mod;
		cfQuery.mcVersions = ModPlatform::singleVersionList(m_mcVersion);
		cfQuery.loaders = ModPlatform::singleLoaderList(m_loader);
		const auto url = FlameApi::get().projectVersionsUrl(cfQuery);
		qDebug() << "Fetching CurseForge dependency files (recursive):"
				 << url.toString();

		request(QString("CF::DepFetch(%1)").arg(depProjectId), url,
				[this, depProjectId, requiredBy](const QByteArray& bytes) {
					if (!bytes.isEmpty()) {
						onDependencyProjectResolved("curseforge", depProjectId,
													bytes, requiredBy);
					}
				});
	}
}

void DependencyResolver::processMRVersionDeps(const QJsonObject& versionObj,
											  const QStringList& requiredBy)
{
	auto deps = Json::ensureArray(versionObj, "dependencies");
	for (auto depRaw : deps) {
		auto depObj = depRaw.toObject();
		QString depType = Json::ensureString(depObj, "dependency_type", "");

		QString depProjectId = Json::ensureString(depObj, "project_id", "");
		QString depVersionId = Json::ensureString(depObj, "version_id", "");
		if (depProjectId.isEmpty())
			continue;

		QString key = "modrinth:" + depProjectId;

		// Embedded deps are not downloaded again for other mods.
		if (depType == "embedded") {
			m_resolvedProjectIds.insert(key);
			continue;
		}

		if (depType != "required") {
			/* optional and incompatible are not followed. */
			continue;
		}

		if (m_resolvedProjectIds.contains(key)) {
			noteAlsoRequiredBy(QStringLiteral("modrinth"), depProjectId,
							   requiredBy);
			continue;
		}
		m_resolvedProjectIds.insert(key);

		if (!depVersionId.isEmpty()) {
			request(
				QString("MR::DepVersion(%1)").arg(depProjectId),
				ModrinthApi::versionUrl(depVersionId),
				[this, depProjectId, depVersionId,
				 requiredBy](const QByteArray& bytes) {
					if (bytes.isEmpty()) {
						/* Not retried through the project endpoint: on a systematic
						 * failure that doubles the dead requests. */
						return;
					}

					const auto vObj = QJsonDocument::fromJson(bytes).object();

					/* The exact version may predate this instance's Minecraft. */
					bool compatible = false;
					for (auto v : Json::ensureArray(vObj, "game_versions")) {
						if (v.toString() == m_mcVersion) {
							compatible = true;
							break;
						}
					}

					if (!compatible) {
						qWarning()
							<< "Modrinth dep version" << depVersionId
							<< "is not compatible with MC" << m_mcVersion
							<< "- falling back to project version search";

						/* Ask the project for a fitting version; request() counts it. */
						ModPlatform::VersionQuery fbQuery;
						fbQuery.projectId = depProjectId;
						fbQuery.contentType = ModPlatform::ContentType::Mod;
						fbQuery.mcVersions =
							ModPlatform::singleVersionList(m_mcVersion);
						fbQuery.loaders =
							ModPlatform::singleLoaderList(m_loader);

						request(
							QString("MR::DepProject(%1)").arg(depProjectId),
							ModrinthApi::get().projectVersionsUrl(fbQuery),
							[this, depProjectId,
							 requiredBy](const QByteArray& fb) {
								if (!fb.isEmpty()) {
									onDependencyProjectResolved(
										"modrinth", depProjectId, fb,
										requiredBy);
								}
							});
						return;
					}

					ModPlatform::DependencyInfo dep;
					dep.projectId = depProjectId;
					dep.versionId = Json::ensureString(vObj, "id", "");
					dep.name = Json::ensureString(vObj, "name", depProjectId);
					dep.platform = "modrinth";
					dep.isRequired = true;
					dep.requiredBy = requiredBy;
					dep.versionType =
						Json::ensureString(vObj, "version_type", "");

					auto files = Json::ensureArray(vObj, "files");
					for (auto fileRaw : files) {
						auto fileObj = fileRaw.toObject();
						bool primary =
							Json::ensureBoolean(fileObj, QStringLiteral("primary"), false);
						if (primary || files.size() == 1) {
							dep.downloadUrl =
								Json::ensureString(fileObj, "url", "");
							dep.fileName =
								Json::ensureString(fileObj, "filename", "");
							dep.fileSize =
								Json::ensureInteger(fileObj, "size", 0);
							auto hashes = Json::ensureObject(fileObj, "hashes");
							dep.sha1 = Json::ensureString(hashes, "sha1", "");
							break;
						}
					}

					if (dep.downloadUrl.isEmpty()) {
						qWarning() << "Modrinth dep version" << depVersionId
								   << "has no downloadable file, skipping";
						return;
					}

					const QString depName = dep.name;
					acceptDependency(dep);
					processMRVersionDeps(vObj, {depName});
				});
		} else {
			ModPlatform::VersionQuery mrQuery;
			mrQuery.projectId = depProjectId;
			mrQuery.contentType = ModPlatform::ContentType::Mod;
			mrQuery.mcVersions = ModPlatform::singleVersionList(m_mcVersion);
			mrQuery.loaders = ModPlatform::singleLoaderList(m_loader);

			request(QString("MR::DepProject(%1)").arg(depProjectId),
					ModrinthApi::get().projectVersionsUrl(mrQuery),
					[this, depProjectId, requiredBy](const QByteArray& bytes) {
						if (!bytes.isEmpty()) {
							onDependencyProjectResolved(
								"modrinth", depProjectId, bytes, requiredBy);
						}
					});
		}
	}
}

void DependencyResolver::crossResolveFromCurseForge(
	const QString& projectId, const QStringList& requiredBy)
{
	request(QString("CF::FetchName(%1)").arg(projectId),
			FlameApi::get().projectUrl(projectId),
			[this, projectId, requiredBy](const QByteArray& bytes) {
				const QJsonDocument doc = QJsonDocument::fromJson(bytes);

				QString name;
				QString slug;
				if (doc.isObject() && doc.object().contains("data")) {
					const auto dataObj = doc.object().value("data").toObject();
					name = Json::ensureString(dataObj, "name", "");
					slug = Json::ensureString(dataObj, "slug", "");
				}
				if (!name.isEmpty()) {
					qDebug() << "CF cross resolve extracted name:" << name
							 << "slug:" << slug << "for" << projectId;
					executeCrossResolve("modrinth", name, slug, requiredBy);
				}
			});
}

void DependencyResolver::crossResolveFromModrinth(
	const QString& projectId, const QStringList& requiredBy)
{
	request(QString("MR::FetchName(%1)").arg(projectId),
			ModrinthApi::get().projectUrl(projectId),
			[this, projectId, requiredBy](const QByteArray& bytes) {
				const QJsonDocument doc = QJsonDocument::fromJson(bytes);

				QString name;
				QString slug;
				if (doc.isObject()) {
					name = Json::ensureString(doc.object(), "title", "");
					slug = Json::ensureString(doc.object(), "slug", "");
				}
				if (!name.isEmpty()) {
					qDebug() << "MR cross resolve extracted title:" << name
							 << "slug:" << slug << "for" << projectId;
					executeCrossResolve("curseforge", name, slug, requiredBy);
				}
			});
}

void DependencyResolver::noUsableCurseForgeFile(const QString& projectId,
												const QStringList& requiredBy,
												const QString& wantedId)
{
	if (wantedId.isEmpty()) {
		crossResolveFromCurseForge(projectId, requiredBy);
		return;
	}
	ModPlatform::UnresolvedDep unresolved;
	unresolved.name = wantedId;
	m_unresolvedDeps.append(unresolved);
}

void DependencyResolver::executeCrossResolve(const QString& targetPlatform,
											 const QString& projectName,
											 const QString& sourceSlug,
											 const QStringList& requiredBy,
											 const QString& wantedId)
{
	// Cross-platform dedup by name.
	QString normalized = normalizeName(projectName);
	if (m_resolvedNames.contains(normalized)) {
		qDebug() << "Cross resolve skipped - already resolved by name:"
				 << projectName;
		return;
	}

	const QString query = QUrl::toPercentEncoding(projectName);
	const QUrl url = (targetPlatform == "modrinth")
						 ? ModrinthApi::nameSearchUrl(query)
						 : FlameApi::nameSearchUrl(query);

	request(
		QString("CrossSearch(%1)").arg(projectName), url,
		[this, targetPlatform, projectName, sourceSlug, requiredBy,
		 wantedId](const QByteArray& bytes) {
			if (bytes.isEmpty()) {
				/* The search failed: report the dependency as unresolved. */
				qWarning() << "Cross resolve search failed for" << projectName;
				ModPlatform::UnresolvedDep unresolved;
				unresolved.name = projectName;
				m_unresolvedDeps.append(unresolved);
				return;
			}

			const QJsonDocument doc = QJsonDocument::fromJson(bytes);

			// Prefer an exact slug match, then a name match.
			QString hitId;
			QString hitSlug;
			QString hitName;

			if (targetPlatform == "modrinth") {
				if (doc.isObject() && doc.object().contains("hits")) {
					auto hits = Json::ensureArray(doc.object(), "hits");
					for (auto hitRaw : hits) {
						auto hitObj = hitRaw.toObject();
						QString candidateSlug =
							Json::ensureString(hitObj, "slug", "");
						QString candidateId =
							Json::ensureString(hitObj, "project_id", "");
						QString candidateName =
							Json::ensureString(hitObj, "title", "");

						if (!sourceSlug.isEmpty() &&
							candidateSlug.compare(sourceSlug,
												  Qt::CaseInsensitive) == 0) {
							hitId = candidateId;
							hitSlug = candidateSlug;
							hitName = candidateName;
							break;
						}
						if (normalizeName(candidateName) ==
							normalizeName(projectName)) {
							hitId = candidateId;
							hitSlug = candidateSlug;
							hitName = candidateName;
							break;
						}
						// First result is a fallback only without a source slug.
						if (hitId.isEmpty() && sourceSlug.isEmpty()) {
							hitId = candidateId;
							hitSlug = candidateSlug;
							hitName = candidateName;
						}
					}
				}
			} else {
				if (doc.isObject() && doc.object().contains("data")) {
					auto dataArr = Json::ensureArray(doc.object(), "data");
					for (auto hitRaw : dataArr) {
						auto hitObj = hitRaw.toObject();
						QString candidateSlug =
							Json::ensureString(hitObj, "slug", "");
						QString candidateId = QString::number(
							Json::ensureInteger(hitObj, "id", 0));
						QString candidateName =
							Json::ensureString(hitObj, "name", "");

						if (!sourceSlug.isEmpty() &&
							candidateSlug.compare(sourceSlug,
												  Qt::CaseInsensitive) == 0) {
							hitId = candidateId;
							hitSlug = candidateSlug;
							hitName = candidateName;
							break;
						}
						if (normalizeName(candidateName) ==
							normalizeName(projectName)) {
							hitId = candidateId;
							hitSlug = candidateSlug;
							hitName = candidateName;
							break;
						}
						if (hitId.isEmpty() && sourceSlug.isEmpty()) {
							hitId = candidateId;
							hitSlug = candidateSlug;
							hitName = candidateName;
						}
					}
				}
			}

			// Source slug given but no slug matched: ambiguous, reject.
			if (!hitId.isEmpty() && !sourceSlug.isEmpty() &&
				hitSlug.isEmpty()) {
				qWarning() << "Cross resolve rejected ambiguous match for"
						   << projectName << "- source slug:" << sourceSlug
						   << "didn't match any result";
				hitId.clear();
			}

			if (!hitId.isEmpty()) {
				QString key = targetPlatform + ":" + hitId;
				if (!m_resolvedProjectIds.contains(key)) {
					m_resolvedProjectIds.insert(key);
					m_resolvedNames.insert(normalizeName(projectName));
					qDebug() << "Cross resolved to" << targetPlatform
							 << "project" << hitId << "slug:" << hitSlug
							 << "(source slug:" << sourceSlug << ")";

					ModPlatform::VersionQuery crossQuery;
					crossQuery.projectId = hitId;
					crossQuery.contentType = ModPlatform::ContentType::Mod;
					crossQuery.mcVersions =
						ModPlatform::singleVersionList(m_mcVersion);
					crossQuery.loaders =
						ModPlatform::singleLoaderList(m_loader);
					const QUrl fileUrl =
						(targetPlatform == "modrinth")
							? ModrinthApi::get().projectVersionsUrl(crossQuery)
							: FlameApi::get().projectVersionsUrl(crossQuery);

					request(QString("CrossDepResolve(%1)").arg(hitId), fileUrl,
							[this, targetPlatform, hitId, projectName, requiredBy,
							 wantedId](const QByteArray& depBytes) {
								if (depBytes.isEmpty()) {
									ModPlatform::UnresolvedDep unresolved;
									unresolved.name = projectName;
									m_unresolvedDeps.append(unresolved);
									return;
								}
								onDependencyProjectResolved(
									targetPlatform, hitId, depBytes,
									requiredBy, wantedId);
							});
				} else {
					noteAlsoRequiredBy(targetPlatform, hitId, requiredBy,
									   wantedId);
					bool wasResolved = false;
					for (const auto& dep : m_dependencies) {
						if ((dep.platform + ":" + dep.projectId) == key) {
							wasResolved = true;
							break;
						}
					}
					if (!wasResolved) {
						qWarning() << "Dependency" << projectName
								   << "could not be resolved on any platform";
						ModPlatform::UnresolvedDep unresolved;
						unresolved.name = projectName;
						/* Lets checkCompletion() drop it if the lookup in flight records it. */
						unresolved.platform = targetPlatform;
						unresolved.projectId = hitId;
						m_unresolvedDeps.append(unresolved);
					}
				}
			} else {
				qWarning() << "Cross resolve search returned no valid match for"
						   << projectName;
				ModPlatform::UnresolvedDep unresolved;
				unresolved.name = projectName;
				m_unresolvedDeps.append(unresolved);
			}
		});
}

void DependencyResolver::checkCompletion()
{
	if (m_aborted || !isRunning()) {
		/* Already reported (aborted, or succeeded from a re-entrant call);
		 * a second verdict is wrong. */
		return;
	}

	if (m_pendingRequests <= 0 && m_currentModIndex >= m_selectedMods.size()) {
		// Same platform+project resolved at different versions: keep the first.
		{
			QSet<QString> seenKeys;
			QList<ModPlatform::DependencyInfo> deduped;
			for (const auto& dep : m_dependencies) {
				QString key = dep.platform + ":" + dep.projectId;
				if (seenKeys.contains(key)) {
					qDebug()
						<< "Removing duplicate dep (same project, version "
						   "conflict):"
						<< dep.name << dep.versionId << "on" << dep.platform;
					continue;
				}
				seenKeys.insert(key);
				deduped.append(dep);
			}
			m_dependencies = deduped;
		}

		// Same library from both sites: keep one, preferring one with a SHA1.
		{
			QMap<QString, int>
				nameToIndex; // normalized name -> index in deduped list
			QList<ModPlatform::DependencyInfo> deduped;
			for (const auto& dep : m_dependencies) {
				QString normalized = normalizeName(dep.name);
				if (normalized.isEmpty()) {
					deduped.append(dep);
					continue;
				}
				if (nameToIndex.contains(normalized)) {
					int existingIdx = nameToIndex[normalized];
					const auto& existing = deduped[existingIdx];
					bool replaceExisting =
						existing.sha1.isEmpty() && !dep.sha1.isEmpty();
					if (replaceExisting) {
						qDebug() << "Cross-platform dedup: replacing"
								 << existing.name << "(" << existing.platform
								 << ") with" << dep.name << "(" << dep.platform
								 << ") - better metadata";
						deduped[existingIdx] = dep;
					} else {
						qDebug() << "Cross-platform dedup: skipping duplicate"
								 << dep.name << "(" << dep.platform
								 << ") - already have" << existing.name << "("
								 << existing.platform << ")";
					}
				} else {
					nameToIndex.insert(normalized, deduped.size());
					deduped.append(dep);
				}
			}
			m_dependencies = deduped;
		}

		// Same file name reached by different paths.
		{
			QSet<QString> seenFiles;
			QList<ModPlatform::DependencyInfo> deduped;
			for (const auto& dep : m_dependencies) {
				if (!dep.fileName.isEmpty() &&
					seenFiles.contains(dep.fileName)) {
					qDebug() << "Removing dep with duplicate filename:"
							 << dep.fileName;
					continue;
				}
				if (!dep.fileName.isEmpty()) {
					seenFiles.insert(dep.fileName);
				}
				deduped.append(dep);
			}
			m_dependencies = deduped;
		}

		// Against selected mods, by name and by platform+project: CurseForge
		// entries carry the file's displayName, so a name match alone misses it.
		{
			QList<ModPlatform::DependencyInfo> deduped;
			for (const auto& dep : m_dependencies) {
				QString normalized = normalizeName(dep.name);
				bool isDuplicate = false;
				for (const auto& selected : m_selectedMods) {
					const bool sameProject =
						!dep.platform.isEmpty() && !dep.projectId.isEmpty() &&
						selected.platform == dep.platform &&
						selected.projectId == dep.projectId;
					const bool sameName =
						!normalized.isEmpty() &&
						normalizeName(selected.name) == normalized;
					if (sameProject || sameName) {
						qDebug() << "Removing dep that duplicates selected mod:"
								 << dep.name << "(" << dep.platform
								 << dep.projectId << ") vs selected"
								 << selected.name << "(" << selected.platform
								 << selected.projectId << ")";
						isDuplicate = true;
						break;
					}
				}
				if (!isDuplicate) {
					deduped.append(dep);
				}
			}
			m_dependencies = deduped;
		}

		// Drop unresolved entries that were resolved after all, and duplicates.
		QList<ModPlatform::UnresolvedDep> finalUnresolved;
		QSet<QString> seenNames;
		for (const auto& unresolved : m_unresolvedDeps) {
			if (seenNames.contains(unresolved.name))
				continue;
			bool wasResolved = false;
			for (const auto& dep : m_dependencies) {
				if (dep.name == unresolved.name ||
					dep.projectId == unresolved.projectId ||
					normalizeName(dep.name) == normalizeName(unresolved.name)) {
					wasResolved = true;
					break;
				}
			}
			if (!wasResolved) {
				seenNames.insert(unresolved.name);
				finalUnresolved.append(unresolved);
			}
		}
		m_unresolvedDeps = finalUnresolved;

		if (!m_unresolvedDeps.isEmpty()) {
			qWarning() << "Unresolved dependencies:";
			for (const auto& u : m_unresolvedDeps) {
				qWarning() << "  -" << u.name << "(" << u.platform
						   << u.projectId << ")";
			}
		}

		qDebug() << "Dependency resolution complete:" << m_dependencies.size()
				 << "resolved," << m_unresolvedDeps.size() << "unresolved";

		emitSucceeded();
	}
}
