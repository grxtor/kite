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
#include <QHash>
#include <QStringList>
#include <QVector>

#include "modplatform/atlauncher/ATLPackManifest.h"

/*
 * An ATLauncher pack's optional mods, to tick before it installs: the
 * core, widget-free port of AtlOptionalModListModel (ui/pages/modplatform/
 * atlauncher/AtlOptionalModDialog.cpp), for a QML dialog to bind to.
 *
 * The same rules as there: the pack's own `selected` flags are where it
 * starts; ticking a mod ticks what it depends on and unticks the others in
 * its group; unticking it unticks what depends on it, and a hidden
 * dependency (a library) goes once nothing needs it any more. "Recommended"
 * and "None" reset the whole list. Hidden mods are rows too - they carry
 * the dependency rules - with a `hidden` role a dialog leaves them out by.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class AtlOptionalModsModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ count CONSTANT)
	Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		DescriptionRole,
		SelectedRole,
		RecommendedRole,
		HiddenRole,
		GroupRole
	};

	explicit AtlOptionalModsModel(const QVector<ATLauncher::VersionMod>& mods,
								  QObject* parent = nullptr);

	int count() const
	{
		return int(m_mods.size());
	}
	int selectedCount() const;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	Q_INVOKABLE void toggle(int row);
	Q_INVOKABLE void setSelected(int row, bool selected);
	Q_INVOKABLE void selectRecommended();
	Q_INVOKABLE void clearAll();

	/// What ATLauncher::PackInstallTask is told to install.
	QVector<QString> selectedNames() const;

  signals:
	void selectionChanged();

  private:
	void setMod(int index, bool enable);

	QVector<ATLauncher::VersionMod> m_mods;
	QHash<QString, int> m_index;
	QHash<QString, bool> m_selection;
	/// Mods that pulled a dependency in, by the dependency's name.
	QHash<QString, QStringList> m_dependants;
};
