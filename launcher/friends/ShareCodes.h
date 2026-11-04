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
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>

#include <optional>

/*
 * Share codes (server/friends/README.md, "Share codes"): `KITE-7F3K-9QXM`
 * names a manifest of a Minecraft version, a loader and content picked by
 * Modrinth or CurseForge id, never by URL or file. The requests' bodies and
 * the answers, as plain functions over plain structs.
 */
namespace ShareCodes
{
	/// No 0, O, 1, I or L, as on the server (src/shareCodes.ts).
	inline constexpr char kAlphabet[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
	constexpr int kCodeLength = 8;
	/// What the server keeps: entries in a manifest, characters in its name.
	constexpr int kMaxEntries = 500;
	constexpr int kMaxNameLength = 64;

	namespace Kind
	{
		extern const QString Mod;
		extern const QString ResourcePack;
		extern const QString ShaderPack;
		extern const QString DataPack;
	} // namespace Kind

	namespace Platform
	{
		extern const QString Modrinth;
		extern const QString CurseForge;
	} // namespace Platform

	struct Entry {
		QString kind;
		QString platform;
		QString projectId;
		/// Modrinth's versionId, or CurseForge's fileId as digits.
		QString fileId;

		bool operator==(const Entry& other) const
		{
			return kind == other.kind && platform == other.platform &&
				   projectId == other.projectId && fileId == other.fileId;
		}
	};

	struct Manifest {
		QString name;
		QString minecraftVersion;
		/// "fabric", "quilt", "forge", "neoforge" or "vanilla".
		QString loader;
		/// Empty for vanilla.
		QString loaderVersion;
		QList<Entry> content;
	};

	struct Code {
		QString code;
		/// Only from GET /v1/share-codes/{code}; GET /v1/share-codes has the
		/// name, version and loader without the content.
		Manifest manifest;
		QDateTime createdAt;
		QDateTime expiresAt;
	};

	/// The stored form ("7F3K9QXM") of a code as a person typed it: case,
	/// spaces, dashes and the KITE- prefix do not matter. Empty if it cannot be one.
	QString normalize(const QString& raw);
	/// "KITE-7F3K-9QXM" for "7F3K9QXM".
	QString display(const QString& stored);

	namespace Path
	{
		QString codes();
		/// @p code in its stored form.
		QString code(const QString& code);
	} // namespace Path

	/// @p name without the control and bidi characters the server strips,
	/// trimmed and cut to kMaxNameLength.
	QString cleanName(const QString& name);
	/// Whether the server takes @p entry: a known kind, and ids of the form
	/// its platform uses.
	bool isValid(const Entry& entry);

	/// POST /v1/share-codes. Leaves out what the server would refuse: a
	/// second entry for one project and kind, mods on vanilla, past kMaxEntries.
	QByteArray manifestBody(const Manifest& manifest);
	/// A manifest as the server checks it; empty for one it would refuse.
	std::optional<Manifest> parseManifest(const QJsonObject& object);

	/// The answer to POST /v1/share-codes.
	std::optional<Code> parseCreated(const QByteArray& body);
	/// The answer to GET /v1/share-codes/{code}.
	std::optional<Code> parseFetched(const QByteArray& body);
	/// The answer to GET /v1/share-codes.
	std::optional<QList<Code>> parseList(const QByteArray& body);

	/// For QML: code (as display() writes it), name, minecraftVersion, loader,
	/// loaderVersion, createdAt, expiresAt (ISO), and how many of each kind:
	/// mods, resourcePacks, shaderPacks, dataPacks.
	QVariantMap toMap(const Code& code);
} // namespace ShareCodes
