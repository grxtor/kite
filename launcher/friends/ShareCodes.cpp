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

#include "ShareCodes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

namespace ShareCodes
{
	namespace Kind
	{
		const QString Mod = QStringLiteral("mod");
		const QString ResourcePack = QStringLiteral("resourcepack");
		const QString ShaderPack = QStringLiteral("shaderpack");
		const QString DataPack = QStringLiteral("datapack");
	} // namespace Kind

	namespace Platform
	{
		const QString Modrinth = QStringLiteral("modrinth");
		const QString CurseForge = QStringLiteral("curseforge");
	} // namespace Platform

	namespace
	{
		const QString kPrefix = QStringLiteral("KITE");
		const QString kVanilla = QStringLiteral("vanilla");
		const QStringList kLoaders = {"fabric", "quilt", "forge", "neoforge",
									  kVanilla};
		constexpr int kGroupLength = 4;
		/* CurseForge ids are 32-bit. */
		constexpr qint64 kMaxCurseForgeId = 2147483647;
		const QRegularExpression kVersionPattern(
			QRegularExpression::anchoredPattern(
				QStringLiteral("[A-Za-z0-9._+ -]{1,32}")));
		const QRegularExpression kLoaderVersionPattern(
			QRegularExpression::anchoredPattern(
				QStringLiteral("[A-Za-z0-9._+-]{1,64}")));
		const QRegularExpression kModrinthIdPattern(
			QRegularExpression::anchoredPattern(
				QStringLiteral("[A-Za-z0-9]{1,16}")));
		/* Control and bidi characters, which the server strips from a name. */
		const QRegularExpression kUnsafeChars(QStringLiteral(
			"[\\x{0000}-\\x{001F}\\x{007F}-\\x{009F}\\x{200E}\\x{200F}"
			"\\x{202A}-\\x{202E}\\x{2066}-\\x{2069}]"));

		bool matches(const QRegularExpression& pattern, const QString& text)
		{
			return pattern.match(text).hasMatch();
		}

		bool isCurseForgeId(const QString& id)
		{
			bool ok = false;
			const qint64 value = id.toLongLong(&ok);
			return ok && value >= 1 && value <= kMaxCurseForgeId;
		}

		QJsonObject entryJson(const Entry& entry)
		{
			if (entry.platform == Platform::Modrinth) {
				return {{"kind", entry.kind},
						{"platform", entry.platform},
						{"projectId", entry.projectId},
						{"versionId", entry.fileId}};
			}
			return {{"kind", entry.kind},
					{"platform", entry.platform},
					{"projectId", entry.projectId.toLongLong()},
					{"fileId", entry.fileId.toLongLong()}};
		}

		std::optional<Entry> parseEntry(const QJsonObject& object)
		{
			Entry entry;
			entry.kind = object.value("kind").toString();
			entry.platform = object.value("platform").toString();
			const bool modrinth = entry.platform == Platform::Modrinth;
			const QJsonValue project = object.value("projectId");
			const QJsonValue file =
				object.value(modrinth ? "versionId" : "fileId");
			entry.projectId = modrinth ? project.toString()
									   : QString::number(project.toInteger());
			entry.fileId = modrinth ? file.toString()
									: QString::number(file.toInteger());
			if (!isValid(entry)) {
				return std::nullopt;
			}
			return entry;
		}
	} // namespace

	QString cleanName(const QString& name)
	{
		return QString(name).remove(kUnsafeChars).trimmed().left(kMaxNameLength);
	}

	bool isValid(const Entry& entry)
	{
		static const QStringList kinds = {Kind::Mod, Kind::ResourcePack,
										  Kind::ShaderPack, Kind::DataPack};
		if (!kinds.contains(entry.kind)) {
			return false;
		}
		if (entry.platform == Platform::Modrinth) {
			return matches(kModrinthIdPattern, entry.projectId) &&
				   matches(kModrinthIdPattern, entry.fileId);
		}
		return entry.platform == Platform::CurseForge &&
			   isCurseForgeId(entry.projectId) && isCurseForgeId(entry.fileId);
	}

	QString normalize(const QString& raw)
	{
		static const QRegularExpression separators(QStringLiteral("[\\s-]"));
		static const QRegularExpression body(
			QRegularExpression::anchoredPattern(
				QStringLiteral("[%1]{%2}")
					.arg(QLatin1String(kAlphabet))
					.arg(kCodeLength)));
		QString compact = raw.toUpper().remove(separators);
		/* I is not in the alphabet, so no code starts with KITE by itself. */
		if (compact.startsWith(kPrefix)) {
			compact = compact.mid(kPrefix.size());
		}
		return matches(body, compact) ? compact : QString();
	}

	QString display(const QString& stored)
	{
		const QString code = normalize(stored);
		if (code.isEmpty()) {
			return QString();
		}
		return kPrefix + QLatin1Char('-') + code.left(kGroupLength) +
			   QLatin1Char('-') + code.mid(kGroupLength);
	}

	namespace Path
	{
		QString codes()
		{
			return QStringLiteral("/v1/share-codes");
		}
		QString code(const QString& code)
		{
			return codes() + QLatin1Char('/') + code;
		}
	} // namespace Path

	QByteArray manifestBody(const Manifest& manifest)
	{
		const bool vanilla = manifest.loader == kVanilla;
		QJsonObject loader{{"type", manifest.loader}};
		if (!vanilla) {
			loader.insert("version", manifest.loaderVersion);
		}
		QJsonArray content;
		QSet<QString> seen;
		for (const Entry& entry : manifest.content) {
			const QString key =
				entry.platform + ':' + entry.kind + ':' + entry.projectId;
			if (!isValid(entry) || seen.contains(key) ||
				(vanilla && entry.kind == Kind::Mod) ||
				content.size() >= kMaxEntries) {
				continue;
			}
			seen.insert(key);
			content.append(entryJson(entry));
		}
		const QJsonObject body{{"name", cleanName(manifest.name)},
							   {"minecraftVersion", manifest.minecraftVersion},
							   {"loader", loader},
							   {"content", content}};
		return QJsonDocument(body).toJson(QJsonDocument::Compact);
	}

	std::optional<Manifest> parseManifest(const QJsonObject& object)
	{
		Manifest manifest;
		manifest.name = cleanName(object.value("name").toString());
		manifest.minecraftVersion = object.value("minecraftVersion").toString();
		const QJsonObject loader = object.value("loader").toObject();
		manifest.loader = loader.value("type").toString();
		manifest.loaderVersion = loader.value("version").toString();
		const bool vanilla = manifest.loader == kVanilla;
		if (manifest.name.isEmpty() ||
			!matches(kVersionPattern, manifest.minecraftVersion) ||
			!kLoaders.contains(manifest.loader) ||
			(!vanilla && !matches(kLoaderVersionPattern, manifest.loaderVersion)) ||
			!object.value("content").isArray()) {
			return std::nullopt;
		}
		if (vanilla) {
			manifest.loaderVersion.clear();
		}
		for (const QJsonValue& value : object.value("content").toArray()) {
			const auto entry = parseEntry(value.toObject());
			if (!entry || (vanilla && entry->kind == Kind::Mod)) {
				return std::nullopt;
			}
			manifest.content.append(*entry);
		}
		return manifest;
	}
} // namespace ShareCodes
