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

#include "LauncherLocations.h"

#include <QDir>

namespace LauncherImport
{
	namespace
	{
		QString join(const QString& base, const QString& relative)
		{
			if (base.isEmpty()) {
				return {};
			}
			return QDir::cleanPath(base + QLatin1Char('/') + relative);
		}

		QString macSupport(const SearchEnvironment& env, const QString& name)
		{
			return join(env.home, QStringLiteral("Library/Application Support/") + name);
		}

		/* A Flatpak keeps its data under ~/.var/app/<id>/. */
		QString flatpak(const SearchEnvironment& env, const QString& id,
						const QString& inside)
		{
			return join(env.home, QStringLiteral(".var/app/%1/%2").arg(id, inside));
		}

		/* Prism and its relatives share one layout; only the name differs. */
		QStringList multiMcFamily(const SearchEnvironment& env,
								  const QString& macName,
								  const QString& windowsName,
								  const QString& linuxName,
								  const QString& flatpakId)
		{
			using System = SearchEnvironment::System;
			switch (env.system) {
			case System::MacOS:
				return {macSupport(env, macName)};
			case System::Windows:
				return windowsName.isEmpty() ? QStringList()
											 : QStringList{join(env.appData, windowsName)};
			case System::Linux: {
				QStringList dirs{join(env.dataHome, linuxName)};
				if (!flatpakId.isEmpty()) {
					dirs.append(flatpak(env, flatpakId,
										QStringLiteral("data/") + linuxName));
				}
				return dirs;
			}
			}
			return {};
		}
	} // namespace

	SearchEnvironment SearchEnvironment::underHome(const QString& home,
												   System system)
	{
		SearchEnvironment env;
		env.system = system;
		env.home = QDir::cleanPath(home);
		env.appData = join(env.home, QStringLiteral("AppData/Roaming"));
		env.localAppData = join(env.home, QStringLiteral("AppData/Local"));
		env.dataHome = join(env.home, QStringLiteral(".local/share"));
		return env;
	}

	SearchEnvironment SearchEnvironment::current()
	{
#if defined(Q_OS_MACOS)
		const System system = System::MacOS;
#elif defined(Q_OS_WIN)
		const System system = System::Windows;
#else
		const System system = System::Linux;
#endif
		const QString fakeHome = qEnvironmentVariable("KITE_LAUNCHER_IMPORT_HOME");
		if (!fakeHome.isEmpty()) {
			return underHome(fakeHome, system);
		}
		SearchEnvironment env = underHome(QDir::homePath(), system);
		if (system == System::Windows) {
			const QString appData = qEnvironmentVariable("APPDATA");
			const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
			if (!appData.isEmpty()) {
				env.appData = QDir::fromNativeSeparators(appData);
			}
			if (!localAppData.isEmpty()) {
				env.localAppData = QDir::fromNativeSeparators(localAppData);
			}
		}
		const QString xdg = qEnvironmentVariable("XDG_DATA_HOME");
		if (system == System::Linux && QDir::isAbsolutePath(xdg)) {
			env.dataHome = QDir::cleanPath(xdg);
		}
		return env;
	}

	QList<Source> allSources()
	{
		return {Source::Prism,		 Source::ModrinthApp, Source::CurseForge,
				Source::Official,	 Source::MultiMC,	  Source::PolyMC,
				Source::MeshMC,		 Source::ATLauncher,  Source::Technic,
				Source::FTBApp};
	}

	QStringList candidateDirs(Source source, const SearchEnvironment& env)
	{
		using System = SearchEnvironment::System;
		const bool mac = env.system == System::MacOS;
		const bool windows = env.system == System::Windows;
		switch (source) {
		case Source::Prism:
			return multiMcFamily(env, QStringLiteral("PrismLauncher"),
								 QStringLiteral("PrismLauncher"),
								 QStringLiteral("PrismLauncher"),
								 QStringLiteral("org.prismlauncher.PrismLauncher"));
		case Source::PolyMC:
			return multiMcFamily(env, QStringLiteral("PolyMC"),
								 QStringLiteral("PolyMC"), QStringLiteral("PolyMC"),
								 QStringLiteral("org.polymc.PolyMC"));
		case Source::MultiMC:
			/* On Windows MultiMC is portable only: its data sits beside the
			 * program, wherever that was unpacked. */
			return multiMcFamily(env, QStringLiteral("MultiMC"), QString(),
								 QStringLiteral("multimc"), QString());
		case Source::MeshMC:
			return multiMcFamily(env, QStringLiteral("MeshMC"),
								 QStringLiteral("MeshMC"), QStringLiteral("MeshMC"),
								 QString());
		case Source::ModrinthApp: {
			/* "ModrinthApp" since 0.8; older installs kept the bundle id. */
			const QStringList names{QStringLiteral("ModrinthApp"),
									QStringLiteral("com.modrinth.theseus")};
			QStringList dirs;
			for (const QString& name : names) {
				dirs.append(mac ? macSupport(env, name)
							: windows ? join(env.appData, name)
									  : join(env.dataHome, name));
			}
			return dirs;
		}
		case Source::CurseForge:
			return {join(env.home, QStringLiteral("curseforge/minecraft/Instances")),
					join(env.home,
						 QStringLiteral("Documents/curseforge/minecraft/Instances"))};
		case Source::Official:
			if (mac) {
				return {macSupport(env, QStringLiteral("minecraft"))};
			}
			if (windows) {
				return {join(env.appData, QStringLiteral(".minecraft"))};
			}
			return {join(env.home, QStringLiteral(".minecraft")),
					flatpak(env, QStringLiteral("com.mojang.Minecraft"),
							QStringLiteral(".minecraft"))};
		case Source::ATLauncher:
			return {mac ? macSupport(env, QStringLiteral("ATLauncher"))
				: windows ? join(env.appData, QStringLiteral("ATLauncher"))
						  : join(env.dataHome, QStringLiteral("ATLauncher"))};
		case Source::Technic:
			return {mac ? macSupport(env, QStringLiteral("technic"))
				: windows ? join(env.appData, QStringLiteral(".technic"))
						  : join(env.home, QStringLiteral(".technic"))};
		case Source::FTBApp:
			return {mac ? macSupport(env, QStringLiteral(".ftba"))
				: windows ? join(env.localAppData, QStringLiteral(".ftba"))
						  : join(env.home, QStringLiteral(".ftba"))};
		}
		return {};
	}
} // namespace LauncherImport
