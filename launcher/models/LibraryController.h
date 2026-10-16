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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>

class IconList;
class InstanceList;

/*
 * The library's housekeeping, for the QML pages: what the widget library
 * did through MainWindow and InstanceView, over the same InstanceList.
 *
 *  - Groups: rename, delete (its instances become ungrouped), fold, and
 *    move an instance into one (a drag onto a group's header).
 *  - Undo the last delete, when it went to the trash.
 *  - Duplicate with the copy dialog's choices: worlds, play time, icon.
 *  - An instance's folders, for the "Open … folder" menu entries.
 *
 * Folded groups are kept where the widget library keeps them, in the
 * group file (instgroups.json), so both UIs agree. That file has no entry
 * for "no group", so folding the ungrouped section lasts for the session.
 */
class LibraryController : public QObject
{
	Q_OBJECT
	/// Names of the folded groups; "" is the ungrouped section.
	Q_PROPERTY(QStringList collapsedGroups READ collapsedGroups NOTIFY
				   collapsedGroupsChanged)
	/// Whether undoDelete() has something to put back, and what it is
	/// called.
	Q_PROPERTY(bool canUndoDelete READ canUndoDelete NOTIFY trashChanged)
	Q_PROPERTY(QString lastDeletedName READ lastDeletedName NOTIFY trashChanged)
	/// IconList, for the duplicate dialog's icon picker. Null without one.
	Q_PROPERTY(QObject* iconsModel READ iconsModel CONSTANT)
	/// Where custom icons live, for the picker's "Open folder".
	Q_PROPERTY(QString iconsDir READ iconsDir CONSTANT)

  public:
	LibraryController(std::shared_ptr<InstanceList> instances,
					  std::shared_ptr<IconList> icons,
					  QObject* parent = nullptr);

	QStringList collapsedGroups() const;
	bool canUndoDelete() const;
	QString lastDeletedName() const;
	QObject* iconsModel() const;
	QString iconsDir() const;

	Q_INVOKABLE void setGroupCollapsed(const QString& group, bool collapsed);
	/// Renaming onto an existing group merges the two; see
	/// InstanceList::renameGroup(). Surrounding spaces are dropped.
	Q_INVOKABLE void renameGroup(const QString& from, const QString& to);
	/// The group's instances stay, ungrouped.
	Q_INVOKABLE void deleteGroup(const QString& group);
	/// "" moves it out of every group.
	Q_INVOKABLE void moveToGroup(const QString& id, const QString& group);

	/// Puts the last instance deleted to the trash back, in its group.
	/// False when there was nothing to put back or it could not all be
	/// restored (the instance may then still be in the trash).
	Q_INVOKABLE bool undoDelete();

	/// {name, group, iconKey} of instance @p id, for an action that starts
	/// from nothing but an id. Empty for an unknown id.
	Q_INVOKABLE QVariantMap describe(const QString& id) const;

	/// The instance's folders: {instance, game, mods}. `mods` is empty for
	/// an instance that is not Minecraft. Empty for an unknown id.
	Q_INVOKABLE QVariantMap folders(const QString& id) const;
	/// Opens @p path in the file manager, creating it first if needed (a
	/// mods folder an instance has never used).
	Q_INVOKABLE void openFolder(const QString& path) const;

	/// Copies instance @p id as @p name into @p group: with its worlds
	/// when @p copySaves, its play time when @p keepPlaytime, wearing
	/// @p iconKey (empty: the original's). Returns a TaskWatcher, or null
	/// for an unknown id or an empty name.
	Q_INVOKABLE QObject* duplicate(const QString& id, const QString& name,
								   const QString& group, bool copySaves,
								   bool keepPlaytime, const QString& iconKey);

  signals:
	void collapsedGroupsChanged();
	void trashChanged();

  private:
	std::shared_ptr<InstanceList> m_instances;
	std::shared_ptr<IconList> m_icons;
};
