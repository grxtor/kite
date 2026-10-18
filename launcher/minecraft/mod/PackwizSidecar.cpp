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

#include "PackwizSidecar.h"

#include <QDateTime>
#include <QDebug>
#include <QLatin1Char>
#include <QLatin1String>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <toml++/toml.hpp>

namespace {
	const QLatin1String kSuffix(".pw.toml");

	using ConstNode = toml::node_view<const toml::node>;

	QString nodeString(const ConstNode& node)
	{
		/* Deliberately not toml++'s value_or("") shorthand: that hands
		 * back a pointer into the parsed document, which is only valid
		 * while the table is alive. Copying out of the optional keeps
		 * the lifetime question from ever coming up. */
		if (const auto text = node.value<std::string>()) {
			return QString::fromStdString(*text);
		}
		return QString();
	}

	/* Platform IDs are strings to us, but packwiz stores CurseForge's as
	 * TOML integers. Accept either, so a hand-written or foreign sidecar
	 * that quoted them still resolves. */
	QString nodeId(const ConstNode& node)
	{
		if (const auto number = node.value<std::int64_t>()) {
			return QString::number(*number);
		}
		return nodeString(node);
	}

	bool isPlatform(const QString& platform, QLatin1String name)
	{
		return platform.compare(name, Qt::CaseInsensitive) == 0;
	}

	/* Base name for a sidecar we have no slug for. `.disabled` and the
	 * archive extension come off so that toggling a mod on and off does
	 * not change which file its metadata lives in. */
	QString baseNameOf(const QString& fileName)
	{
		QString name = ModMetadataIndex::canonicalFileName(fileName);
		static const QLatin1String archiveSuffixes[] = {
			QLatin1String(".jar"), QLatin1String(".zip"),
			QLatin1String(".litemod")};
		for (const QLatin1String& suffix : archiveSuffixes) {
			if (name.endsWith(suffix, Qt::CaseInsensitive)) {
				name.chop(suffix.size());
				break;
			}
		}
		return name;
	}

	/* Sidecar names are looked up case-insensitively by other tools, and
	 * they end up in URLs and shell commands often enough that keeping
	 * them to slug-shaped characters is worth the small loss of
	 * fidelity. The slug itself passes through untouched. */
	QString sanitizeBaseName(const QString& name)
	{
		QString out;
		out.reserve(name.size());
		for (const QChar c : name) {
			const bool plain = (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
							   || (c >= QLatin1Char('0')
								   && c <= QLatin1Char('9'))
							   || c == QLatin1Char('.')
							   || c == QLatin1Char('_')
							   || c == QLatin1Char('-');
			if (plain) {
				out.append(c);
			} else if (c.isLetterOrNumber()) {
				out.append(c.toLower());
			} else if (!out.endsWith(QLatin1Char('-'))) {
				out.append(QLatin1Char('-'));
			}
		}
		while (out.endsWith(QLatin1Char('-'))) {
			out.chop(1);
		}
		return out;
	}
}

bool Packwiz::isSidecarFileName(const QString& fileName)
{
	return fileName.endsWith(kSuffix, Qt::CaseInsensitive)
		   && fileName.size() > kSuffix.size();
}

QString Packwiz::slugFromFileName(const QString& fileName)
{
	if (!isSidecarFileName(fileName)) {
		return QString();
	}
	QString slug = fileName;
	slug.chop(kSuffix.size());
	return slug;
}

QString Packwiz::sidecarFileName(const ModMetadataIndex::Entry& entry)
{
	const QString base = sanitizeBaseName(entry.slug);
	if (base.isEmpty()) {
		return fallbackSidecarFileName(entry);
	}
	return base + kSuffix;
}

QString Packwiz::fallbackSidecarFileName(const ModMetadataIndex::Entry& entry)
{
	const QString base = sanitizeBaseName(baseNameOf(entry.fileName));
	if (base.isEmpty()) {
		return QString();
	}
	return base + kSuffix;
}

QByteArray Packwiz::serialize(const ModMetadataIndex::Entry& entry)
{
	if (entry.fileName.isEmpty()) {
		return QByteArray();
	}

	const bool curseForge =
		isPlatform(entry.platform, QLatin1String("curseforge"));
	const bool modrinth = isPlatform(entry.platform, QLatin1String("modrinth"));

	toml::table root;
	root.insert_or_assign("name", entry.name.toStdString());
	root.insert_or_assign("filename", entry.fileName.toStdString());
	if (!entry.side.isEmpty()) {
		/* Left out when unknown rather than guessed: an absent `side`
		 * already means "both" to every reader of this format, while a
		 * wrong one would keep a mod out of a server install. */
		root.insert_or_assign("side", entry.side.toStdString());
	}

	toml::table download;
	/* CurseForge asks third parties not to hand out its file URLs, so the
	 * format has a mode that says "resolve this through the API instead".
	 * Our own copy of the URL still goes in, under our own key, because
	 * we do use it (see the blocked-download path in ModFolderPage) and
	 * because a foreign reader in metadata mode ignores it. */
	download.insert_or_assign(
		"mode",
		std::string(curseForge ? "metadata:curseforge" : "url"));
	if (!curseForge && !entry.downloadUrl.isEmpty()) {
		download.insert_or_assign("url", entry.downloadUrl.toStdString());
	}

	QString hashFormat = entry.hashFormat.toLower();
	QString hash = entry.hash;
	if (!entry.sha1.isEmpty()) {
		/* Prefer the SHA-1 when we have one: it is the digest this
		 * launcher verifies downloads against, and every reader of the
		 * format takes whatever `hash-format` says. A SHA-512 carried in
		 * from another tool is only kept when that is all we have. */
		hashFormat = QStringLiteral("sha1");
		hash = entry.sha1.toLower();
	}
	if (!hashFormat.isEmpty() && !hash.isEmpty()) {
		download.insert_or_assign("hash-format", hashFormat.toStdString());
		download.insert_or_assign("hash", hash.toStdString());
	}
	root.insert_or_assign("download", std::move(download));

	bool wroteUpdate = false;
	if (curseForge) {
		/* Both IDs are numbers in this format, and a zero would be read
		 * back as a real project. Falling back to our own keys keeps the
		 * provenance instead of dropping the file's history. */
		const int projectId = entry.projectId.toInt();
		const int fileId = entry.versionId.toInt();
		if (projectId > 0 && fileId > 0) {
			toml::table provider;
			provider.insert_or_assign("project-id",
									  static_cast<std::int64_t>(projectId));
			provider.insert_or_assign("file-id",
									  static_cast<std::int64_t>(fileId));
			toml::table update;
			update.insert_or_assign("curseforge", std::move(provider));
			root.insert_or_assign("update", std::move(update));
			wroteUpdate = true;
		}
	} else if (modrinth && !entry.projectId.isEmpty()
			   && !entry.versionId.isEmpty()) {
		toml::table provider;
		provider.insert_or_assign("mod-id", entry.projectId.toStdString());
		provider.insert_or_assign("version", entry.versionId.toStdString());
		toml::table update;
		update.insert_or_assign("modrinth", std::move(provider));
		root.insert_or_assign("update", std::move(update));
		wroteUpdate = true;
	}

	if (!entry.slug.isEmpty()) {
		root.insert_or_assign("x-kite-slug", entry.slug.toStdString());
	}
	if (!wroteUpdate && !entry.platform.isEmpty()) {
		/* Manually added files are recorded as "local", and they have no
		 * update source to describe. */
		root.insert_or_assign("x-kite-platform",
							  entry.platform.toStdString());
		if (!entry.projectId.isEmpty()) {
			root.insert_or_assign("x-kite-project-id",
								  entry.projectId.toStdString());
		}
		if (!entry.versionId.isEmpty()) {
			root.insert_or_assign("x-kite-version-id",
								  entry.versionId.toStdString());
		}
	}
	/* Outside the block above on purpose: the version as a person reads
	 * it is worth keeping whether or not the file has an update source,
	 * and the format has nowhere else to put it - packwiz's own provider
	 * tables take ids. */
	if (!entry.versionNumber.isEmpty()) {
		root.insert_or_assign("x-kite-version-number",
							  entry.versionNumber.toStdString());
	}
	if (curseForge && !entry.downloadUrl.isEmpty()) {
		root.insert_or_assign("x-kite-download-url",
							  entry.downloadUrl.toStdString());
	}
	if (entry.isDependency) {
		root.insert_or_assign("x-kite-dependency", true);
	}
	if (entry.fileSize > 0) {
		root.insert_or_assign("x-kite-file-size",
							  static_cast<std::int64_t>(entry.fileSize));
	}
	const QDateTime stamp = entry.installedAt.isValid()
								? entry.installedAt
								: QDateTime::currentDateTimeUtc();
	root.insert_or_assign("x-kite-installed-at",
						  stamp.toString(Qt::ISODate).toStdString());

	std::stringstream stream;
	stream << root;
	std::string text = stream.str();
	text.push_back('\n');
	return QByteArray(text.data(), static_cast<qsizetype>(text.size()));
}

ModMetadataIndex::Entry Packwiz::parse(const QByteArray& bytes,
									   const QString& slugHint)
{
	ModMetadataIndex::Entry entry;

	toml::table parsed;
	try {
		parsed = toml::parse(std::string_view(bytes.constData(), bytes.size()));
	} catch (const toml::parse_error& err) {
		const std::string_view reason = err.description();
		qWarning() << "Packwiz: malformed sidecar:"
				   << QString::fromUtf8(reason.data(),
										static_cast<qsizetype>(reason.size()));
		return entry;
	}

	/* Read through a const reference on purpose: toml++ hands out
	 * node_view<node> for a mutable table and node_view<const node> for a
	 * const one, and the two are unrelated types with no conversion
	 * between them. Everything below wants the const flavour. */
	const toml::table& table = parsed;

	entry.fileName = nodeString(table["filename"]);
	if (entry.fileName.isEmpty()) {
		return entry;
	}
	entry.name = nodeString(table["name"]);
	entry.side = nodeString(table["side"]);

	if (const toml::table* download = table["download"].as_table()) {
		entry.downloadUrl = nodeString((*download)["url"]);
		entry.hashFormat = nodeString((*download)["hash-format"]).toLower();
		entry.hash = nodeString((*download)["hash"]);
		if (entry.hashFormat == QLatin1String("sha1")) {
			entry.sha1 = entry.hash.toLower();
		}
	}
	if (entry.downloadUrl.isEmpty()) {
		entry.downloadUrl = nodeString(table["x-kite-download-url"]);
	}

	const ConstNode update = table["update"];
	if (const toml::table* provider = update["curseforge"].as_table()) {
		entry.platform = QStringLiteral("curseforge");
		entry.projectId = nodeId((*provider)["project-id"]);
		entry.versionId = nodeId((*provider)["file-id"]);
	} else if ((provider = update["modrinth"].as_table())) {
		entry.platform = QStringLiteral("modrinth");
		entry.projectId = nodeId((*provider)["mod-id"]);
		entry.versionId = nodeId((*provider)["version"]);
	} else {
		entry.platform = nodeString(table["x-kite-platform"]);
		entry.projectId = nodeString(table["x-kite-project-id"]);
		entry.versionId = nodeString(table["x-kite-version-id"]);
	}

	entry.versionNumber = nodeString(table["x-kite-version-number"]);

	entry.slug = nodeString(table["x-kite-slug"]);
	if (entry.slug.isEmpty()) {
		/* How every reader of this format gets the slug for a file it did
		 * not write itself: it is the sidecar's own name. */
		entry.slug = slugHint;
	}
	entry.isDependency =
		table["x-kite-dependency"].value<bool>().value_or(false);
	entry.fileSize = static_cast<qint64>(
		table["x-kite-file-size"].value<std::int64_t>().value_or(0));
	const QString stamp = nodeString(table["x-kite-installed-at"]);
	if (!stamp.isEmpty()) {
		entry.installedAt = QDateTime::fromString(stamp, Qt::ISODate);
	}

	return entry;
}
