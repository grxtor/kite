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

#include "ManagedPackController.h"

#include <QDebug>
#include <QUrl>
#include <QVariantMap>
#include <memory>

#include "BaseInstance.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/Download.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"
#include "tasks/TaskWatcher.h"

ManagedPackController::Provider
ManagedPackController::providerFromString(const QString& provider)
{
	const QString normalised = provider.trimmed().toLower();
	if (normalised == QLatin1String("modrinth")) {
		return Provider::Modrinth;
	}
	// "flame" is what the upstream launchers call CurseForge - see
	// ManagedPackPage::providerFromString()'s own comment.
	if (normalised == QLatin1String("curseforge") ||
		normalised == QLatin1String("flame")) {
		return Provider::CurseForge;
	}
	return Provider::Unknown;
}

bool ManagedPackController::isSupported(const BaseInstance* instance)
{
	if (instance == nullptr || !instance->isManagedPack()) {
		return false;
	}
	const Provider provider =
		providerFromString(instance->managedPackProvider());
	if (provider == Provider::Unknown) {
		return false;
	}
	if (provider == Provider::CurseForge &&
		LAUNCHER->settings()->get("CurseForgeAPIKey").toString().isEmpty()) {
		return false;
	}
	return true;
}

ManagedPackController::ManagedPackController(BaseInstance* instance,
											 QObject* parent)
	: QObject(parent), m_instance(instance)
{
	m_provider = providerFromString(m_instance->managedPackProvider());
}

ManagedPackController::~ManagedPackController()
{
	if (m_versionsJob) {
		m_versionsJob->abort();
	}
}

QString ManagedPackController::providerLabel() const
{
	switch (m_provider) {
		case Provider::Modrinth:
			return QStringLiteral("Modrinth");
		case Provider::CurseForge:
			return QStringLiteral("CurseForge");
		case Provider::Unknown:
			break;
	}
	return QString();
}

QString ManagedPackController::packName() const
{
	return m_instance->managedPackName();
}

QString ManagedPackController::packId() const
{
	return m_instance->managedPackId();
}

QString ManagedPackController::installedVersionName() const
{
	return m_instance->managedPackVersionName();
}

QString ManagedPackController::installedVersionId() const
{
	return m_instance->managedPackVersionId();
}

bool ManagedPackController::hasPackId() const
{
	return m_instance->hasManagedPackId();
}

QString ManagedPackController::packUrl() const
{
	const QString packId = m_instance->managedPackId();
	switch (m_provider) {
		case Provider::Modrinth: {
			// Modrinth accepts either the slug or the id here; the slug is
			// preferred (it is what the user would see in a browser) but
			// not always recorded.
			const QString slug = m_instance->managedPackSlug();
			return ModrinthApi::get()
				.projectPageUrl(slug.isEmpty() ? packId : slug)
				.toString();
		}
		case Provider::CurseForge:
			return FlameApi::get().projectPageUrl(packId).toString();
		case Provider::Unknown:
			break;
	}
	return m_instance->managedPackSourceUrl();
}

void ManagedPackController::setLoading(bool loading)
{
	if (m_loading == loading) {
		return;
	}
	m_loading = loading;
	emit loadingChanged();
}

void ManagedPackController::setError(const QString& error)
{
	m_error = error;
	emit errorChanged();
}

void ManagedPackController::fetchVersions()
{
	if (m_loaded || m_loading || !hasPackId()) {
		return;
	}

	const QString packId = m_instance->managedPackId();
	QUrl url;
	switch (m_provider) {
		case Provider::Modrinth: {
			ModPlatform::VersionQuery query;
			query.projectId = packId;
			url = ModrinthApi::get().projectVersionsUrl(query);
			break;
		}
		case Provider::CurseForge: {
			ModPlatform::VersionQuery query;
			query.projectId = packId;
			url = FlameApi::get().projectVersionsUrl(query);
			break;
		}
		case Provider::Unknown:
			setError(tr("Unknown pack provider."));
			return;
	}

	setLoading(true);
	setError(QString());

	auto response = std::make_shared<QByteArray>();
	const quint64 generation = ++m_generation;

	m_versionsJob.reset(new NetJob(QStringLiteral("ManagedPack::Versions(%1)")
									   .arg(packId),
								   LAUNCHER->network()));
	m_versionsJob->addNetAction(
		Net::Download::makeByteArray(url, response.get()));

	connect(m_versionsJob.get(), &NetJob::succeeded, this,
			[this, response, generation] {
				applyVersions(generation, *response);
			});
	connect(m_versionsJob.get(), &NetJob::failed, this,
			[this, generation](const QString& reason) {
				if (generation != m_generation) {
					return;
				}
				setLoading(false);
				setError(reason);
			});

	m_versionsJob->start();
}

void ManagedPackController::reload()
{
	m_loaded = false;
	m_versions.clear();
	m_versionsVariant.clear();
	emit versionsChanged();
	fetchVersions();
}

void ManagedPackController::applyVersions(quint64 generation,
										  const QByteArray& bytes)
{
	if (generation != m_generation) {
		return;
	}
	setLoading(false);

	bool parsed = false;
	switch (m_provider) {
		case Provider::Modrinth:
			m_versions = ManagedPack::parseModrinthVersions(bytes, &parsed);
			break;
		case Provider::CurseForge:
			m_versions = ManagedPack::parseCurseForgeFiles(bytes, &parsed);
			break;
		case Provider::Unknown:
			break;
	}

	if (!parsed || m_versions.isEmpty()) {
		setError(tr("Failed to read the available versions."));
		return;
	}

	if (m_provider == Provider::CurseForge) {
		// CurseForge returns a null downloadUrl for a file whose project
		// opted out of third-party distribution - the common case for
		// modpacks, not the exception. The site's own download route
		// serves those files (see ManagedPackPage::applyVersions()'s own
		// comment), so fall back to it here too.
		const QString packId = m_instance->managedPackId();
		for (auto& version : m_versions) {
			if (version.downloadUrl.isEmpty()) {
				version.downloadUrl =
					FlameApi::browserDownloadUrl(packId, version.versionId);
			}
		}
	}

	m_loaded = true;
	rebuildVariants();
}

void ManagedPackController::rebuildVariants()
{
	const QString installedId = m_instance->managedPackVersionId();
	const QString installedName = m_instance->managedPackVersionName();

	m_versionsVariant.clear();
	for (const auto& version : m_versions) {
		const bool isInstalled =
			(!installedId.isEmpty() && version.versionId == installedId) ||
			(installedId.isEmpty() && !installedName.isEmpty() &&
			 (version.versionNumber == installedName ||
			  version.displayName == installedName));

		QVariantMap row;
		row[QStringLiteral("id")] = version.versionId;
		row[QStringLiteral("label")] = version.label();
		row[QStringLiteral("current")] = isInstalled;
		row[QStringLiteral("installable")] = version.isInstallable();
		// Only ever non-empty for Modrinth (included in the version list
		// reply) - CurseForge's changelog needs a second request per file,
		// which this controller does not make; see the class comment.
		row[QStringLiteral("changelog")] = version.changelog;
		m_versionsVariant.append(row);
	}
	emit versionsChanged();
}

QObject* ManagedPackController::updateToVersion(int index)
{
	if (!hasPackId() || index < 0 || index >= m_versions.size() ||
		m_instance->isRunning()) {
		return nullptr;
	}
	const ManagedPack::Version& version = m_versions.at(index);
	if (!version.isInstallable()) {
		return nullptr;
	}

	const QUrl downloadUrl(version.downloadUrl);
	const QString versionId = version.versionId;
	const QString versionName = version.versionNumber.isEmpty()
									? version.displayName
									: version.versionNumber;

	auto* task = new InstanceImportTask(downloadUrl);

	InstanceImportTask::UpdateTarget target;
	target.instanceId = m_instance->id();
	target.versionId = versionId;
	target.versionLabel = versionName;
	task->setUpdateTarget(target);
	// Straight out of the catalogue's own version list - the launcher
	// chose this download, not the user, so it is trusted the same way
	// ManagedPackPage::update() trusts a catalogue selection.
	task->setTrustedSource(true);

	InstanceImportTask::PackSourceHint hint;
	hint.provider = m_instance->managedPackProvider();
	hint.packId = m_instance->managedPackId();
	hint.packSlug = m_instance->managedPackSlug();
	hint.packName = m_instance->managedPackName();
	hint.sourceUrl = m_instance->managedPackSourceUrl();
	hint.versionId = versionId;
	hint.versionLabel = versionName;
	task->setPackSourceHint(hint);

	task->setGroup(LAUNCHER->instances()->getInstanceGroup(m_instance->id()));
	task->setIcon(m_instance->iconKey());
	// Kept exactly as it is: ManagedPackPage offers to fold the new version
	// into the name too, but only after asking (CustomMessageBox), which
	// this bridge has no dialog to do. Leaving the name alone is the safe
	// default the same comment there argues for either way.
	task->setName(m_instance->name());

	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(task);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(versionName.isEmpty() ? packName() : versionName);
	watcher->setInstanceId(m_instance->id());
	/* Deliberately not refreshed/reloaded from here on success: this
	 * instance has just been replaced on disk (staged over its own id),
	 * and m_instance is a bare pointer borrowed from the InstanceDetails
	 * that owns this controller - continuing to read it, or asking it for
	 * a new version list, risks doing so against a stale object. The
	 * widget page dealt with the same fact by closing its own window; the
	 * QML tab does the equivalent by navigating back to the library once
	 * this watcher succeeds, rather than this controller trying to refresh
	 * itself in place. */
	wrapped->start();
	return watcher;
}
