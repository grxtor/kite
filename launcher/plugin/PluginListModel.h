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

#include <QAbstractListModel>
#include <QVector>

#include "plugin/PluginMetadata.h"

class PluginManager;

/*
 * Every plugin module the launcher found, for the QML Settings section
 * "Installed plugins" -- what the widget PluginsDialog lists: name and
 * version, author, description, licence, source link, signature, status
 * (loaded, or why not), dependencies, and whether the user wants it
 * enabled. Like the dialog, switching a plugin on or off is remembered
 * for the next start (PluginManager::setModuleDisabled()); nothing loads
 * or unloads while the launcher runs, which `restartNeeded` reports.
 *
 * Lives in MeshMC_logic next to PluginManager; QmlShell hands one out
 * through a factory Application installs (QmlShell::pluginList()).
 */
class PluginListModel : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(int count READ count NOTIFY countChanged)
	/// A plugin was switched on or off since the launcher started.
	Q_PROPERTY(bool restartNeeded READ restartNeeded NOTIFY restartNeededChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		VersionRole,
		AuthorRole,
		DescriptionRole,
		LicenseRole,
		SourceUrlRole,
		/// A short, translated label for the signature state.
		SignatureRole,
		/// Whether the signature verified against a trusted key.
		SignedRole,
		SigningKeyRole,
		/// A short, translated label: "Loaded", or why it is not.
		StatusRole,
		/// "loaded", "discovered" or "disabled".
		StatusKindRole,
		/// The reason in words, when the loader gave one.
		StatusDetailRole,
		/// The user's wish -- not whether it actually loaded.
		EnabledRole,
		/// False for a plugin the launcher itself has absorbed.
		CanToggleRole,
		/// "name >= version (optional), ..." or empty.
		DependenciesRole,
	};

	/* @p manager is not owned and must outlive this model; null gives an
	 * empty list (and is what the test subclass passes). */
	explicit PluginListModel(PluginManager* manager, QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	int count() const;
	bool restartNeeded() const;

	/* Remembers that the user wants plugin @p row on or off from the next
	 * start, the way PluginsDialog's check box does. No-op for a plugin
	 * that cannot be toggled. */
	Q_INVOKABLE void setEnabled(int row, bool enabled);

	/// Re-reads fetchModules() and resets the model.
	void refresh();

	static QString signatureLabel(PluginSignatureState state);

  signals:
	void countChanged();
	void restartNeededChanged();

  protected:
	/* The test seam, as in PluginSurfaceModel: by default these read and
	 * write the PluginManager given to the constructor. */
	virtual QVector<PluginMetadata> fetchModules() const;
	virtual bool isUserDisabled(const QString& name) const;
	virtual void setUserDisabled(const QString& name, bool disabled);

  private:
	PluginManager* m_manager = nullptr;
	QVector<PluginMetadata> m_modules;
	bool m_restartNeeded = false;
};
