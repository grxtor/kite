/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace JavaDownload
{

	struct RuntimeVersion {
		int major = 0;
		int minor = 0;
		int security = 0;
		int build = 0;

		QString toString() const
		{
			if (build > 0)
				return QString("%1.%2.%3+%4")
					.arg(major)
					.arg(minor)
					.arg(security)
					.arg(build);
			return QString("%1.%2.%3").arg(major).arg(minor).arg(security);
		}
	};

	struct RuntimeEntry {
		QString name;
		QString url;
		QString checksumHash;
		QString checksumType;
		QString downloadType;
		QString packageType;
		QString releaseTime;
		QString runtimeOS;
		QString vendor;
		RuntimeVersion version;
	};

	struct JavaVersionInfo {
		QString uid;
		QString versionId;
		QString name;
		QString sha256;
		QString releaseTime;
		bool recommended = false;
	};

	struct JavaProviderInfo {
		QString uid;
		QString name;
		QString iconName;

		static QList<JavaProviderInfo> availableProviders()
		{
			return {
				{"net.adoptium.java", "Eclipse Adoptium", "adoptium"},
				{"com.azul.java", "Azul Zulu", "azul"},
				{"com.ibm.java", "IBM Semeru (OpenJ9)", "openj9_hex_custom"},
				{"net.minecraft.java", "Mojang", "mojang"},
			};
		}
	};

	/* Names and paths in the meta server's and Mojang's JSON are untrusted: they
	 * end up joined into paths that are written to and deleted. */

	// One file or folder name: no separators, not empty, not "." or "..".
	inline bool isSafeSegment(const QString& segment)
	{
		return !segment.isEmpty() && segment != QLatin1String(".") &&
			   segment != QLatin1String("..") &&
			   !segment.contains(QLatin1Char('/')) &&
			   !segment.contains(QLatin1Char('\\')) &&
			   !segment.contains(QLatin1Char(':')) &&
			   !segment.contains(QChar::Null);
	}

	// A '/' separated path made only of safe segments.
	inline bool isSafeRelativePath(const QString& path)
	{
		const auto parts = path.split(QLatin1Char('/'));
		for (const auto& part : parts) {
			if (!isSafeSegment(part)) {
				return false;
			}
		}
		return !parts.isEmpty();
	}

	/* Whether @p path lies strictly below @p root, ".." resolved. Both sides go
	 * through QFileInfo: QDir::absolutePath() hands a clean path back as it is
	 * but sends one with ".." through Windows, which gives "/x" the current
	 * drive and "c:" a capital letter, and the two would no longer compare. */
	inline bool isInside(const QString& root, const QString& path)
	{
		QString base = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
		if (!base.endsWith(QLatin1Char('/'))) {
			base += QLatin1Char('/');
		}
		return QDir::cleanPath(QFileInfo(path).absoluteFilePath())
			.startsWith(base);
	}

	// <root>/<relative>, or empty if @p relative is unsafe or escapes @p root.
	inline QString joinInside(const QString& root, const QString& relative)
	{
		if (!isSafeRelativePath(relative)) {
			return QString();
		}
		const QString joined = QDir::cleanPath(
			QDir(root).absolutePath() + QLatin1Char('/') + relative);
		return isInside(root, joined) ? joined : QString();
	}

	/* Whether a link at @p linkPath may point at @p target (relative to the
	 * link's own folder): ".." only as a leading run, so the target cannot
	 * reach outside @p root through another link, and landing inside it. */
	inline bool isSafeLinkTarget(const QString& root, const QString& linkPath,
								 const QString& target)
	{
		if (target.isEmpty() || target.startsWith(QLatin1Char('/')) ||
			target.contains(QLatin1Char('\\')) ||
			target.contains(QLatin1Char(':'))) {
			return false;
		}
		const auto parts = target.split(QLatin1Char('/'));
		int i = 0;
		while (i < parts.size() && parts[i] == QLatin1String("..")) {
			++i;
		}
		for (; i < parts.size(); ++i) {
			if (!isSafeSegment(parts[i])) {
				return false;
			}
		}
		return isInside(root, QFileInfo(linkPath).path() + QLatin1Char('/') +
								  target);
	}

	inline QString currentRuntimeOS()
	{
#if defined(Q_OS_LINUX)
#if defined(__aarch64__)
		return "linux-arm64";
#elif defined(__riscv)
		return "linux-riscv64";
#else
		return "linux-x64";
#endif
#elif defined(Q_OS_MACOS)
#if defined(__aarch64__)
		return "mac-os-arm64";
#else
		return "mac-os-x64";
#endif
#elif defined(Q_OS_WIN)
#if defined(__aarch64__) || defined(_M_ARM64)
		return "windows-arm64";
#else
		return "windows-x64";
#endif
#else
		return "unknown";
#endif
	}

	inline RuntimeEntry parseRuntimeEntry(const QJsonObject& obj)
	{
		RuntimeEntry entry;
		entry.name = obj["name"].toString();
		entry.url = obj["url"].toString();
		entry.downloadType = obj["downloadType"].toString();
		entry.packageType = obj["packageType"].toString();
		entry.releaseTime = obj["releaseTime"].toString();
		entry.runtimeOS = obj["runtimeOS"].toString();
		entry.vendor = obj["vendor"].toString();

		auto checksum = obj["checksum"].toObject();
		entry.checksumHash = checksum["hash"].toString();
		entry.checksumType = checksum["type"].toString();

		auto ver = obj["version"].toObject();
		entry.version.major = ver["major"].toInt();
		entry.version.minor = ver["minor"].toInt();
		entry.version.security = ver["security"].toInt();
		entry.version.build = ver["build"].toInt();

		return entry;
	}

	inline QList<RuntimeEntry> parseRuntimes(const QJsonObject& obj)
	{
		QList<RuntimeEntry> entries;
		auto arr = obj["runtimes"].toArray();
		for (const auto& val : arr) {
			entries.append(parseRuntimeEntry(val.toObject()));
		}
		return entries;
	}

	inline QList<JavaVersionInfo> parseVersionIndex(const QJsonObject& obj,
													const QString& uid)
	{
		QList<JavaVersionInfo> versions;
		auto arr = obj["versions"].toArray();
		for (const auto& val : arr) {
			auto vObj = val.toObject();
			JavaVersionInfo info;
			info.uid = uid;
			info.versionId = vObj["version"].toString();
			info.sha256 = vObj["sha256"].toString();
			info.releaseTime = vObj["releaseTime"].toString();
			info.recommended = vObj["recommended"].toBool(false);
			info.name = obj["name"].toString();
			versions.append(info);
		}
		return versions;
	}

} // namespace JavaDownload
