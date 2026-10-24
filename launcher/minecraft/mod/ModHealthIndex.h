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

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "ModHealth.h"

/* What is in the folder, found by id, and which loader reads each file.
 * Private to the health check (ModHealth.cpp and ModHealthLoaders.cpp);
 * header-only, as it is small and has no other users. */
namespace ModHealth
{
	namespace detail
	{
		/* One file that answers to an id, and the version it answers with. */
		struct Provider {
			int entry = 0;
			QString version;
		};
		using Providers = QHash<QString, QList<Provider>>;

		inline QStringList lowered(const QStringList& texts)
		{
			QStringList result;
			for (const QString& text : texts)
				result.append(text.toLower());
			return result;
		}

		/* Sinytra Connector's mod id: "connectormod" in the builds for 1.20.1,
		 * "connector" in newer ones. */
		inline bool isConnectorId(const QString& id)
		{
			return id == QLatin1String("connector") ||
				   id == QLatin1String("connectormod");
		}

		/* An id the way Connector compares them: it gives a Fabric mod's id
		 * to Forge's mod list with "-" written as "_" ("fabric-api" is the
		 * Forgified Fabric API's "fabric_api"). */
		inline QString foldedId(const QString& id)
		{
			return id.toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
		}

		struct FolderIndex {
			FolderIndex(const QList<Entry>& files,
						const Environment& environment)
				: entries(files), ownLoaders(lowered(environment.loaders))
			{
				for (auto it = environment.builtIns.constBegin();
					 it != environment.builtIns.constEnd(); ++it) {
					builtIns.insert(it.key().toLower(), it.value());
				}
				// Minecraft is always there, whether or not the caller said
				// so.
				const QString minecraft = QStringLiteral("minecraft");
				if (!builtIns.contains(minecraft))
					builtIns.insert(minecraft, environment.minecraft);

				// A file's own id goes first, so that when it is the one
				// named in a report it is the file, not whatever else
				// bundles its id.
				for (int i = 0; i < files.size(); ++i) {
					provide(files.at(i), files.at(i).details.mod_id, i,
							files.at(i).details.version);
				}
				for (int i = 0; i < files.size(); ++i) {
					for (const ProvidedMod& provided :
						 files.at(i).details.provides) {
						provide(files.at(i), provided.id, i, provided.version);
					}
				}

				// Connector is a mod like any other, found by an id it
				// answers to - its jar only carries it, as a nested mod.
				bool hasConnector = false;
				for (const QString& id : enabled.keys()) {
					hasConnector |= isConnectorId(id);
				}
				readable = readableLoaders(environment, hasConnector);
			}

			//! The loader that reads the file; empty when none of its own does.
			QString readingLoader(const Entry& entry) const
			{
				const QStringList builtFor = lowered(entry.details.loaders);
				for (const QString& loader : readable) {
					if (builtFor.contains(loader))
						return loader;
				}
				return QString();
			}

			/* Whether @p loader, the one reading a file, is Fabric's
			 * metadata read by something that is not Fabric or Quilt:
			 * that is Connector, whose ids are folded. */
			bool readsThroughConnector(const QString& loader) const
			{
				return loader == QLatin1String("fabric") &&
					   !ownLoaders.contains(QLatin1String("fabric")) &&
					   !ownLoaders.contains(QLatin1String("quilt"));
			}

			/* The providers of @p id, found the way Connector finds them
			 * when @p folded. */
			const QList<Provider> enabledFor(const QString& id,
											 bool folded) const
			{
				return folded ? enabledFolded.value(foldedId(id))
							  : enabled.value(id.toLower());
			}
			const QList<Provider> disabledFor(const QString& id,
											  bool folded) const
			{
				return folded ? disabledFolded.value(foldedId(id))
							  : disabled.value(id.toLower());
			}

			bool isWrongLoader(int entry) const
			{
				const Entry& file = entries.at(entry);
				return !file.details.loaders.isEmpty() &&
					   readingLoader(file).isEmpty();
			}

			//! Enabled and actually loaded: the files that can clash.
			bool isLoaded(int entry) const
			{
				return entries.at(entry).enabled && !isWrongLoader(entry);
			}

			const QList<Entry>& entries;
			/// The instance's own loaders, in lower case.
			QStringList ownLoaders;
			QStringList readable;
			/// By lower-case id.
			QHash<QString, QString> builtIns;
			Providers enabled;
			Providers disabled;
			/// The same, by foldedId().
			Providers enabledFolded;
			Providers disabledFolded;

		  private:
			void provide(const Entry& entry, const QString& id, int index,
						 const QString& version)
			{
				if (id.isEmpty())
					return;
				(entry.enabled ? enabled : disabled)[id.toLower()].append(
					{ index, version });
				(entry.enabled ? enabledFolded : disabledFolded)[foldedId(id)]
					.append({ index, version });
			}
		};
	} // namespace detail
} // namespace ModHealth
