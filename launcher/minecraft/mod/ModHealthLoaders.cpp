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

#include "ModHealth.h"

#include <optional>

#include "ModHealthIndex.h"
#include "ModHealthLoaders.h"
#include "VersionRange.h"

namespace ModHealth
{
	namespace
	{
		const QString LoaderFabric = QStringLiteral("fabric");
		const QString LoaderQuilt = QStringLiteral("quilt");
		const QString LoaderForge = QStringLiteral("forge");
		const QString LoaderNeoForge = QStringLiteral("neoforge");

		/// The first Minecraft release whose NeoForge no longer reads
		/// mods.toml, only neoforge.mods.toml.
		const QString NeoForgeOnlyFrom = QStringLiteral("1.20.5");

		bool neoForgeReadsForge(const QString& minecraft)
		{
			// A version that cannot be ordered (a snapshot, none at all) gets
			// the benefit of the doubt: a forge jar wrongly called unreadable
			// is a false alarm, a wrongly accepted one is only silence.
			const std::optional<int> order =
				VersionRange::compare(minecraft, NeoForgeOnlyFrom);
			return !order || *order < 0;
		}
	} // namespace

	namespace detail
	{
		const QString MinecraftId = QStringLiteral("minecraft");

		/* The runtime hands these out itself, with no file behind them. */
		const QStringList RuntimeIds = {
			QStringLiteral("java"),		 QStringLiteral("fml"),
			QStringLiteral("javafml"),	 QStringLiteral("lowcodefml"),
			QStringLiteral("mixinextras"),
		};

		/* The loaders' own ids. The caller lists them in builtIns with a
		 * version; one it left out is still the loader's to provide, not a
		 * missing mod ("forge" on NeoForge 1.20.1, for one). */
		const QStringList LoaderIds = {
			QStringLiteral("forge"),
			QStringLiteral("neoforge"),
			QStringLiteral("fabricloader"),
			QStringLiteral("quilt_loader"),
		};

		/* The extra versions a built-in answers to, which FML's
		 * VersionSupportMatrix (net.neoforged.fml.loading and
		 * net.minecraftforge.fml.loading) adds for the game version it runs:
		 * a mod built for NeoForge 1.21 may ask for [1.21,1.21.1) and still
		 * load on 1.21.1; Forge 1.20.1 also counts as 47.1.79. */
		QStringList alsoAnswersTo(const QString& id, const QStringList& loaders,
								  const QString& minecraft)
		{
			const auto isMinecraft = [&minecraft](const QString& version) {
				const std::optional<int> order =
					VersionRange::compare(minecraft, version);
				return order && *order == 0;
			};
			if (loaders.contains(LoaderNeoForge) &&
				isMinecraft(QStringLiteral("1.21.1"))) {
				if (id == MinecraftId)
					return { QStringLiteral("1.21") };
				if (id == LoaderNeoForge)
					return { QStringLiteral("21.0.166") };
			}
			if (loaders.contains(LoaderForge) &&
				isMinecraft(QStringLiteral("1.20.1")) && id == LoaderForge) {
				return { QStringLiteral("47.1.79") };
			}
			return {};
		}
	} // namespace detail

	QStringList readableLoaders(const Environment& environment,
								bool hasConnector)
	{
		QStringList readable;
		const auto add = [&readable](const QString& loader) {
			if (!readable.contains(loader))
				readable.append(loader);
		};

		const QStringList own = detail::lowered(environment.loaders);
		for (const QString& loader : own)
			add(loader);
		for (const QString& loader : own) {
			if (loader == LoaderQuilt)
				add(LoaderFabric);
			if (loader == LoaderNeoForge &&
				neoForgeReadsForge(environment.minecraft)) {
				add(LoaderForge);
			}
			if (hasConnector &&
				(loader == LoaderForge || loader == LoaderNeoForge)) {
				add(LoaderFabric);
			}
		}
		return readable;
	}
} // namespace ModHealth
