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
class Mod;

/*
 * Everything ContentTab.qml does to an instance's content folders that the
 * widget ModFolderPage (and its CoreModFolderPage / GlobalDataPackPage
 * variants) could do: one searchable list per kind, bulk enable, disable
 * and remove over a selection, the project page and the configs folder,
 * and "Export list" through ContentListExport.
 *
 * Kinds: "mods", "coremods", "resourcepacks", "shaderpacks",
 * "texturepacks", "datapacks". `kinds` lists the ones this instance has
 * right now:
 *   - coremods only on legacy Forge (Forge on a Minecraft released before
 *     the legacy cutoff, CoreModFolderPage::shouldDisplay()), or when the
 *     coremods folder already has something in it. Checked without
 *     building the folder model, which would create the folder.
 *   - texturepacks only on instances that predate resource packs.
 *   - datapacks (the instance-wide folder for Paxi-style loaders) only
 *     while the instance's GlobalDataPacksEnabled setting is on, at
 *     GlobalDataPacksPath; the list follows both settings as they change.
 *
 * Rows are addressed by ContentFilterModel's `key`, not by row number,
 * so a selection survives sorting, searching and toggling.
 *
 * Created lazily by InstanceDetails::content() and parented to it; every
 * object handed to QML from here is parented to this controller, so the
 * QML engine never takes ownership of one.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class InstanceContentController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QStringList kinds READ kinds NOTIFY kindsChanged)
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	/// Whether the instance has a mod loader component. Mods need one; see
	/// ModFolderPage::ensureModLoaderPresent().
	Q_PROPERTY(bool hasModLoader READ hasModLoader NOTIFY hasModLoaderChanged)
	/// <game root>/config, for "Open configs folder".
	Q_PROPERTY(QString configsDir READ configsDir CONSTANT)
	/// Why the last saveText() failed; empty after a success.
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
	/// Updates, version changes, dependency checks and metadata resets
	/// for these folders (ContentUpdateController).
	Q_PROPERTY(QObject* updater READ updater CONSTANT)

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

	/// The folder model behind @p kind (building it if needed), or null -
	/// for ContentUpdateController, which works on the folders directly.
	std::shared_ptr<ModFolderModel> folderFor(const QString& kind) const
	{
		return folderModel(kind);
	}
	/// Tells the list for @p kind that the rows with @p keys changed in a
	/// way the folder model does not notice (a forgotten origin).
	void refreshRows(const QString& kind, const QStringList& keys);

	/// The searchable list for @p kind (a ContentFilterModel), or null if
	/// this instance does not have that kind right now.
	Q_INVOKABLE QObject* model(const QString& kind);
	/// The folder behind @p kind, or "" if it has none right now.
	Q_INVOKABLE QString dir(const QString& kind) const;
	/// Whether files of @p kind may be added, removed or switched now:
	/// always for the kinds the game reloads on its own (resource packs,
	/// shader packs, data packs), only while it is not running otherwise -
	/// the same split as ModPlatform::allowsChangesWhileRunning().
	Q_INVOKABLE bool changesAllowed(const QString& kind) const;
	/// Returns how many files actually changed state.
	Q_INVOKABLE int setEnabled(const QString& kind, const QStringList& keys,
							   bool enabled);
	/// Deletes the files; returns how many were removed.
	Q_INVOKABLE int remove(const QString& kind, const QStringList& keys);
	/// @p fileUrlOrPath: a file:// URL (as a QML FileDialog hands out) or
	/// a plain local path.
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
	/// ".html", ".md", ... for @p format.
	Q_INVOKABLE QString exportExtension(const QString& format) const;
	/// The starting line for a custom template, in @p format's shape.
	Q_INVOKABLE QString exampleLine(const QString& format) const;
	/// Where a save dialog should start: <Documents>/<@p name><extension>
	/// as a file:// URL, with characters no file system takes removed.
	Q_INVOKABLE QString suggestedExportUrl(const QString& name,
										   const QString& format) const;
	/// Writes @p text (UTF-8) to @p fileUrlOrPath; false and lastError
	/// set on failure.
	Q_INVOKABLE bool saveText(const QString& fileUrlOrPath,
							  const QString& text);

	/// One export row for @p mod: what the archive says about itself
	/// first, the version recorded at install time second - the order
	/// ModFolderPage::on_actionExportList_triggered() uses.
	static ContentListExport::Item exportItemFor(const Mod& mod,
												 const ModMetadataIndex* index);
	static ContentListExport::Format formatFromString(const QString& format);
	static ContentListExport::Fields fieldsFromStrings(const QStringList& fields);
	/// ContentListExport::render() with the strings above.
	static QString renderExport(const QList<ContentListExport::Item>& items,
								const QString& format,
								const QStringList& fields,
								const QString& lineTemplate);
	/// Whether a game running does not lock @p kind - see changesAllowed().
	static bool kindAllowsChangesWhileRunning(const QString& kind);
	/// file:// URL or plain path -> local path.
	static QString localPath(const QString& fileUrlOrPath);

  signals:
	void kindsChanged();
	void runningChanged();
	void hasModLoaderChanged();
	void lastErrorChanged();

  private:
	/// The folder model behind @p kind, building it if needed; null if
	/// this instance does not have that kind right now.
	std::shared_ptr<ModFolderModel> folderModel(const QString& kind) const;
	ContentFilterModel* filterModel(const QString& kind) const;
	bool showsCoreMods() const;
	bool globalDataPacksEnabled() const;
	void refreshHasModLoader();
	/// The global data pack folder moved or was switched on or off.
	void onGlobalDataPacksChanged();
	void setLastError(const QString& error);

  private:
	MinecraftInstance* m_instance = nullptr;
	/// kind -> list, created on first model() call. Owned via QObject
	/// parentage (parent is `this`).
	QHash<QString, ContentFilterModel*> m_models;
	/// The folder models this controller started watching itself (the
	/// ones InstanceDetails does not already watch), stopped again in the
	/// destructor.
	QList<std::shared_ptr<ModFolderModel>> m_watched;
	bool m_hasModLoader = false;
	QString m_lastError;
	/// Owned via QObject parentage (parent is `this`).
	ContentUpdateController* m_updater = nullptr;
};
