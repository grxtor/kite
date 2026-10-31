/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "minecraft/mod/ModMetadataIndex.h"

/*
 * The decisions behind PerformanceSetupTask that need no instance and no
 * network, so they can be tested alone.
 */
namespace PerformanceSetup
{
	/// @p defaults in order, without @p excluded and @p installed.
	QStringList wantedProjects(const QStringList& defaults,
							   const QStringList& excluded,
							   const QStringList& installed);
	/// Whether @p loader names a mod loader. Kite Performance only installs
	/// mods into an instance that has one: a vanilla instance is never
	/// changed.
	bool hasLoader(const QString& loader);
	/// Whether the performance mods exist for Minecraft @p mcVersion: a
	/// release from 1.16 on, or a numbered release such as 26.1.2. False
	/// for older releases, snapshots and betas.
	bool supportsVersion(const QString& mcVersion);
	/// File names in @p folder (.jar, enabled or not) that @p index does not
	/// record as installed from Modrinth: added by hand, or installed from
	/// CurseForge or recorded as local. Modrinth can identify these by hash,
	/// and only these need it.
	QStringList jarsNotFromModrinth(const QDir& folder,
									const ModMetadataIndex& index);
	/// The SHA-1 of each of @p files in @p folder, skipping unreadable ones.
	QStringList sha1sOf(const QDir& folder, const QStringList& files);
	/// The body of Modrinth's bulk hash lookup.
	QByteArray hashLookupBody(const QStringList& sha1s);
	/// The project ids in a bulk hash lookup's answer; empty for anything
	/// that is not one.
	QStringList projectsInHashReply(const QByteArray& reply);
	/// KitePerformanceExcluded, KitePerformanceIncluded and
	/// KitePerformanceUnmatched are stored comma-joined: the config file
	/// keeps strings, not lists.
	QStringList parseProjects(const QString& stored);
	QString joinProjects(const QStringList& projects);
	/// The project ids of the default mods among @p catalogMods
	/// (BundleCatalog::mods()), in order.
	QStringList defaultProjects(const QVariantList& catalogMods);
	/// What the installed mods are matched to: the Minecraft version and the
	/// loader, as stored in KitePerformanceFor.
	QString matchKey(const QString& mcVersion, const QString& loader);
	/// The files to fetch again after the Minecraft version or the loader
	/// changed, by project id: every file of @p entries recorded from
	/// Modrinth whose project is one of @p catalogProjects, or that was
	/// pulled in as a dependency (Fabric API and the like).
	QHash<QString, QString>
	filesToRematch(const QList<ModMetadataIndex::Entry>& entries,
				   const QStringList& catalogProjects);
	/// @p excluded plus the projects of @p removed that are among
	/// @p defaults, in order and each once: a default the player deleted is
	/// not brought back.
	QStringList withRemoved(const QStringList& excluded,
							const QStringList& removed,
							const QStringList& defaults);
	/// @p included without the projects of @p removed, in order: an extra
	/// the player deleted is not kept installed any more.
	QStringList withoutRemoved(const QStringList& included,
							   const QStringList& removed);
	/// The projects of @p projects that are also in @p among, in the order of
	/// @p projects.
	QStringList commonProjects(const QStringList& projects,
							   const QStringList& among);
	/// The names of the mods of @p projects (Modrinth project ids) that
	/// @p index records from Modrinth, in order: the name it was installed
	/// under, else its file name. A project the folder does not have is left
	/// out.
	QStringList installedNames(const ModMetadataIndex& index,
							   const QStringList& projects);
	/// What KitePerformanceMissing holds: the catalog mods that had no build
	/// for the match key @p matchKey when @p when asked Modrinth about
	/// @p projects. Stored as "<match key>|<seconds since the epoch>|<project
	/// ids, comma-joined>"; "|" is in none of the three.
	QString missingRecord(const QString& matchKey, const QDateTime& when,
						  const QStringList& projects);
	/// The projects of @p record (missingRecord()) still not worth asking
	/// about at @p now: it is for @p matchKey - another version or loader may
	/// have them - and under a day old. Empty for anything else, a record
	/// from the future and one that is not a record.
	QStringList recentlyMissing(const QString& record, const QString& matchKey,
								const QDateTime& now);
} // namespace PerformanceSetup
