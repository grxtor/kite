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

#include "ModMetadataFormats.h"

#include <QDebug>
#include <QList>
#include <QString>
#include <QStringList>
#include <toml++/toml.hpp>

#include <optional>
#include <string>
#include <string_view>

using ModMetadata::detail::addProvided;
using ModMetadata::detail::kLoaderForge;
using ModMetadata::detail::kLoaderNeoForge;

namespace
{

	// What an unexpanded build variable looks like.
	const QLatin1String kBuildPlaceholder("${");

	QString tomlString(const toml::table& table, std::string_view key)
	{
		if (auto val = table[key].value<std::string>()) {
			return QString::fromStdString(*val);
		}
		return {};
	}

	/* The [[dependencies.<modid>]] tables of the mods the file declares.
	 * Forge only reads those; a table under any other name is not a
	 * dependency of anything. */
	template <typename Visit>
	void forEachDependencyTable(const toml::table& root,
								const QStringList& modIds, Visit visit)
	{
		const toml::table* byMod = root["dependencies"].as_table();
		if (!byMod) {
			return;
		}
		for (auto&& [declaring, node] : *byMod) {
			const toml::array* list = node.as_array();
			if (!list || !modIds.contains(QString::fromStdString(
							 std::string(declaring.str())))) {
				continue;
			}
			for (const toml::node& element : *list) {
				if (const toml::table* table = element.as_table()) {
					visit(*table);
				}
			}
		}
	}

	/* Forge writes mandatory = true; NeoForge writes type = "required" and
	 * its siblings. Where both are there, type is the newer and decides.
	 * Nothing for what changes nothing about whether the game starts:
	 * optional dependencies.
	 *
	 * A dependency that says neither is required in NeoForge's own file -
	 * that is NeoForge's default, and plenty of mods lean on it (some write
	 * a "required = true" it never reads) - and not in Forge's, where the
	 * loader insists on the flag. */
	std::optional<ModDependency::Kind> tomlDependencyKind(
		const toml::table& dependency, bool neoforgeFile)
	{
		if (auto type = dependency["type"].value<std::string>()) {
			const QString name = QString::fromStdString(*type).toLower();
			if (name == QLatin1String("required")) {
				return ModDependency::Kind::Required;
			}
			if (name == QLatin1String("incompatible")) {
				return ModDependency::Kind::Incompatible;
			}
			if (name == QLatin1String("discouraged")) {
				return ModDependency::Kind::Discouraged;
			}
			return std::nullopt;
		}
		const auto mandatory = dependency["mandatory"].value<bool>();
		if (mandatory ? *mandatory : neoforgeFile) {
			return ModDependency::Kind::Required;
		}
		return std::nullopt;
	}

	QStringList tomlVersionRange(const toml::table& dependency)
	{
		const QString range = tomlString(dependency, "versionRange").trimmed();
		// A ${...} left in the range is a build that never filled it in;
		// there is nothing to compare a version against.
		if (range.isEmpty() || range.contains(kBuildPlaceholder)) {
			return {};
		}
		return {range};
	}

	/* What the file says about mods other than the one it is shown as: the
	 * further [[mods]] it declares, and what every one of them needs. */
	void addTomlRelations(ModDetails& details, const toml::table& root,
						  const toml::array& mods, bool neoforgeFile)
	{
		QList<ProvidedMod> declared;
		for (const toml::node& node : mods) {
			if (const toml::table* mod = node.as_table()) {
				declared.append(
					{tomlString(*mod, "modId"), tomlString(*mod, "version")});
			}
		}
		QStringList ownIds;
		for (const ProvidedMod& mod : declared) {
			ownIds.append(mod.id);
		}
		// The first is the one shown; the rest are only ids it answers to.
		for (qsizetype i = 1; i < declared.size(); ++i) {
			addProvided(details, declared.at(i));
		}

		bool dependsOnNeoForge = false;
		QList<ModDependency> found;
		forEachDependencyTable(root, ownIds, [&](const toml::table& table) {
			const QString id = tomlString(table, "modId");
			const auto kind = tomlDependencyKind(table, neoforgeFile);
			// Only a dependency that has to be met says the file is
			// NeoForge's: Forge's own jars often list neoforge as optional
			// (PacketFixer), and Forge 1.20.1 loads those.
			dependsOnNeoForge |= id == kLoaderNeoForge && kind &&
								 *kind == ModDependency::Kind::Required;
			// One declared mod needing another of the same file is not
			// something to install.
			if (!kind || id.isEmpty() || ownIds.contains(id)) {
				return;
			}
			found.append(ModDependency{id, *kind,
									   ModDependency::Syntax::Maven,
									   tomlVersionRange(table), QString()});
		});

		// A mods.toml that requires neoforge is a NeoForge mod that kept the
		// old file name.
		const QString loader = neoforgeFile || dependsOnNeoForge
								   ? kLoaderNeoForge
								   : kLoaderForge;
		details.loaders = {loader};
		for (ModDependency& dependency : found) {
			dependency.loader = loader;
			details.dependencies.append(dependency);
		}
	}

} // namespace

namespace ModMetadata::detail
{

	// https://github.com/MinecraftForge/Documentation/blob/5ab4ba6cf9abc0ac4c0abd96ad187461aefd72af/docs/gettingstarted/structuring.md
	// neoforge.mods.toml is the same format under another name.
	std::shared_ptr<ModDetails> ReadMCModTOML(QByteArray contents,
											  bool neoforgeFile)
	{
		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();

		toml::table tomlData;
		try {
			tomlData = toml::parse(
				std::string_view(contents.constData(), contents.size()));
		} catch (const toml::parse_error&) {
			return nullptr;
		}

		// array defined by [[mods]]
		auto* tomlModsArr = tomlData["mods"].as_array();
		if (!tomlModsArr) {
			qWarning() << "Corrupted mods.toml? Couldn't find [[mods]] array!";
			return nullptr;
		}

		// we only really care about the first element, since multiple mods in
		// one file is not supported by us at the moment
		auto* tomlModsTable0 = tomlModsArr->get_as<toml::table>(0);
		if (!tomlModsTable0) {
			qWarning() << "Corrupted mods.toml? [[mods]] didn't have an "
						  "element at index 0!";
			return nullptr;
		}

		// mandatory properties - always in [[mods]]
		if (auto val = (*tomlModsTable0)["modId"].value<std::string>()) {
			details->mod_id = QString::fromStdString(*val);
		}
		if (auto val = (*tomlModsTable0)["version"].value<std::string>()) {
			details->version = QString::fromStdString(*val);
		}
		if (auto val = (*tomlModsTable0)["displayName"].value<std::string>()) {
			details->name = QString::fromStdString(*val);
		}
		if (auto val = (*tomlModsTable0)["description"].value<std::string>()) {
			details->description = QString::fromStdString(*val);
		}

		// optional properties - can be in the root table or [[mods]]
		QString authors;
		if (auto val = tomlData["authors"].value<std::string>()) {
			authors = QString::fromStdString(*val);
		} else if (auto val =
					   (*tomlModsTable0)["authors"].value<std::string>()) {
			authors = QString::fromStdString(*val);
		}
		if (!authors.isEmpty()) {
			// author information is stored as a string now, not a list
			details->authors.append(authors);
		}
		// is credits even used anywhere? including this for completion/parity
		// with old data version
		QString credits;
		if (auto val = tomlData["credits"].value<std::string>()) {
			credits = QString::fromStdString(*val);
		} else if (auto val =
					   (*tomlModsTable0)["credits"].value<std::string>()) {
			credits = QString::fromStdString(*val);
		}
		details->credits = credits;
		QString homeurl;
		if (auto val = tomlData["displayURL"].value<std::string>()) {
			homeurl = QString::fromStdString(*val);
		} else if (auto val =
					   (*tomlModsTable0)["displayURL"].value<std::string>()) {
			homeurl = QString::fromStdString(*val);
		}
		if (!homeurl.isEmpty()) {
			// fix up url.
			if (!homeurl.startsWith("http://") &&
				!homeurl.startsWith("https://") &&
				!homeurl.startsWith("ftp://")) {
				homeurl.prepend("http://");
			}
		}
		details->homeurl = homeurl;

		// Per mod in [[mods]]; some older mods.toml files set it once at
		// the root instead.
		if (auto val = (*tomlModsTable0)["logoFile"].value<std::string>()) {
			details->icon = QString::fromStdString(*val);
		} else if (auto val = tomlData["logoFile"].value<std::string>()) {
			details->icon = QString::fromStdString(*val);
		}

		addTomlRelations(*details, tomlData, *tomlModsArr, neoforgeFile);
		return details;
	}

} // namespace ModMetadata::detail
