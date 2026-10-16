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

#include "models/LibraryController.h"

#include "DesktopServices.h"
#include "InstanceCopyTask.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "tasks/TaskWatcher.h"

LibraryController::LibraryController(std::shared_ptr<InstanceList> instances,
									 std::shared_ptr<IconList> icons,
									 QObject* parent)
	: QObject(parent), m_instances(std::move(instances)),
	  m_icons(std::move(icons))
{
	/* Deleting goes through the shell, not through here, and the list only
	 * learns the folder has gone when it rescans. Whatever changes the
	 * rows may have changed what there is to undo. */
	auto* list = m_instances.get();
	connect(list, &QAbstractItemModel::rowsRemoved, this,
			&LibraryController::trashChanged);
	connect(list, &QAbstractItemModel::rowsInserted, this,
			&LibraryController::trashChanged);
	connect(list, &QAbstractItemModel::modelReset, this,
			&LibraryController::trashChanged);
	/* A rescan re-reads the group file, folds included. */
	connect(list, &QAbstractItemModel::modelReset, this,
			&LibraryController::collapsedGroupsChanged);
}

QStringList LibraryController::collapsedGroups() const
{
	QStringList collapsed;
	if (m_instances->isGroupCollapsed(QString())) {
		collapsed.append(QString());
	}
	for (const QString& group : m_instances->getGroups()) {
		if (!group.isEmpty() && m_instances->isGroupCollapsed(group)) {
			collapsed.append(group);
		}
	}
	return collapsed;
}

bool LibraryController::canUndoDelete() const
{
	return m_instances->trashedSomething();
}

QString LibraryController::lastDeletedName() const
{
	return m_instances->lastTrashedName();
}

QObject* LibraryController::iconsModel() const
{
	return m_icons.get();
}

QString LibraryController::iconsDir() const
{
	return m_icons ? m_icons->getDirectory() : QString();
}

void LibraryController::setGroupCollapsed(const QString& group, bool collapsed)
{
	if (m_instances->isGroupCollapsed(group) == collapsed) {
		return;
	}
	m_instances->on_GroupStateChanged(group, collapsed);
	emit collapsedGroupsChanged();
}

void LibraryController::renameGroup(const QString& from, const QString& to)
{
	const QString name = to.trimmed();
	if (name.isEmpty() || name == from) {
		return;
	}
	const bool wasCollapsed = m_instances->isGroupCollapsed(from);
	m_instances->renameGroup(from, name);
	if (wasCollapsed) {
		emit collapsedGroupsChanged();
	}
}

void LibraryController::deleteGroup(const QString& group)
{
	if (group.isEmpty()) {
		return;
	}
	const bool wasCollapsed = m_instances->isGroupCollapsed(group);
	m_instances->deleteGroup(group);
	if (wasCollapsed) {
		emit collapsedGroupsChanged();
	}
}

void LibraryController::moveToGroup(const QString& id, const QString& group)
{
	m_instances->setInstanceGroup(id, group.trimmed());
}

bool LibraryController::undoDelete()
{
	if (!m_instances->trashedSomething()) {
		return false;
	}
	const bool restored = m_instances->undoTrashInstance();
	emit trashChanged();
	return restored;
}

QVariantMap LibraryController::describe(const QString& id) const
{
	auto instance = m_instances->getInstanceById(id);
	if (!instance) {
		return {};
	}
	return {
		{QStringLiteral("name"), instance->name()},
		{QStringLiteral("group"), m_instances->getInstanceGroup(id)},
		{QStringLiteral("iconKey"), instance->iconKey()},
	};
}

QVariantMap LibraryController::folders(const QString& id) const
{
	auto instance = m_instances->getInstanceById(id);
	if (!instance) {
		return {};
	}
	auto* minecraft = dynamic_cast<MinecraftInstance*>(instance.get());
	return {
		{QStringLiteral("instance"), instance->instanceRoot()},
		{QStringLiteral("game"), instance->gameRoot()},
		{QStringLiteral("mods"), minecraft ? minecraft->modsRoot() : QString()},
	};
}

void LibraryController::openFolder(const QString& path) const
{
	if (path.isEmpty()) {
		return;
	}
	DesktopServices::openDirectory(path, true);
}

QObject* LibraryController::duplicate(const QString& id, const QString& name,
									  const QString& group, bool copySaves,
									  bool keepPlaytime, const QString& iconKey)
{
	auto original = m_instances->getInstanceById(id);
	/* One line, as the name field is: the same rule QmlShell's
	 * sanitizedInstanceName() applies to a typed name. */
	QString copyName = name;
	copyName.replace(QLatin1Char('\n'), QLatin1Char(' '));
	copyName = copyName.trimmed();
	if (!original || copyName.isEmpty()) {
		return nullptr;
	}

	auto* copyTask = new InstanceCopyTask(original, copySaves, keepPlaytime);
	copyTask->setName(copyName);
	copyTask->setGroup(group);
	copyTask->setIcon(iconKey.isEmpty() ? original->iconKey() : iconKey);

	Task* wrapped = m_instances->wrapInstanceTask(copyTask);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(copyName);
	wrapped->start();
	return watcher;
}
