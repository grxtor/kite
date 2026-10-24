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
#include <QDir>
#include <QString>
#include <QStringList>

class ModMetadataIndex;

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
	/// File names in @p folder (.jar, enabled or not) that @p index has no
	/// record of.
	QStringList looseJars(const QDir& folder, const ModMetadataIndex& index);
	/// The SHA-1 of each of @p files in @p folder, skipping unreadable ones.
	QStringList sha1sOf(const QDir& folder, const QStringList& files);
	/// The body of Modrinth's bulk hash lookup.
	QByteArray hashLookupBody(const QStringList& sha1s);
	/// The project ids in a bulk hash lookup's answer; empty for anything
	/// that is not one.
	QStringList projectsInHashReply(const QByteArray& reply);
	/// KitePerformanceExcluded is stored comma-joined: the config file
	/// keeps strings, not lists.
	QStringList parseExcluded(const QString& stored);
	QString joinExcluded(const QStringList& projects);
} // namespace PerformanceSetup
