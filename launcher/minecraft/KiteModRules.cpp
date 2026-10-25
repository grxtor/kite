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

#include "KiteModRules.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>

#include "Version.h"

namespace
{
	constexpr int kKnownFormatVersion = 1;
	const QString kAddModsPrefix = QStringLiteral("-Dfabric.addMods=");
	const QString kCacheSubfolder = QStringLiteral("kite-mod");
	const QString kJarPrefix = QStringLiteral("kite-mod-");
	const QString kJarSuffix = QStringLiteral(".jar");

	bool isSha256(const QString& text)
	{
		static const QRegularExpression hex(
			QStringLiteral("^[0-9a-fA-F]{64}$"));
		return hex.match(text).hasMatch();
	}

	bool isPlainFileName(const QString& name)
	{
		return !name.isEmpty() && name != QStringLiteral(".") &&
			   name != QStringLiteral("..") && !name.contains('/') &&
			   !name.contains('\\') && name.endsWith(kJarSuffix);
	}

	bool isHttps(const QString& text)
	{
		const QUrl url(text, QUrl::StrictMode);
		return url.isValid() && url.scheme() == QStringLiteral("https") &&
			   !url.host().isEmpty();
	}

	std::optional<KiteModBuild> parseBuild(const QJsonObject& object)
	{
		KiteModBuild build;
		build.minecraft = object.value("minecraft").toString();
		build.version = object.value("version").toString();
		build.minLoader = object.value("minLoader").toString();
		build.file = object.value("file").toString();
		build.url = object.value("url").toString();
		build.sha256 = object.value("sha256").toString().toLower();
		build.size = static_cast<qint64>(object.value("size").toDouble(-1));
		if (build.minecraft.isEmpty() || build.version.isEmpty() ||
			build.minLoader.isEmpty() || build.size <= 0 ||
			!isPlainFileName(build.file) || !isHttps(build.url) ||
			!isSha256(build.sha256)) {
			return std::nullopt;
		}
		return build;
	}
} // namespace

namespace KiteMod
{
	std::optional<QList<KiteModBuild>> parseManifest(const QByteArray& json)
	{
		QJsonParseError error;
		const QJsonDocument document = QJsonDocument::fromJson(json, &error);
		if (error.error != QJsonParseError::NoError || !document.isObject()) {
			return std::nullopt;
		}
		const QJsonObject root = document.object();
		if (root.value("formatVersion").toInt(-1) != kKnownFormatVersion) {
			return std::nullopt;
		}
		QList<KiteModBuild> builds;
		for (const QJsonValue& entry : root.value("builds").toArray()) {
			if (auto build = parseBuild(entry.toObject())) {
				builds.append(*build);
			}
		}
		return builds;
	}

	std::optional<KiteModBuild> pickBuild(const QList<KiteModBuild>& builds,
										  const QString& mcVersion,
										  const QString& loaderVersion)
	{
		std::optional<KiteModBuild> best;
		for (const KiteModBuild& build : builds) {
			if (build.minecraft != mcVersion ||
				Version(loaderVersion) < Version(build.minLoader)) {
				continue;
			}
			if (!best || Version(build.version) > Version(best->version)) {
				best = build;
			}
		}
		return best;
	}

	bool appliesTo(bool enabled, const QString& primaryLoader,
				   const QString& loaderVersion)
	{
		return enabled && primaryLoader == QStringLiteral("fabric") &&
			   !loaderVersion.isEmpty();
	}

	QString addModsArgument(const QStringList& userJvmArgs,
							const QString& jarPath)
	{
		QString theirs;
		for (const QString& arg : userJvmArgs) {
			if (arg.startsWith(kAddModsPrefix)) {
				theirs = arg.mid(kAddModsPrefix.size());
			}
		}
		if (theirs.isEmpty()) {
			return kAddModsPrefix + jarPath;
		}
		return kAddModsPrefix + theirs + QDir::listSeparator() + jarPath;
	}

	QString cacheFolder(const QString& cacheRoot)
	{
		return QDir(cacheRoot).filePath(kCacheSubfolder);
	}

	QString jarPath(const QString& cacheRoot, const KiteModBuild& build)
	{
		return QDir(cacheFolder(cacheRoot)).filePath(build.file);
	}

	bool isDownloaded(const QString& cacheRoot, const KiteModBuild& build)
	{
		const QFileInfo info(jarPath(cacheRoot, build));
		return info.isFile() && info.size() == build.size;
	}

	std::optional<KiteModBuild> cachedBuild(const QString& cacheRoot,
											const QString& mcVersion,
											const QString& loaderVersion)
	{
		QFile manifest(QDir(cacheRoot).filePath(kManifestPath));
		if (!manifest.open(QIODevice::ReadOnly)) {
			return std::nullopt;
		}
		const auto builds = parseManifest(manifest.readAll());
		if (!builds) {
			return std::nullopt;
		}
		return pickBuild(*builds, mcVersion, loaderVersion);
	}

	QStringList outdatedJars(const QStringList& names, const QString& keep)
	{
		QStringList outdated;
		for (const QString& name : names) {
			if (name != keep && name.startsWith(kJarPrefix) &&
				name.endsWith(kJarSuffix)) {
				outdated.append(name);
			}
		}
		return outdated;
	}
} // namespace KiteMod
