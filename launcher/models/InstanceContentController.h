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

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "modplatform/ContentListExport.h"

class MinecraftInstance;
class ModFolderModel;
class ModMetadataIndex;
class ContentFilterModel;
class ContentUpdateController;
class ModHealthModel;
class Mod;
class QTimer;

/*
 * ContentTab.qml's backend: one searchable list per kind, bulk
 * enable/disable/remove, the project page, the configs folder and "Export
 * list". Rows are addressed by ContentFilterModel's `key`, not row number, so
 * a selection survives sorting and searching. Created lazily by InstanceDetails
 * and parented to it; every object handed to QML is parented to this controller.
 *
 * Kinds: "mods", "coremods", "resourcepacks", "shaderpacks", "texturepacks",
 * "datapacks"; `kinds` lists the ones this instance has now:
 *   - coremods: legacy Forge, or when the coremods folder already has files
 *     (checked without building the folder model, which would create it).
 *   - texturepacks: only instances that predate resource packs.
 *   - datapacks: while GlobalDataPacksEnabled is on, at GlobalDataPacksPath.
 * `health` is the mods folder's health check, redone shortly after the folder
 * or the loader changes.
 */
class InstanceContentController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QStringList kinds READ kinds NOTIFY kindsChanged)
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	Q_PROPERTY(bool hasModLoader READ hasModLoader NOTIFY hasModLoaderChanged)
	Q_PROPERTY(QString configsDir READ configsDir CONSTANT)
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
	Q_PROPERTY(QObject* updater READ updater CONSTANT)
	/// Missing, switched-off and wrong-version dependencies, clashes, duplicates (ModHealthModel).
	Q_PROPERTY(QObject* health READ health CONSTANT)

  public:
	explicit InstanceContentController(MinecraftInstance* instance,
									   QObject* parent = nullptr);
	~InstanceContentController() override;

	QStringList kinds() const;
	bool running() const;
	bool hasModLoader() const
	{
		return m_hasModLoader;
	}
	QString configsDir() const;
	QString lastError() const
	{
		return m_lastError;
	}
	QObject* updater() const;
	QObject* health() const;

	/// Builds the folder model if needed; null if the kind is missing. For ContentUpdateController.
	std::shared_ptr<ModFolderModel> folderFor(const QString& kind) const
	{
		return folderModel(kind);
	}
	/// For rows changed in a way the folder model does not notice (a forgotten origin).
	void refreshRows(const QString& kind, const QStringList& keys);

	/// A ContentFilterModel, or null if this instance does not have @p kind now.
	Q_INVOKABLE QObject* model(const QString& kind);
	Q_INVOKABLE QString dir(const QString& kind) const;
	/// Always for kinds the game reloads itself (resource, shader, data packs);
	/// otherwise only while it is not running.
	Q_INVOKABLE bool changesAllowed(const QString& kind) const;
	Q_INVOKABLE int setEnabled(const QString& kind, const QStringList& keys,
							   bool enabled);
	Q_INVOKABLE int remove(const QString& kind, const QStringList& keys);
	/// @p fileUrlOrPath: a file:// URL or a local path.
	Q_INVOKABLE bool install(const QString& kind,
							 const QString& fileUrlOrPath);

	/// The list as text, for the rows with @p keys (every row when
	/// empty). @p format: "html", "markdown", "plaintext", "json", "csv"
	/// or "custom"; @p fields any of "version", "authors", "url",
	/// "filename" (ignored by "custom", which uses @p lineTemplate).
	Q_INVOKABLE QString exportText(const QString& kind, const QStringList& keys,
								   const QString& format,
								   const QStringList& fields,
								   const QString& lineTemplate) const;
	Q_INVOKABLE QString exportExtension(const QString& format) const;
	Q_INVOKABLE QString exampleLine(const QString& format) const;
	/// <Documents>/<@p name><extension> as a file:// URL, with characters no file system takes removed.
	Q_INVOKABLE QString suggestedExportUrl(const QString& name,
										   const QString& format) const;
	/// UTF-8; false with lastError set on failure.
	Q_INVOKABLE bool saveText(const QString& fileUrlOrPath,
							  const QString& text);

	/// The version the archive declares first, the one recorded at install time second.
	static ContentListExport::Item exportItemFor(const Mod& mod,
												 const ModMetadataIndex* index);
	static ContentListExport::Format formatFromString(const QString& format);
	static ContentListExport::Fields fieldsFromStrings(const QStringList& fields);
	static QString renderExport(const QList<ContentListExport::Item>& items,
								const QString& format,
								const QStringList& fields,
								const QString& lineTemplate);
	static bool kindAllowsChangesWhileRunning(const QString& kind);
	/// file:// URL or plain path -> local path.
	static QString localPath(const QString& fileUrlOrPath);

  signals:
	void kindsChanged();
	void runningChanged();
	void hasModLoaderChanged();
	void lastErrorChanged();

  private:
	std::shared_ptr<ModFolderModel> folderModel(const QString& kind) const;
	ContentFilterModel* filterModel(const QString& kind) const;
	bool showsCoreMods() const;
	bool globalDataPacksEnabled() const;
	void refreshHasModLoader();
	/// Waits for changes to settle: files are read one at a time, each announcing itself.
	void scheduleHealthCheck();
	void runHealthCheck();
	void onGlobalDataPacksChanged();
	void setLastError(const QString& error);

  private:
	MinecraftInstance* m_instance = nullptr;
	/// kind -> list, created on first model() call; parented to this.
	QHash<QString, ContentFilterModel*> m_models;
	/// Folder models watched here (not by InstanceDetails); stopped in the destructor.
	QList<std::shared_ptr<ModFolderModel>> m_watched;
	bool m_hasModLoader = false;
	QString m_lastError;
	ContentUpdateController* m_updater = nullptr;
	ModHealthModel* m_health = nullptr;
	/// Single-shot, restarted by every change: a folder being read is checked once, after the last file.
	QTimer* m_healthTimer = nullptr;
};
