/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "LoaderInstaller.h"

#include <QStringList>
#include <QVariantMap>

#include "Version.h"
#include "core/LauncherContext.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/Component.h"
#include "minecraft/PackProfile.h"
#include "models/NewInstanceController.h"

LoaderInstaller::LoaderInstaller(PackProfile* profile, QObject* parent)
	: QObject(parent), m_profile(profile),
	  // Parented to `this`: a parentless QObject handed to QML through a property can
	  // be garbage-collected from under the unique_ptr. The unique_ptr still runs
	  // first on destruction (member before base), so there is no double free.
	  m_versions(std::make_unique<LoaderVersionListProxy>(this))
{
}

LoaderInstaller::~LoaderInstaller() = default;

QVariantList LoaderInstaller::loaders() const
{
	QVariantList out;
	for (const ModLoaderInfo& loader : knownModLoaders()) {
		QVariantMap entry;
		entry[QStringLiteral("uid")] = loader.uid;
		entry[QStringLiteral("brandName")] = loader.brandName;
		entry[QStringLiteral("iconName")] = loader.iconName;
		out.append(entry);
	}
	return out;
}

QObject* LoaderInstaller::versions() const
{
	return m_versions.get();
}

QString LoaderInstaller::installedVersion() const
{
	if (m_selectedUid.isEmpty() || !m_profile) {
		return QString();
	}
	return m_profile->getComponentVersion(m_selectedUid);
}

QString LoaderInstaller::conflictName() const
{
	const ModLoaderInfo* loader = modLoaderForUid(m_selectedUid);
	if (!loader || !m_profile) {
		return QString();
	}
	for (const QString& conflictUid : loader->conflictsWith) {
		Component* conflict = m_profile->getComponent(conflictUid);
		// A disabled or customized component is harmless or not ours to touch (see install()).
		if (conflict && conflict->isEnabled() && !conflict->isCustom()) {
			return conflict->getName();
		}
	}
	return QString();
}

void LoaderInstaller::selectLoader(const QString& uid)
{
	m_selectedUid = uid;
	m_supported = true;
	m_unsupportedReason.clear();

	const ModLoaderInfo* loader = modLoaderForUid(uid);
	if (!loader || !m_profile) {
		m_versions->setSourceModel(nullptr);
		emit selectedUidChanged();
		return;
	}

	const QString mcVersion =
		m_profile->getComponentVersion(QStringLiteral("net.minecraft"));

	/* A stated floor means the metadata cannot rule this loader out itself, so
	 * there is nothing to load at all. */
	if (!loader->earliestMinecraft.isEmpty() &&
		Version(mcVersion) < Version(loader->earliestMinecraft)) {
		m_supported = false;
		m_unsupportedReason =
			tr("%1 does not run on Minecraft %2. The earliest version it "
			   "supports is %3.")
				.arg(loader->brandName, mcVersion, loader->earliestMinecraft);
		m_versions->setSourceModel(nullptr);
		emit selectedUidChanged();
		return;
	}

	/* setMinecraftVersion() before setSourceModel(): the filter is correct when the
	 * new source's rows are first evaluated. setSourceModel() starts the download
	 * if needed and settles `versions.loading`/`versions.error`. */
	auto list = LAUNCHER->metadataIndex()->get(uid);
	m_versions->setMinecraftVersion(mcVersion);
	m_versions->setSourceModel(list.get());

	emit selectedUidChanged();
}

QList<LoaderInstaller::InstallStep> LoaderInstaller::installSequence(
	int conflictCount)
{
	QList<InstallStep> steps;
	for (int i = 0; i < conflictCount; ++i) {
		steps.append(InstallStep::DisableConflict);
	}
	steps.append(InstallStep::EnableSelected);
	steps.append(InstallStep::ChangeVersion);
	return steps;
}

bool LoaderInstaller::install(const QString& versionId)
{
	const ModLoaderInfo* loader = modLoaderForUid(m_selectedUid);
	if (!loader || !m_profile || versionId.isEmpty()) {
		return false;
	}
	if (m_profile->busy()) {
		return false;
	}

	/* Collect conflicts first, so installSequence() can turn their count into the
	 * fixed step order without touching the profile. */
	QStringList conflictsToDisable;
	for (const QString& conflictUid : loader->conflictsWith) {
		Component* conflict = m_profile->getComponent(conflictUid);
		if (conflict && conflict->isEnabled() && !conflict->isCustom()) {
			conflictsToDisable.append(conflictUid);
		}
	}

	/* EnableSelected must come before ChangeVersion (see installSequence()). It is
	 * a no-op for a brand new loader, whose component ChangeVersion creates enabled. */
	for (InstallStep step : installSequence(conflictsToDisable.size())) {
		switch (step) {
			case InstallStep::DisableConflict:
				m_profile->setComponentEnabled(conflictsToDisable.takeFirst(),
											   false);
				break;
			case InstallStep::EnableSelected:
				m_profile->setComponentEnabled(m_selectedUid, true);
				break;
			case InstallStep::ChangeVersion:
				if (!m_profile->changeComponentVersion(m_selectedUid,
													   versionId)) {
					return false;
				}
				break;
		}
	}

	return true;
}
