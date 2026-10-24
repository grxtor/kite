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

#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QSet>

#include <memory>
#include "minecraft/OpSys.h"
#include "minecraft/Rule.h"
#include "ProblemProvider.h"
#include "Library.h"
#include <meta/JsonFormat.h>

class PackProfile;
class VersionFile;
class LaunchProfile;
struct RuntimeContext;
struct MojangDownloadInfo;
struct MojangAssetIndexInfo;

using VersionFilePtr = std::shared_ptr<VersionFile>;
class VersionFile : public ProblemContainer
{
	friend class MojangVersionFormat;
	friend class OneSixVersionFormat;

  public: /* methods */
	/// Adds this file to @p profile, with the libraries that apply in
	/// @p context.
	void applyTo(LaunchProfile* profile, const RuntimeContext& context);

  public: /* data */
	/// Kite: order hint for this version file if no explicit order is set
	int order = 0;

	/// Kite: human readable name of this package
	QString name;

	/// Kite: package ID of this package
	QString uid;

	/// Kite: version of this package
	QString version;

	/// Kite: DEPRECATED dependency on a Minecraft version
	QString dependsOnMinecraftVersion;

	/// Mojang: DEPRECATED used to version the Mojang version format
	int minimumKiteVersion = -1;

	/// Mojang: DEPRECATED version of Minecraft this is
	QString minecraftVersion;

	/// Mojang: class to launch Minecraft with
	QString mainClass;

	/// Kite: class to launch legacy Minecraft with (embed in a custom window)
	QString appletClass;

	/// Mojang: Minecraft launch arguments (may contain placeholders for
	/// variable substitution)
	QString minecraftArguments;

	/// Mojang: type of the Minecraft version
	QString type;

	/// Mojang: the time this version was actually released by Mojang
	QDateTime releaseTime;

	/// Mojang: DEPRECATED the time this version was last updated by Mojang
	QDateTime updateTime;

	/// Mojang: DEPRECATED asset group to be used with Minecraft
	QString assets;

	/// Kite: list of tweaker mod arguments for launchwrapper
	QStringList addTweakers;

	/// Mojang: list of libraries to add to the version
	QList<LibraryPtr> libraries;

	/// Kite: list of maven files to put in the libraries folder, but not in
	/// classpath
	QList<LibraryPtr> mavenFiles;

	/// The main jar (Minecraft version library, normally)
	LibraryPtr mainJar;

	/// Kite: list of attached traits of this version file - used to enable
	/// features
	QSet<QString> traits;

	/// Kite: list of jar mods added to this version
	QList<LibraryPtr> jarMods;

	/// Kite: list of mods added to this version
	QList<LibraryPtr> mods;

	/**
	 * Kite: set of packages this depends on
	 * NOTE: this is shared with the meta format!!!
	 */
	Meta::RequireSet requirements;

	/**
	 * Kite: set of packages this conflicts with
	 * NOTE: this is shared with the meta format!!!
	 */
	Meta::RequireSet conflicts;

	/// is volatile -- may be removed as soon as it is no longer needed by
	/// something else
	bool m_volatile = false;

  public:
	// Mojang: DEPRECATED list of 'downloads' - client jar, server jar, windows
	// server exe, maybe more.
	QMap<QString, std::shared_ptr<MojangDownloadInfo>> mojangDownloads;

	// Mojang: extended asset index download information
	std::shared_ptr<MojangAssetIndexInfo> mojangAssetIndex;
};
