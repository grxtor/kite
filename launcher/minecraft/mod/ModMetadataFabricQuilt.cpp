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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include "ModIconCache.h"

using ModMetadata::detail::addProvided;
using ModMetadata::detail::kLoaderFabric;
using ModMetadata::detail::kLoaderQuilt;

namespace
{

	/* fabric.mod.json / quilt.mod.json "icon": one path, or a map of pixel
	 * size -> path. Takes the largest no bigger than what the cache keeps,
	 * or failing that the smallest one there is. */
	QString pickIcon(const QJsonValue& value)
	{
		if (value.isString()) {
			return value.toString();
		}
		const QJsonObject sizes = value.toObject();
		QString best;
		int bestSize = 0;
		QString smallest;
		int smallestSize = 0;
		for (auto it = sizes.constBegin(); it != sizes.constEnd(); ++it) {
			bool ok = false;
			const int size = it.key().toInt(&ok);
			const QString path = it.value().toString();
			if (!ok || size <= 0 || path.isEmpty()) {
				continue;
			}
			if (size <= ModIconCache::kMaxExtent && size > bestSize) {
				best = path;
				bestSize = size;
			}
			if (smallestSize == 0 || size < smallestSize) {
				smallest = path;
				smallestSize = size;
			}
		}
		return best.isEmpty() ? smallest : best;
	}

	/* "*" - or nothing at all - is how a version spec says any version will
	 * do. One such alternative makes the whole list mean that, which is
	 * what an empty `versions` says. */
	QStringList collapseAnyVersion(const QStringList& alternatives)
	{
		for (const QString& predicate : alternatives) {
			const QString trimmed = predicate.trimmed();
			if (trimmed.isEmpty() || trimmed == QLatin1String("*")) {
				return {};
			}
		}
		return alternatives;
	}

	/* A fabric.mod.json dependency is a version string or an array of them,
	 * which are alternatives. */
	QStringList fabricVersions(const QJsonValue& value)
	{
		QStringList alternatives;
		if (value.isString()) {
			alternatives << value.toString();
		}
		for (const QJsonValue& alternative : value.toArray()) {
			if (alternative.isString()) {
				alternatives << alternative.toString();
			}
		}
		return collapseAnyVersion(alternatives);
	}

	/* "depends", "breaks" and "conflicts" are all the same shape: a mod id
	 * to the versions it is about. ("recommends" and "suggests" are hints
	 * for the user; the loader does nothing with them.) */
	void addFabricDependencies(ModDetails& details, const QJsonObject& table,
							   ModDependency::Kind kind)
	{
		for (auto it = table.constBegin(); it != table.constEnd(); ++it) {
			details.dependencies.append(ModDependency{
				it.key(), kind, ModDependency::Syntax::Fabric,
				fabricVersions(it.value()), kLoaderFabric});
		}
	}

	/* An id may carry the maven group of its mod ("org.example:thing");
	 * what other mods and the game know it by is the part after it. */
	QString quiltId(const QString& id)
	{
		return id.mid(id.lastIndexOf(QLatin1Char(':')) + 1);
	}

	QStringList quiltVersionAlternatives(const QJsonValue& versions);

	// Predicates that all have to match go in one string, space separated. A
	// predicate that takes any version adds nothing to it.
	QString joinPredicates(const QString& joined, const QString& predicate)
	{
		if (collapseAnyVersion({predicate}).isEmpty()) {
			return joined;
		}
		return joined.isEmpty() ? predicate
								: joined + QLatin1Char(' ') + predicate;
	}

	/* "all": every spec of the list has to match. A spec that is itself a
	 * choice makes one list per alternative. */
	QStringList quiltAllOf(const QJsonArray& specs)
	{
		QStringList joined{QString()};
		for (const QJsonValue& spec : specs) {
			QStringList next;
			for (const QString& predicate : quiltVersionAlternatives(spec)) {
				for (const QString& prefix : joined) {
					next << joinPredicates(prefix, predicate.trimmed());
				}
			}
			joined = next;
		}
		return joined;
	}

	/* A quilt.mod.json "versions": a version spec, a list of them (any may
	 * match), or {"any": [...]} / {"all": [...]} of the same, nested as
	 * deep as the author likes. */
	QStringList quiltVersionAlternatives(const QJsonValue& versions)
	{
		if (versions.isString()) {
			return {versions.toString()};
		}
		if (versions.isArray()) {
			QStringList alternatives;
			for (const QJsonValue& spec : versions.toArray()) {
				alternatives << quiltVersionAlternatives(spec);
			}
			return alternatives;
		}
		const QJsonObject choice = versions.toObject();
		if (choice.contains("any")) {
			return quiltVersionAlternatives(choice.value("any"));
		}
		return quiltAllOf(choice.value("all").toArray());
	}

	/* quilt_loader "depends" / "breaks": a bare id, an object, or an array,
	 * which is a group of which one suffices - needing one of several is
	 * not needing any in particular, so those are left out. */
	void addQuiltDependencies(ModDetails& details, const QJsonArray& entries,
							  ModDependency::Kind kind)
	{
		for (const QJsonValue& entry : entries) {
			QString id;
			QStringList versions;
			if (entry.isString()) {
				id = entry.toString();
			} else if (entry.isObject()) {
				const QJsonObject object = entry.toObject();
				if (object.value("optional").toBool()) {
					continue;
				}
				id = object.value("id").toString();
				versions = collapseAnyVersion(
					quiltVersionAlternatives(object.value("versions")));
			}
			id = quiltId(id);
			if (!id.isEmpty()) {
				details.dependencies.append(
					ModDependency{id, kind, ModDependency::Syntax::Fabric,
								  versions, kLoaderQuilt});
			}
		}
	}

	// A "provides" entry is an id, or {"id": ..., "version": ...}.
	void addQuiltProvided(ModDetails& details, const QJsonArray& entries)
	{
		for (const QJsonValue& entry : entries) {
			if (entry.isString()) {
				addProvided(details,
							{quiltId(entry.toString()), details.version});
			} else if (entry.isObject()) {
				const QJsonObject object = entry.toObject();
				addProvided(details,
							{quiltId(object.value("id").toString()),
							 object.value("version").toString(details.version)});
			}
		}
	}

} // namespace

namespace ModMetadata::detail
{

	// https://fabricmc.net/wiki/documentation:fabric_mod_json
	std::shared_ptr<ModDetails> ReadFabricModInfo(QByteArray contents)
	{
		QJsonParseError jsonError;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
		auto object = jsonDoc.object();
		auto schemaVersion = object.contains("schemaVersion")
								 ? object.value("schemaVersion").toInt(0)
								 : 0;

		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();

		details->mod_id = object.value("id").toString();
		details->version = object.value("version").toString();

		details->name = object.contains("name")
							? object.value("name").toString()
							: details->mod_id;
		details->description = object.value("description").toString();
		details->icon = pickIcon(object.value("icon"));

		if (schemaVersion >= 1) {
			QJsonArray authors = object.value("authors").toArray();
			for (auto author : authors) {
				if (author.isObject()) {
					details->authors.append(
						author.toObject().value("name").toString());
				} else {
					details->authors.append(author.toString());
				}
			}

			if (object.contains("contact")) {
				QJsonObject contact = object.value("contact").toObject();

				if (contact.contains("homepage")) {
					details->homeurl = contact.value("homepage").toString();
				}
			}
		}

		details->loaders = {kLoaderFabric};
		addFabricDependencies(*details, object.value("depends").toObject(),
							  ModDependency::Kind::Required);
		addFabricDependencies(*details, object.value("breaks").toObject(),
							  ModDependency::Kind::Incompatible);
		addFabricDependencies(*details, object.value("conflicts").toObject(),
							  ModDependency::Kind::Discouraged);
		for (const QJsonValue& id : object.value("provides").toArray()) {
			addProvided(*details, {id.toString(), details->version});
		}
		return details;
	}

	QJsonObject quiltLoaderObject(const QByteArray& contents)
	{
		return QJsonDocument::fromJson(contents)
			.object()
			.value("quilt_loader")
			.toObject();
	}

	// https://github.com/QuiltMC/rfcs/blob/main/specification/0002-quilt.mod.json.md
	std::shared_ptr<ModDetails> ReadQuiltModInfo(QByteArray contents)
	{
		const QJsonObject loader = quiltLoaderObject(contents);
		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();
		details->mod_id = loader.value("id").toString();
		details->version = loader.value("version").toString();

		const QJsonObject metadata = loader.value("metadata").toObject();
		details->name = metadata.contains("name")
							? metadata.value("name").toString()
							: details->mod_id;
		details->description = metadata.value("description").toString();
		details->icon = pickIcon(metadata.value("icon"));

		details->loaders = {kLoaderQuilt};
		addQuiltDependencies(*details, loader.value("depends").toArray(),
							 ModDependency::Kind::Required);
		addQuiltDependencies(*details, loader.value("breaks").toArray(),
							 ModDependency::Kind::Incompatible);
		addQuiltProvided(*details, loader.value("provides").toArray());
		return details;
	}

} // namespace ModMetadata::detail
