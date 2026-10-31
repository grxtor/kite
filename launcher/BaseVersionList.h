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

#include <QObject>
#include <QVariant>
#include <QAbstractListModel>

#include "BaseVersion.h"
#include "tasks/Task.h"
#include "QObjectPtr.h"

/* Base class for each instance type's version list, which tracks the available
 * game versions. It is not loaded on startup; check isLoaded() and run
 * getLoadTask() first. It is a QAbstractListModel, so the model methods (all
 * with defaults) decide how it shows in a list view. */
class BaseVersionList : public QAbstractListModel
{
	Q_OBJECT
  public:
	enum ModelRoles {
		VersionPointerRole = Qt::UserRole,
		VersionRole,
		VersionIdRole,
		ParentVersionRole,
		RecommendedRole,
		LatestRole,
		TypeRole,
		BranchRole,
		PathRole,
		ArchitectureRole,
		SortRole
	};
	typedef QList<int> RoleList;

	explicit BaseVersionList(QObject* parent = 0);

	/* A task that reloads the version list; it should reset the model when
	 * done. */
	virtual Task::Ptr getLoadTask() = 0;

	virtual bool isLoaded() = 0;

	virtual const BaseVersionPtr at(int i) const = 0;

	virtual int count() const = 0;

	QVariant data(const QModelIndex& index, int role) const override;
	int rowCount(const QModelIndex& parent) const override;
	int columnCount(const QModelIndex& parent) const override;
	QHash<int, QByteArray> roleNames() const override;

	virtual RoleList providesRoles() const;

	virtual BaseVersionPtr findVersion(const QString& descriptor);

	/* The recommended version; if the list has no recommended versions, the
	 * same as getLatestStable. */
	virtual BaseVersionPtr getRecommended() const;

	virtual void sortVersions() = 0;

  protected slots:
	/* Updates the list with the given versions by copying each into this one.
	 * This runs on the GUI thread because the versions the load task creates
	 * live on its thread, and Qt will not let their parents be set to an
	 * object from another thread. */
	virtual void updateListData(QList<BaseVersionPtr> versions) = 0;
};
