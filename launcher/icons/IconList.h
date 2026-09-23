/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include <QMutex>
#include <QAbstractListModel>
#include <QFile>
#include <QDir>
#include <QtGui/QIcon>
#include <QColor>
#include <QHash>
#include <memory>

#include "MMCIcon.h"
#include "settings/Setting.h"

#include "QObjectPtr.h"

class QFileSystemWatcher;

class IconList : public QAbstractListModel
{
	Q_OBJECT
  public:
	/* QML-only role, additive alongside the Qt::DisplayRole/UserRole a
	 * QListView already gets from data() -- see roleNames(). Numbered like
	 * InstanceList's own QML-only roles (Qt::UserRole + 10 and up) to leave
	 * room without colliding if Qt::UserRole itself is ever repurposed
	 * here. */
	enum Roles { IsBuiltinRole = Qt::UserRole + 10 };

	explicit IconList(const QStringList& builtinPaths, QString path,
					  QObject* parent = 0);
	virtual ~IconList() {};

	QIcon getIcon(const QString& key) const;
	/* The icon's characteristic colour, for tinting whatever surrounds it:
	 * an average weighted by opacity and saturation, so a mostly grey icon
	 * with a coloured accent is tinted by the accent. Cached per key until
	 * the icon changes; invalid if the icon has no visible pixels. */
	QColor tint(const QString& key) const;
	int getIconIndex(const QString& key) const;
	QString getDirectory() const;

	virtual QVariant data(const QModelIndex& index,
						  int role = Qt::DisplayRole) const override;
	virtual int
	rowCount(const QModelIndex& parent = QModelIndex()) const override;
	/* Names Qt::DisplayRole/Qt::UserRole/IsBuiltinRole as `name`/`key`/
	 * `isBuiltin` so a QML delegate (an icon picker grid) can bind to them
	 * by name, the way every other QML-facing model here does. Additive:
	 * the numeric roles a QListView already gets from data() do not move. */
	virtual QHash<int, QByteArray> roleNames() const override;

	virtual QStringList mimeTypes() const override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action,
							  int row, int column,
							  const QModelIndex& parent) override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

	bool addThemeIcon(const QString& key);
	bool addIcon(const QString& key, const QString& name, const QString& path,
				 const IconType type);
	void saveIcon(const QString& key, const QString& path,
				  const char* format) const;
	bool deleteIcon(const QString& key);
	bool iconFileExists(const QString& key) const;

	void installIcons(const QStringList& iconFiles);
	void installIcon(const QString& file, const QString& name);

	const MMCIcon* icon(const QString& key) const;

	void startWatching();
	void stopWatching();

  signals:
	void iconUpdated(QString key);

  private:
	// hide copy constructor
	IconList(const IconList&) = delete;
	// hide assign op
	IconList& operator=(const IconList&) = delete;
	void reindex();

  public slots:
	void directoryChanged(const QString& path);

  protected slots:
	void fileChanged(const QString& path);
	void SettingChanged(const Setting& setting, QVariant value);

  private:
	shared_qobject_ptr<QFileSystemWatcher> m_watcher;
	bool is_watching;
	QMap<QString, int> name_index;
	QVector<MMCIcon> icons;
	QDir m_dir;
	mutable QHash<QString, QColor> m_tintCache;
};
