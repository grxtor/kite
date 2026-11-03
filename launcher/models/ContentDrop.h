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

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <memory>

#include "BaseInstance.h"
#include "minecraft/mod/DroppedContent.h"

class ModFolderModel;
class MinecraftInstance;

/*
 * Files, folders and links dropped on an instance, sorted by what they are
 * (DroppedContent) and put where each belongs: mods, resource packs and
 * shader packs into their folders, worlds into saves, data packs into a
 * world's datapacks folder. A drop is read first (prepare()), the questions it
 * raises are answered (chooseWorld(), resolveConflicts()), then commit() does
 * it and returns the sentence to show. Holds only the instance, so any tile
 * of the library can have one without opening the instance's page.
 *
 * prepare() returns a map:
 *   counts        what the drop holds ("2 mods and 1 world"), "" for nothing
 *   modpack       path of the first modpack (Import takes it), "" if none
 *   extraModpacks how many more modpacks were dropped (Import takes one)
 *   links         project links (Modrinth, CurseForge) to install with
 *                 installLink()
 *   otherLinks    any other http(s) links, for Import as before
 *   unknown       names of files that are none of the above
 *   needsWorld    data packs are in the drop: chooseWorld() before commit()
 *   worldChoices  [{folder, name}] of the instance's worlds, for the picker
 *   hasWorld      a world is in the drop (adding one while the game runs asks)
 *   conflicts     [{name, existing}] other versions of mods already installed:
 *                 resolveConflicts() before commit()
 *   needsLoader   mods are in the drop but the instance has no mod loader
 *   running       the game is running
 *   empty         nothing here for the instance, modpacks and links aside
 */
class ContentDrop : public QObject
{
	Q_OBJECT

  public:
	explicit ContentDrop(InstancePtr instance, QObject* parent = nullptr);
	~ContentDrop() override;

	/// What @p urls hold, for the overlay while dragging ("2 mods and 1 resource
	/// pack"); empty when nothing is recognisable. Reads, never changes.
	Q_INVOKABLE QString describe(const QStringList& urls) const;

	/// Reads a drop and keeps it for commit(); replaces any earlier one.
	Q_INVOKABLE QVariantMap prepare(const QStringList& urls);
	/// The folder name (saves/<folder>) data packs go into.
	Q_INVOKABLE void chooseWorld(const QString& folderName);
	/// "replace" swaps each conflicting mod for the dropped one; "keep" adds
	/// both.
	Q_INVOKABLE void resolveConflicts(const QString& mode);
	/// Does what prepare() found. {text, tone, worldTask, worlds}: the
	/// sentence for a toast, its tone, a TaskWatcher while worlds are being
	/// unpacked (else null) and how many worlds that is.
	Q_INVOKABLE QVariantMap commit();
	Q_INVOKABLE void cancel();

	/// Installs the project a link names, version picked for this instance's
	/// Minecraft version and loader. A ContentLinkInstall to wait on with
	/// finished(ok, text); null when @p url is not a project link.
	Q_INVOKABLE QObject* installLink(const QString& url);

  private:
	using Pending = DroppedContent::Entry;

	std::shared_ptr<ModFolderModel> folderFor(const QString& kind) const;
	bool changesAllowed(const QString& kind) const;
	QString dataPackDir() const;
	QVariantList worldChoices() const;
	QObject* startWorldInstall(const QStringList& paths);
	bool installContent(const Pending& pending, const QString& kind,
						DroppedContent::Outcome& outcome);
	bool installDataPack(const Pending& pending,
						 DroppedContent::Outcome& outcome);

	InstancePtr m_instance;
	MinecraftInstance* m_mc = nullptr;

	QList<Pending> m_pending;
	QString m_world;
	bool m_replace = true;
	QStringList m_unknown;
};
