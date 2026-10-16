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
 * Exporting one instance, for the QML dialog: the widget-free replacement
 * for ExportInstanceDialog and ExportPackDialog together.
 *
 * Three formats. "zip" writes the instance folder as it is (what the
 * library's import reads back). "modrinth" and "curseforge" write a pack
 * for that catalogue: a manifest naming the mods to download on install,
 * plus the rest of the game folder. The packs need a Minecraft instance,
 * so `packFormats` is false for anything else and the format stays "zip".
 *
 * `files` is the checkable tree both widget dialogs used (FileIgnoreProxy
 * over a QFileSystemModel), made readable for a QML TreeView: one column,
 * with roles "display" (the file name), "checkState" (writable: assigning
 * Qt.Unchecked leaves that file or folder out) and "isDir".
 * What is unchecked is kept in the instance's `.packignore`, shared by all
 * three formats as it was by the two widget dialogs. Debris nobody wants
 * in an export (logs, crash reports, caches, `.DS_Store`, and for packs
 * this launcher's own metadata sidecars) is hidden rather than offered.
 * `filesRoot` is where the tree starts: the instance folder for a zip,
 * the game folder for a pack, since nothing above it means anything to
 * another launcher.
 *
 * The pack fields are remembered per instance (the Export* settings
 * MinecraftInstance registers), whether or not the export goes ahead.
 *
 * exportTo() hands back a TaskWatcher; the archive is written on a worker
 * thread and can be stopped with cancel(). One export at a time.
 */
class InstanceExportController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
	Q_PROPERTY(QString instanceName READ instanceName CONSTANT)
	/// Whether "modrinth" and "curseforge" are on offer.
	Q_PROPERTY(bool packFormats READ packFormats CONSTANT)
	/// Whether the game is running: files it is writing may be caught
	/// half-written, which the dialog warns about before exporting.
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	/// The checkable file tree; see the class comment for its roles.
	Q_PROPERTY(QObject* files READ files CONSTANT)
	Q_PROPERTY(QModelIndex filesRoot READ filesRoot NOTIFY filesRootChanged)

	/// "zip", "modrinth" or "curseforge". Unknown values are ignored, and
	/// so are the pack formats when `packFormats` is false.
	Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY choicesChanged)
	/// Empty means "the instance's name".
	Q_PROPERTY(QString packName READ packName WRITE setPackName NOTIFY choicesChanged)
	Q_PROPERTY(QString packVersion READ packVersion WRITE setPackVersion NOTIFY choicesChanged)
	/// CurseForge only.
	Q_PROPERTY(QString packAuthor READ packAuthor WRITE setPackAuthor NOTIFY choicesChanged)
	/// Modrinth only.
	Q_PROPERTY(QString packSummary READ packSummary WRITE setPackSummary NOTIFY choicesChanged)
	/// Mark mods the user disabled as optional in the manifest, rather than
	/// leaving them out.
	Q_PROPERTY(bool includeOptional READ includeOptional WRITE setIncludeOptional NOTIFY choicesChanged)
	/// CurseForge only: whether to state a memory figure, and which (MiB).
	Q_PROPERTY(bool recommendMemory READ recommendMemory WRITE setRecommendMemory NOTIFY choicesChanged)
	Q_PROPERTY(int recommendedMemory READ recommendedMemory WRITE setRecommendedMemory NOTIFY choicesChanged)
	/// Whether exportTo() would go ahead with what is filled in: a
	/// Modrinth pack must have a version.
	Q_PROPERTY(bool ready READ ready NOTIFY choicesChanged)

	/// Where the last export that succeeded was written.
	Q_PROPERTY(QString lastOutput READ lastOutput NOTIFY lastOutputChanged)
	/// Why the last exportTo() did not start; empty otherwise. A task
	/// that starts and then fails says why in its TaskWatcher's `error`.
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

	/// ".zip", or ".mrpack" for a Modrinth pack.
	Q_INVOKABLE QString fileExtension() const;
	/// A file:// URL for the save dialog to start from: the home folder,
	/// named after the pack (or the instance), with fileExtension().
	Q_INVOKABLE QString suggestedFileUrl() const;
	/// Starts writing the export to @p urlOrPath (a file:// URL from a
	/// QML FileDialog, or a plain path), adding the format's extension if
	/// it has none. Saves the choices first, as saveChoices() does.
	/// Returns a TaskWatcher, or null with `error` set when it could not
	/// start (or another export is still running).
	Q_INVOKABLE QObject* exportTo(const QString& urlOrPath);
	/// Stops the running export, if any. Its watcher then fails with
	/// `task.wasAborted()` true, which is not worth an error message.
	Q_INVOKABLE void cancel();
	/// Remembers the unchecked files and the pack fields for the next
	/// export of this instance. For a dialog closed without exporting.
	Q_INVOKABLE void saveChoices();
	/// Opens the folder `lastOutput` is in.
	Q_INVOKABLE void showInFolder() const;

  signals:
	void runningChanged();
	void filesRootChanged();
	void choicesChanged();
	void lastOutputChanged();
	void errorChanged();

  private:
	enum class Format { Zip, ModrinthPack, CurseForgePack };

	/* The ignore lists for the current format: the debris every export
	 * hides, plus the metadata sidecars a pack must not carry. */
	void applyIgnoreLists();
	/* A copy of what the tree currently leaves out, for the export task
	 * to ask from its worker thread while the tree itself stays live. */
	MMCZip::FilterFileFunction snapshotFilter() const;
	Task* buildTask(const QString& output);
	/* Copies a custom icon into the instance folder, so the zip carries
	 * it and an import of the zip can find it again. */
	void saveInstanceIcon() const;
	void setError(const QString& error);
	void loadPackFields();
	QString ignoreFileName() const;

	InstancePtr m_instance;
	/* Null unless the instance is Minecraft-backed. */
	MinecraftInstance* m_minecraft = nullptr;
	Format m_format = Format::Zip;

	/* Declared in this order so they are destroyed in the reverse one:
	 * the tree reads the file system model, and the model hands work to
	 * the icon provider from its own gatherer thread. */
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
