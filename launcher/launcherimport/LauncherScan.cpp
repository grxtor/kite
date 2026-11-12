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

#include "LauncherScan.h"

#include <QDir>
#include <QSet>

#include "ImportCopy.h"
#include "ScanCommon.h"

namespace LauncherImport
{
	namespace
	{
		/* Times before this are a launcher's way of saying "never". */
		constexpr int kFirstPlausibleYear = 2009;
		/* A number past this is milliseconds, not seconds (year 5138). */
		constexpr qint64 kMillisecondsThreshold = 100000000000LL;

		/* Whether @p dir holds data of @p source worth listing. */
		bool hasData(Source source, const QString& dir)
		{
			const QDir d(dir);
			switch (source) {
			case Source::Prism:
			case Source::PolyMC:
			case Source::MultiMC:
			case Source::MeshMC:
			case Source::ATLauncher:
			case Source::FTBApp:
				return d.exists(QStringLiteral("instances"));
			case Source::ModrinthApp:
				return d.exists(QStringLiteral("app.db")) ||
					   d.exists(QStringLiteral("profiles"));
			case Source::CurseForge:
				return d.exists();
			case Source::Official:
				return d.exists(QStringLiteral("launcher_profiles.json"));
			case Source::Technic:
				return d.exists(QStringLiteral("modpacks"));
			}
			return false;
		}

		FoundLauncher scanOne(Source source, const QString& dir)
		{
			switch (source) {
			case Source::Prism:
			case Source::PolyMC:
			case Source::MultiMC:
			case Source::MeshMC:
				return scanMultiMc(source, dir);
			case Source::ModrinthApp:
				return scanModrinthApp(dir);
			case Source::CurseForge:
				return scanCurseForge(dir);
			case Source::Official:
				return scanOfficial(dir);
			case Source::ATLauncher:
			case Source::Technic:
			case Source::FTBApp:
				break;
			}
			FoundLauncher unsupported;
			unsupported.source = source;
			unsupported.dataDir = dir;
			return unsupported;
		}
	} // namespace

	QString sourceKey(Source source)
	{
		switch (source) {
		case Source::Prism: return QStringLiteral("prism");
		case Source::PolyMC: return QStringLiteral("polymc");
		case Source::MultiMC: return QStringLiteral("multimc");
		case Source::MeshMC: return QStringLiteral("meshmc");
		case Source::ModrinthApp: return QStringLiteral("modrinth");
		case Source::CurseForge: return QStringLiteral("curseforge");
		case Source::Official: return QStringLiteral("official");
		case Source::ATLauncher: return QStringLiteral("atlauncher");
		case Source::Technic: return QStringLiteral("technic");
		case Source::FTBApp: return QStringLiteral("ftb");
		}
		return {};
	}

	QString sourceName(Source source)
	{
		switch (source) {
		case Source::Prism: return QStringLiteral("Prism Launcher");
		case Source::PolyMC: return QStringLiteral("PolyMC");
		case Source::MultiMC: return QStringLiteral("MultiMC");
		case Source::MeshMC: return QStringLiteral("MeshMC");
		case Source::ModrinthApp: return QStringLiteral("Modrinth App");
		case Source::CurseForge: return QStringLiteral("CurseForge");
		case Source::Official: return QStringLiteral("Minecraft Launcher");
		case Source::ATLauncher: return QStringLiteral("ATLauncher");
		case Source::Technic: return QStringLiteral("Technic Launcher");
		case Source::FTBApp: return QStringLiteral("FTB App");
		}
		return {};
	}

	bool sourceSupported(Source source)
	{
		return source != Source::ATLauncher && source != Source::Technic &&
			   source != Source::FTBApp;
	}

	QString loaderOfComponent(const QString& uid)
	{
		if (uid == QLatin1String("net.fabricmc.fabric-loader"))
			return QStringLiteral("fabric");
		if (uid == QLatin1String("org.quiltmc.quilt-loader"))
			return QStringLiteral("quilt");
		if (uid == QLatin1String("net.minecraftforge"))
			return QStringLiteral("forge");
		if (uid == QLatin1String("net.neoforged"))
			return QStringLiteral("neoforge");
		return {};
	}

	QString componentOfLoader(const QString& loader)
	{
		if (loader == QLatin1String("fabric"))
			return QStringLiteral("net.fabricmc.fabric-loader");
		if (loader == QLatin1String("quilt"))
			return QStringLiteral("org.quiltmc.quilt-loader");
		if (loader == QLatin1String("forge"))
			return QStringLiteral("net.minecraftforge");
		if (loader == QLatin1String("neoforge"))
			return QStringLiteral("net.neoforged");
		return {};
	}

	QDateTime parseTime(const QVariant& value)
	{
		QDateTime time;
		bool isNumber = false;
		const qint64 number = value.toLongLong(&isNumber);
		if (isNumber && number > 0) {
			time = number > kMillisecondsThreshold
					   ? QDateTime::fromMSecsSinceEpoch(number)
					   : QDateTime::fromSecsSinceEpoch(number);
		} else if (!isNumber) {
			time = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
			if (!time.isValid()) {
				time = QDateTime::fromString(value.toString(), Qt::ISODate);
			}
		}
		if (!time.isValid() || time.date().year() < kFirstPlausibleYear) {
			return {};
		}
		return time;
	}

	QList<FoundLauncher> scanAll(const SearchEnvironment& env,
								 const std::atomic_bool* cancel)
	{
		QList<FoundLauncher> found;
		QSet<QString> seen;
		for (const Source source : allSources()) {
			for (const QString& dir : candidateDirs(source, env)) {
				if (cancel && cancel->load()) {
					return found;
				}
				if (dir.isEmpty() || !hasData(source, dir)) {
					continue;
				}
				const QString real = ScanCommon::canonical(dir);
				if (seen.contains(real)) {
					continue;
				}
				seen.insert(real);
				FoundLauncher launcher = scanOne(source, dir);
				/* A CurseForge folder with no instances is just a folder. */
				if (source == Source::CurseForge && launcher.instances.isEmpty() &&
					launcher.problem.isEmpty()) {
					continue;
				}
				found.append(launcher);
			}
		}
		return found;
	}

	void countCopySizes(QList<FoundLauncher>* launchers,
						const std::atomic_bool* cancel)
	{
		QHash<QString, qint64> counted;
		for (FoundLauncher& launcher : *launchers) {
			for (FoundInstance& instance : launcher.instances) {
				if (cancel && cancel->load()) {
					return;
				}
				if (!instance.importable()) {
					continue;
				}
				const QString real = ScanCommon::canonical(instance.gameDir);
				if (!counted.contains(real)) {
					counted.insert(real, planCopy(instance.gameDir, cancel).bytes);
				}
				instance.copyBytes = counted.value(real);
			}
		}
	}
} // namespace LauncherImport
