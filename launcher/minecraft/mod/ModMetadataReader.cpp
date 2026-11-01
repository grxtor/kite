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

#include "ModMetadataReader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

#include <algorithm>

#include "MMCZip.h"
#include "ModMetadataFormats.h"

using namespace ModMetadata;
using namespace ModMetadata::detail;

namespace
{

	// Every metadata file a jar may carry; all that the first pass looks at.
	const QString kModsToml = QStringLiteral("META-INF/mods.toml");
	const QString kNeoForgeModsToml =
		QStringLiteral("META-INF/neoforge.mods.toml");
	const QString kMcmodInfo = QStringLiteral("mcmod.info");
	const QString kFabricModJson = QStringLiteral("fabric.mod.json");
	const QString kQuiltModJson = QStringLiteral("quilt.mod.json");
	const QString kForgeVersionProps =
		QStringLiteral("forgeversion.properties");
	const QString kJarJarMetadata =
		QStringLiteral("META-INF/jarjar/metadata.json");
	const QString kManifest = QStringLiteral("META-INF/MANIFEST.MF");
	// Sinytra Connector's way of naming the mod jar it carries (no mods.toml).
	const QByteArray kEmbeddedModAttribute("Embedded-Dependencies-Mod");

	const QString kJarVersionPlaceholder = QStringLiteral("${file.jarVersion}");

	// Bundling goes one level deep, sometimes two; the cap stops archives of
	// archives from being unpacked without end.
	constexpr int kMaxNestedJarDepth = 3;

	// One attribute of a jar's MANIFEST.MF by case-insensitive name; empty if
	// absent. Main section only. A line starting with one space continues the
	// previous one (the format wraps at 72 bytes, mid-word).
	QString manifestAttribute(const QByteArray& manifest,
							  const QByteArray& name)
	{
		QList<QByteArray> lines;
		for (QByteArray line : manifest.split('\n')) {
			if (line.endsWith('\r')) {
				line.chop(1);
			}
			if (line.isEmpty()) {
				if (lines.isEmpty()) {
					continue;
				}
				break;
			}
			if (line.startsWith(' ') && !lines.isEmpty()) {
				lines.last().append(line.mid(1));
				continue;
			}
			lines.append(line);
		}

		const QByteArray key = name.toLower() + ':';
		for (const QByteArray& line : lines) {
			if (line.left(key.size()).toLower() == key) {
				return QString::fromUtf8(line.mid(key.size())).trimmed();
			}
		}
		return {};
	}

	// What ${file.jarVersion} stands for: the jar's Implementation-Version, as
	// Forge fills it in. Empty without a manifest; the placeholder stays as
	// written.
	QString manifestVersion(const QByteArray& manifest)
	{
		if (manifest.isEmpty()) {
			return {};
		}
		const QString version =
			manifestAttribute(manifest, "Implementation-Version");

		// some mods use ${projectversion} in their build.gradle, causing this
		// mess to show up in MANIFEST.MF also keep with forge's behavior of
		// setting the version to "NONE" if none is found
		if (version.contains(QLatin1String(
				"task ':jar' property 'archiveVersion'")) ||
			version.isEmpty()) {
			return QStringLiteral("NONE");
		}
		return version;
	}

	void substituteJarVersion(ModDetails& details, const QString& jarVersion)
	{
		if (jarVersion.isEmpty()) {
			return;
		}
		if (details.version == kJarVersionPlaceholder) {
			details.version = jarVersion;
		}
		for (ProvidedMod& mod : details.provides) {
			if (mod.version == kJarVersionPlaceholder) {
				mod.version = jarVersion;
			}
		}
	}

	// The same declaration twice, as in a jar with both mods.toml and
	// neoforge.mods.toml; only one applies.
	void addDependency(ModDetails& details, const ModDependency& dependency)
	{
		const auto same = [&dependency](const ModDependency& other) {
			return other.id == dependency.id && other.kind == dependency.kind &&
				   other.syntax == dependency.syntax &&
				   other.versions == dependency.versions &&
				   other.loader == dependency.loader;
		};
		if (std::none_of(details.dependencies.cbegin(),
						 details.dependencies.cend(), same)) {
			details.dependencies.append(dependency);
		}
	}

	// One mod from a jar's metadata files, in the order they decide what it is
	// shown as: the first that parsed supplies name, version and display
	// fields; loaders, dependencies and provided ids of all are added together.
	DetailsPtr mergeMetadata(const QList<DetailsPtr>& parsed)
	{
		DetailsPtr merged;
		for (const DetailsPtr& details : parsed) {
			if (!details) {
				continue;
			}
			if (!merged) {
				merged = details;
				continue;
			}
			for (const QString& loader : details->loaders) {
				if (!merged->loaders.contains(loader)) {
					merged->loaders.append(loader);
				}
			}
			for (const ModDependency& dependency : details->dependencies) {
				addDependency(*merged, dependency);
			}
			for (const ProvidedMod& mod : details->provides) {
				addProvided(*merged, mod);
			}
		}
		return merged;
	}

	// The jars a mod carries, as paths into its archive: fabric/quilt "jars",
	// Forge's META-INF/jarjar, the manifest's Embedded-Dependencies-Mod.
	QStringList nestedJarPaths(const ZipEntries& files)
	{
		QStringList paths;
		const auto add = [&paths](const QString& path) {
			const QString entry = archivePath(path);
			if (!entry.isEmpty() && !paths.contains(entry)) {
				paths << entry;
			}
		};

		const QJsonObject fabric =
			QJsonDocument::fromJson(files.value(kFabricModJson)).object();
		for (const QJsonValue& jar : fabric.value("jars").toArray()) {
			add(jar.toObject().value("file").toString());
		}
		const QJsonObject quilt =
			quiltLoaderObject(files.value(kQuiltModJson));
		for (const QJsonValue& jar : quilt.value("jars").toArray()) {
			add(jar.toString());
		}
		const QJsonObject jarJar =
			QJsonDocument::fromJson(files.value(kJarJarMetadata)).object();
		for (const QJsonValue& jar : jarJar.value("jars").toArray()) {
			add(jar.toObject().value("path").toString());
		}
		add(manifestAttribute(files.value(kManifest), kEmbeddedModAttribute));
		return paths;
	}

	DetailsPtr readModDetails(const ZipEntries& files, const EntryReader& read,
							  int depth);

	// Bundled mods are there at runtime, so they and what they provide count as
	// present (Fabric API is a few dozen). They do not change how the outer
	// file is described.
	void addNestedMods(ModDetails& details, const ZipEntries& files,
					   const EntryReader& read, int depth)
	{
		const QStringList paths = nestedJarPaths(files);
		if (paths.isEmpty()) {
			return;
		}
		const ZipEntries jars = read(paths);
		for (const QString& path : paths) {
			const QByteArray jar = jars.value(path);
			const EntryReader readNested = [&jar](const QStringList& names) {
				return MMCZip::readFilesFromZipData(jar, names);
			};
			const DetailsPtr nested =
				readModDetails(readNested(kMetadataEntries), readNested,
							   depth + 1);
			if (!nested) {
				continue;
			}
			addProvided(details, {nested->mod_id, nested->version});
			for (const ProvidedMod& mod : nested->provides) {
				addProvided(details, mod);
			}
		}
	}

	// Everything a jar's metadata says, from its metadata files
	// (kMetadataEntries) and a way to read more files from the jar. A jar with
	// none of its own (kotlinforforge-all.jar) is described by what it
	// provides. Null with no metadata and no mod inside.
	DetailsPtr readModDetails(const ZipEntries& files, const EntryReader& read,
							  int depth)
	{
		const auto parsed = [&files](const QString& entry, auto readFile) {
			const QByteArray contents = files.value(entry);
			return contents.isEmpty() ? DetailsPtr() : readFile(contents);
		};
		DetailsPtr details = mergeMetadata(
			{parsed(kModsToml,
					[](const QByteArray& contents) {
						return ReadMCModTOML(contents, false);
					}),
			 parsed(kNeoForgeModsToml,
					[](const QByteArray& contents) {
						return ReadMCModTOML(contents, true);
					}),
			 parsed(kMcmodInfo, ReadMCModInfo),
			 parsed(kFabricModJson, ReadFabricModInfo),
			 parsed(kQuiltModJson, ReadQuiltModInfo),
			 parsed(kForgeVersionProps, ReadForgeInfo)});
		const bool hasMetadata = bool(details);
		if (hasMetadata) {
			substituteJarVersion(*details,
								 manifestVersion(files.value(kManifest)));
		} else {
			details = std::make_shared<ModDetails>();
		}

		if (depth < kMaxNestedJarDepth) {
			addNestedMods(*details, files, read, depth);
		}
		if (!hasMetadata && details->provides.isEmpty()) {
			return nullptr;
		}
		return details;
	}

} // namespace

namespace ModMetadata
{

	const QStringList kMetadataEntries = {
		kModsToml,		   kNeoForgeModsToml,  kMcmodInfo,
		kFabricModJson,	   kQuiltModJson,	   kForgeVersionProps,
		kJarJarMetadata,   kManifest};

	// Metadata names its logo relative to the archive root, not always as the
	// archive spells it: mcmod.info often writes "/assets/x/logo.png" or
	// "./logo.png".
	QString archivePath(QString path)
	{
		path = path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
		while (path.startsWith(QLatin1Char('/')) ||
			   path.startsWith(QLatin1String("./"))) {
			path.remove(0, path.startsWith(QLatin1Char('/')) ? 1 : 2);
		}
		return path;
	}

	DetailsPtr readDetails(const ZipEntries& files, const EntryReader& read)
	{
		return readModDetails(files, read, 0);
	}

} // namespace ModMetadata
