// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

#include "TrashUndo.h"

TrashUndo::TrashUndo(QObject* parent) : QObject(parent)
{
}

bool TrashUndo::available() const
{
	return FS::canTrash();
}

bool TrashUndo::canUndo() const
{
	return !m_items.isEmpty();
}

void TrashUndo::begin()
{
	if (m_items.isEmpty()) {
		return;
	}
	m_items.clear();
	emit changed();
}

void TrashUndo::add(const QList<FS::Trashed>& items)
{
	if (items.isEmpty()) {
		return;
	}
	m_items.append(items);
	emit changed();
}

void TrashUndo::add(const QString& path, const QString& trashPath)
{
	add({FS::Trashed{path, trashPath}});
}

bool TrashUndo::undo()
{
	QList<FS::Trashed> left;
	QStringList back;
	for (const FS::Trashed& item : std::as_const(m_items)) {
		if (FS::restoreTrashed(item)) {
			back.append(item.path);
		} else {
			left.append(item);
		}
	}
	const bool had = !m_items.isEmpty();
	m_items = left;
	if (had) {
		emit changed();
	}
	if (!back.isEmpty()) {
		emit restored(back);
	}
	return left.isEmpty();
}
