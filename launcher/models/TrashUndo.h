// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QList>
#include <QObject>
#include <QStringList>

#include "FileSystem.h"

/*
 * What the last delete on an instance page moved to the trash, so the toast
 * that follows can offer "Undo". One batch at a time: a delete of five mods is
 * one batch, and the next delete replaces it. Nothing is recorded for a file
 * that was deleted outright, which is how the page knows not to offer undo.
 */
class TrashUndo : public QObject
{
	Q_OBJECT

	/// Whether deletes go to a trash here at all; false means they are permanent.
	Q_PROPERTY(bool available READ available CONSTANT)
	Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)

  public:
	explicit TrashUndo(QObject* parent = nullptr);

	bool available() const;
	bool canUndo() const;

	/// Starts a new batch, forgetting the previous one.
	Q_INVOKABLE void begin();
	/// Adds to the current batch.
	void add(const QList<FS::Trashed>& items);
	void add(const QString& path, const QString& trashPath);

	/// Moves the batch back to where it was. True if all of it came back;
	/// what did not stays in the batch (and in the trash) for another try.
	Q_INVOKABLE bool undo();

  signals:
	void changed();
	/// The original paths that are back on disk; owners of those folders
	/// reload their lists on this.
	void restored(const QStringList& paths);

  private:
	QList<FS::Trashed> m_items;
};
