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

// The decisions behind injecting Kite's Fabric mod that need no instance and no
// network, so they can be tested alone. The manifest is at kManifestPath under
// the meta server; its jar is kept in cacheFolder() and passed with
// -Dfabric.addMods, so it never shows up in the mods folder.
namespace KiteMod
{
	/// Where the manifest sits, below the meta URL and the "general" cache.
	const QString kManifestPath = QStringLiteral("kite/mod/index.json");
	/// The component id of Fabric Loader.
	const QString kFabricLoaderUid = QStringLiteral("net.fabricmc.fabric-loader");

	/// The builds in a manifest. Nothing for a non-manifest or an unknown
	/// formatVersion; incomplete or unsafe entries (not https, bad checksum, a
	/// path in the file name) are skipped.
	std::optional<QList<KiteModBuild>> parseManifest(const QByteArray& json);
	/// The build to use: made for exactly @p mcVersion, needing a loader no
	/// newer than @p loaderVersion, and the highest mod version of those.
	std::optional<KiteModBuild> pickBuild(const QList<KiteModBuild>& builds,
										  const QString& mcVersion,
										  const QString& loaderVersion);
	/// Whether a build can be used for an instance, per the manifest we have.
	enum class Support {
		/// No manifest yet, or the instance's versions are unknown: nothing to
		/// claim.
		Unknown,
		Available,
		/// The manifest has no build made for this Minecraft version.
		NoBuildForMinecraft,
		/// Builds exist for this Minecraft version but all need a newer Fabric
		/// Loader.
		LoaderTooOld
	};
	/// The support for @p mcVersion and @p loaderVersion in @p builds; pass no
	/// builds when there is no manifest.
	Support supportFor(const std::optional<QList<KiteModBuild>>& builds,
					   const QString& mcVersion, const QString& loaderVersion);
	/// Whether the mod can be loaded: on, Fabric (not Quilt) and a known loader
	/// version.
	bool appliesTo(bool enabled, const QString& primaryLoader,
				   const QString& loaderVersion);
	/// The -Dfabric.addMods argument to append after @p userJvmArgs. The JVM
	/// keeps the last of two, so a value the player set is carried over in
	/// front of @p jarPath.
	QString addModsArgument(const QStringList& userJvmArgs,
							const QString& jarPath);

	/// The folder for the downloaded jars, inside the "general" cache at
	/// @p cacheRoot.
	QString cacheFolder(const QString& cacheRoot);
	/// Where @p build's jar is kept.
	QString jarPath(const QString& cacheRoot, const KiteModBuild& build);
	/// Whether @p build's jar is on disk with the size the manifest says.
	bool isDownloaded(const QString& cacheRoot, const KiteModBuild& build);
	/// The builds of the manifest cached under @p cacheRoot; nothing when
	/// there is no usable one.
	std::optional<QList<KiteModBuild>> cachedBuilds(const QString& cacheRoot);
	/// supportFor() over the manifest cached under @p cacheRoot.
	Support cachedSupport(const QString& cacheRoot, const QString& mcVersion,
						  const QString& loaderVersion);
	/// The best build of the manifest cached under @p cacheRoot, if any.
	std::optional<KiteModBuild> cachedBuild(const QString& cacheRoot,
											const QString& mcVersion,
											const QString& loaderVersion);
	/// The Kite mod jars in @p names to delete once a new one is in place:
	/// those no build in @p builds names, other than @p keep. A jar the
	/// manifest still lists may belong to an instance on another version or
	/// launching now, so it stays.
	QStringList outdatedJars(const QStringList& names,
							 const QList<KiteModBuild>& builds,
							 const QString& keep);
} // namespace KiteMod
