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
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

/// One build of the Kite mod, as the manifest lists it.
struct KiteModBuild {
	QString minecraft;
	QString version;
	QString minLoader;
	QString file;
	QString url;
	QString sha256;
	qint64 size = 0;
};

/*
 * The decisions behind injecting Kite's own Fabric mod that need no instance
 * and no network, so they can be tested alone. The manifest lives at
 * kManifestPath under the meta server; the jar it names is kept in
 * cacheFolder() and handed to Fabric with -Dfabric.addMods, so it never
 * shows up in the instance's mods folder.
 */
namespace KiteMod
{
	/// Where the manifest sits, below the meta URL and the "general" cache.
	const QString kManifestPath = QStringLiteral("kite/mod/index.json");
	/// The component id of Fabric Loader.
	const QString kFabricLoaderUid = QStringLiteral("net.fabricmc.fabric-loader");

	/// The builds in a manifest. Nothing for anything that is not a manifest
	/// or has a formatVersion this launcher does not know; entries that are
	/// incomplete or unsafe (not https, a bad checksum, a file name with a
	/// path in it) are skipped.
	std::optional<QList<KiteModBuild>> parseManifest(const QByteArray& json);
	/// The build to use: made for exactly @p mcVersion, needing a loader no
	/// newer than @p loaderVersion, and the highest mod version of those.
	std::optional<KiteModBuild> pickBuild(const QList<KiteModBuild>& builds,
										  const QString& mcVersion,
										  const QString& loaderVersion);
	/// Whether the mod can be loaded: switched on, on Fabric (Quilt is a
	/// different loader) with a known loader version.
	bool appliesTo(bool enabled, const QString& primaryLoader,
				   const QString& loaderVersion);
	/// The -Dfabric.addMods argument to append after @p userJvmArgs. The JVM
	/// keeps the last of two, so a value the player already set is carried
	/// over in front of @p jarPath instead of being overridden.
	QString addModsArgument(const QStringList& userJvmArgs,
							const QString& jarPath);

	/// The folder that holds the downloaded jars, inside the "general" cache
	/// at @p cacheRoot.
	QString cacheFolder(const QString& cacheRoot);
	/// Where @p build's jar is kept.
	QString jarPath(const QString& cacheRoot, const KiteModBuild& build);
	/// Whether @p build's jar is on disk with the size the manifest says.
	bool isDownloaded(const QString& cacheRoot, const KiteModBuild& build);
	/// The best build of the manifest cached under @p cacheRoot, if any.
	std::optional<KiteModBuild> cachedBuild(const QString& cacheRoot,
											const QString& mcVersion,
											const QString& loaderVersion);
	/// The Kite mod jars in @p names other than @p keep: the old versions to
	/// delete once a new one is in place.
	QStringList outdatedJars(const QStringList& names, const QString& keep);
} // namespace KiteMod
