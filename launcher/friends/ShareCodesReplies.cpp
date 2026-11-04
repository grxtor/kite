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

/* ShareCodes: the server's answers about codes, and what QML is given. */

#include "ShareCodes.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

namespace ShareCodes
{
	namespace
	{
		QJsonObject objectOf(const QByteArray& body)
		{
			const QJsonDocument doc = QJsonDocument::fromJson(body);
			return doc.isObject() ? doc.object() : QJsonObject();
		}

		QDateTime parseTime(const QJsonValue& value)
		{
			return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
		}

		QString iso(const QDateTime& time)
		{
			return time.isValid() ? time.toUTC().toString(Qt::ISODateWithMs)
								  : QString();
		}

		std::optional<Code> parseCodeObject(const QJsonObject& object)
		{
			Code code;
			code.code = normalize(object.value("code").toString());
			code.createdAt = parseTime(object.value("createdAt"));
			code.expiresAt = parseTime(object.value("expiresAt"));
			if (code.code.isEmpty()) {
				return std::nullopt;
			}
			return code;
		}
	} // namespace

	std::optional<Code> parseCreated(const QByteArray& body)
	{
		return parseCodeObject(objectOf(body));
	}

	std::optional<Code> parseFetched(const QByteArray& body)
	{
		const QJsonObject object = objectOf(body);
		auto code = parseCodeObject(object);
		const auto manifest =
			parseManifest(object.value("manifest").toObject());
		if (!code || !manifest) {
			return std::nullopt;
		}
		code->manifest = *manifest;
		return code;
	}

	std::optional<QList<Code>> parseList(const QByteArray& body)
	{
		const QJsonObject object = objectOf(body);
		if (!object.value("codes").isArray()) {
			return std::nullopt;
		}
		QList<Code> codes;
		for (const QJsonValue& value : object.value("codes").toArray()) {
			const QJsonObject one = value.toObject();
			auto code = parseCodeObject(one);
			if (!code) {
				continue;
			}
			const QJsonObject loader = one.value("loader").toObject();
			code->manifest.name = cleanName(one.value("name").toString());
			code->manifest.minecraftVersion =
				one.value("minecraftVersion").toString();
			code->manifest.loader = loader.value("type").toString();
			code->manifest.loaderVersion = loader.value("version").toString();
			codes.append(*code);
		}
		return codes;
	}

	QVariantMap toMap(const Code& code)
	{
		QHash<QString, int> counts;
		for (const Entry& entry : code.manifest.content) {
			++counts[entry.kind];
		}
		return {{"code", display(code.code)},
				{"name", code.manifest.name},
				{"minecraftVersion", code.manifest.minecraftVersion},
				{"loader", code.manifest.loader},
				{"loaderVersion", code.manifest.loaderVersion},
				{"createdAt", iso(code.createdAt)},
				{"expiresAt", iso(code.expiresAt)},
				{"mods", counts.value(Kind::Mod)},
				{"resourcePacks", counts.value(Kind::ResourcePack)},
				{"shaderPacks", counts.value(Kind::ShaderPack)},
				{"dataPacks", counts.value(Kind::DataPack)}};
	}
} // namespace ShareCodes
