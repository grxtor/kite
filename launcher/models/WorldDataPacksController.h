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

#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <memory>

class MinecraftInstance;
class WorldList;
class DataPackFolderModel;

/*
 * QML-facing data pack manager for one world's saves/<world>/datapacks folder.
 * Holds at most one world's folder model, rebuilt when openForWorld() names a
 * different world (the same world gives the same object back). Created lazily
 * by InstanceDetails::worldDataPacks() and parented to it, so no world's folder
 * is watched until the Data packs tab picks one.
 */
class WorldDataPacksController : public QObject
{
	Q_OBJECT

	/// Sorted-by-name proxy over the current world's DataPackFolderModel; null until openForWorld() has a valid row.
	Q_PROPERTY(QObject* model READ model NOTIFY modelChanged)
	Q_PROPERTY(QString worldName READ worldName NOTIFY modelChanged)
	Q_PROPERTY(QString directory READ directory NOTIFY modelChanged)
	Q_PROPERTY(bool ready READ ready NOTIFY modelChanged)
	/// False while the instance is running.
	Q_PROPERTY(bool unlocked READ unlocked NOTIFY unlockedChanged)

  public:
	explicit WorldDataPacksController(MinecraftInstance* instance,
									  WorldList* worlds,
									  QObject* parent = nullptr);
	~WorldDataPacksController() override;

	QObject* model() const;
	QString worldName() const
	{
		return m_worldName;
	}
	QString directory() const;
	bool ready() const
	{
		return m_model != nullptr;
	}
	bool unlocked() const;

	/// Rebuilds the model unless it already shows world @p row; a no-op for an out-of-range row.
	Q_INVOKABLE void openForWorld(int row);
	Q_INVOKABLE void setEnabled(int row, bool enabled);
	Q_INVOKABLE void remove(int row);
	/// @p fileUrlOrPath: a file:// URL or a local path.
	Q_INVOKABLE bool install(const QString& fileUrlOrPath);

  signals:
	void modelChanged();
	void unlockedChanged();

  private slots:
	void onRunningStatusChanged();

  private:
	/// Borrowed; valid for this controller's lifetime (parented to the InstanceDetails that owns both).
	MinecraftInstance* m_instance;
	WorldList* m_worlds;

	QString m_worldName;
	std::unique_ptr<DataPackFolderModel> m_model;
	std::unique_ptr<QSortFilterProxyModel> m_sorted;
};
