/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

#include "BaseInstance.h"
#include "MMCZip.h"
#include "tasks/Task.h"

class QAbstractFileIconProvider;
class QFileSystemModel;
class ExportFileTree;
class MinecraftInstance;
class TaskWatcher;

/*
 * Exports one instance for the QML dialog. "zip" writes the instance folder as
 * is; "modrinth" and "curseforge" write a pack (a manifest naming the mods to
 * download, plus the rest of the game folder) and need a Minecraft instance,
 * so `packFormats` is false otherwise and the format stays "zip".
 *
 * `files` is a checkable tree (FileIgnoreProxy over a QFileSystemModel) with
 * one column and roles "display" (file name), "checkState" (writable;
 * Qt.Unchecked leaves the file or folder out) and "isDir". Unchecked paths are
 * kept in the instance's `.packignore`, shared by all formats. Debris (logs,
 * crash reports, caches, `.DS_Store`, and for packs the launcher's metadata
 * sidecars) is hidden. `filesRoot` is the instance folder for a zip, the game
 * folder for a pack.
 *
 * Pack fields are remembered per instance (Export* settings), exported or not.
 * exportTo() returns a TaskWatcher; the archive is written on a worker thread
 * and cancel() stops it. One export at a time.
 */
class InstanceExportController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
	Q_PROPERTY(QString instanceName READ instanceName CONSTANT)
	Q_PROPERTY(bool packFormats READ packFormats CONSTANT)
	/// Files the game is writing may be caught half-written; the dialog warns.
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	Q_PROPERTY(QObject* files READ files CONSTANT)
	Q_PROPERTY(QModelIndex filesRoot READ filesRoot NOTIFY filesRootChanged)

	/// "zip", "modrinth" or "curseforge". Unknown values are ignored, and
	/// so are the pack formats when `packFormats` is false.
	Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY choicesChanged)
	Q_PROPERTY(QString packName READ packName WRITE setPackName NOTIFY choicesChanged)
	Q_PROPERTY(QString packVersion READ packVersion WRITE setPackVersion NOTIFY choicesChanged)
	/// CurseForge only.
	Q_PROPERTY(QString packAuthor READ packAuthor WRITE setPackAuthor NOTIFY choicesChanged)
	/// Modrinth only.
	Q_PROPERTY(QString packSummary READ packSummary WRITE setPackSummary NOTIFY choicesChanged)
	/// Mark disabled mods optional in the manifest instead of leaving them out.
	Q_PROPERTY(bool includeOptional READ includeOptional WRITE setIncludeOptional NOTIFY choicesChanged)
	/// CurseForge only: whether to state a memory figure, and which (MiB).
	Q_PROPERTY(bool recommendMemory READ recommendMemory WRITE setRecommendMemory NOTIFY choicesChanged)
	Q_PROPERTY(int recommendedMemory READ recommendedMemory WRITE setRecommendedMemory NOTIFY choicesChanged)
	/// Whether exportTo() would go ahead with what is filled in: a
	/// Modrinth pack must have a version.
	Q_PROPERTY(bool ready READ ready NOTIFY choicesChanged)

	Q_PROPERTY(QString lastOutput READ lastOutput NOTIFY lastOutputChanged)
	/// Why the last exportTo() did not start; a task that fails later says so in its watcher's `error`.
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit InstanceExportController(InstancePtr instance,
									  QObject* parent = nullptr);
	~InstanceExportController() override;

	QString instanceId() const;
	QString instanceName() const;
	bool packFormats() const;
	bool running() const;
	QObject* files() const;
	QModelIndex filesRoot() const;

	QString format() const;
	void setFormat(const QString& format);
	QString packName() const
	{
		return m_packName;
	}
	void setPackName(const QString& name);
	QString packVersion() const
	{
		return m_packVersion;
	}
	void setPackVersion(const QString& version);
	QString packAuthor() const
	{
		return m_packAuthor;
	}
	void setPackAuthor(const QString& author);
	QString packSummary() const
	{
		return m_packSummary;
	}
	void setPackSummary(const QString& summary);
	bool includeOptional() const
	{
		return m_includeOptional;
	}
	void setIncludeOptional(bool include);
	bool recommendMemory() const
	{
		return m_recommendMemory;
	}
	void setRecommendMemory(bool recommend);
	int recommendedMemory() const
	{
		return m_recommendedMemory;
	}
	void setRecommendedMemory(int mib);
	bool ready() const;

	QString lastOutput() const
	{
		return m_lastOutput;
	}
	QString error() const
	{
		return m_error;
	}

	Q_INVOKABLE QString fileExtension() const;
	/// A file:// URL in the home folder, named after the pack or instance, with fileExtension().
	Q_INVOKABLE QString suggestedFileUrl() const;
	/// @p urlOrPath: a file:// URL or a path; the format's extension is added if
	/// missing. Saves the choices first. Returns a TaskWatcher, or null with
	/// `error` set (or while another export runs).
	Q_INVOKABLE QObject* exportTo(const QString& urlOrPath);
	/// The watcher then fails with `task.wasAborted()` true; no error message is needed.
	Q_INVOKABLE void cancel();
	/// For a dialog closed without exporting.
	Q_INVOKABLE void saveChoices();
	Q_INVOKABLE void showInFolder() const;

  signals:
	void runningChanged();
	void filesRootChanged();
	void choicesChanged();
	void lastOutputChanged();
	void errorChanged();

  private:
	enum class Format { Zip, ModrinthPack, CurseForgePack };

	void applyIgnoreLists();
	/* A copy of what the tree leaves out, for the worker thread while the tree stays live. */
	MMCZip::FilterFileFunction snapshotFilter() const;
	Task* buildTask(const QString& output);
	void saveInstanceIcon() const;
	void setError(const QString& error);
	void loadPackFields();
	QString ignoreFileName() const;

	InstancePtr m_instance;
	MinecraftInstance* m_minecraft = nullptr;
	Format m_format = Format::Zip;

	/* Declared in this order so they are destroyed in reverse: the tree reads the
	 * file system model, which hands work to the icon provider from its gatherer thread. */
	std::unique_ptr<QAbstractFileIconProvider> m_icons;
	std::unique_ptr<QFileSystemModel> m_fileSystem;
	std::unique_ptr<ExportFileTree> m_tree;

	QString m_packName;
	QString m_packVersion;
	QString m_packAuthor;
	QString m_packSummary;
	bool m_includeOptional = true;
	bool m_recommendMemory = false;
	int m_recommendedMemory = 0;

	Task::Ptr m_task;
	QPointer<TaskWatcher> m_watcher;
	QString m_lastOutput;
	QString m_error;
};
