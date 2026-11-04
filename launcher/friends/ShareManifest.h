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

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

#include "ShareCodes.h"

class MinecraftInstance;

/*
 * What a share code of an instance carries, worked out from the instance:
 * its Minecraft version and loader, and every content file whose sidecar
 * (ModMetadataIndex) names its Modrinth or CurseForge project and version.
 * The rest (files added by hand, files turned off) is listed as skipped, so
 * the person sharing sees what the other side will not get.
 */
namespace ShareManifest
{
	/// One content folder of the instance and the manifest kind of its files.
	struct Folder {
		QString kind;
		QString path;
	};

	struct Source {
		QString name;
		QString minecraftVersion;
		/// ShareCodes::Manifest::loader; empty for a loader codes cannot name.
		QString loader;
		QString loaderVersion;
		/// The loader's brand name when `loader` is empty ("LiteLoader").
		QString unsupportedLoader;
		QList<Folder> folders;
	};

	struct Item {
		QString name;
		QString fileName;
		QString kind;
		QString platform;
	};

	struct Skipped {
		enum class Reason { NoSource, TurnedOff };
		QString name;
		QString fileName;
		QString kind;
		Reason reason = Reason::NoSource;
		/// The SHA-1 the sidecar recorded, for a lookup by hash (a file from an
		/// .mrpack names its project but not its version); empty if none.
		QString sha1;
	};

	struct Built {
		ShareCodes::Manifest manifest;
		QList<Item> included;
		QList<Skipped> skipped;
	};

	/// The instance's version, loader and content folders; empty for an
	/// instance without a Minecraft version.
	std::optional<Source> sourceOf(MinecraftInstance& instance);

	/// Reads each folder against its sidecars. Items are sorted by name.
	Built build(const Source& source);

	/// The SHA-1s of the skipped files that a lookup could still name.
	QStringList hashesToLookUp(const Built& built);
	/// Moves the skipped files Modrinth knows by hash into the manifest.
	/// @p answer is the reply to POST version_files: SHA-1 -> version object.
	void applyHashLookup(Built& built, const QJsonObject& answer);

	/// For QML: name, minecraftVersion, loader, loaderVersion, included
	/// [{name, kind, platform}], skipped [{name, kind, reason ("noSource" |
	/// "turnedOff")}], and the included counts mods, resourcePacks,
	/// shaderPacks, dataPacks.
	QVariantMap toMap(const Built& built);
} // namespace ShareManifest
