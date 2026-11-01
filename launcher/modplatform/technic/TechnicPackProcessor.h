/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2020-2021 MultiMC Contributors
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

#include <QString>
#include <functional>
#include "settings/SettingsObject.h"

namespace Technic
{
	// Not exported; used by SingleZipPackInstallTask, InstanceImportTask and
	// SolderPackInstallTask.
	class TechnicPackProcessor : public QObject
	{
		Q_OBJECT

	  signals:
		void succeeded();
		void failed(QString reason);

	  public:
		void run(SettingsObjectPtr globalSettings, const QString& instName,
				 const QString& instIcon, const QString& stagingPath,
				 const QString& minecraftVersion = QString(),
				 const bool isSolder = false);

		/* Gives the owner read/write (and execute on folders) over everything
		 * under @p dir; @p onFailed gets each path it could not change. */
		static void
		fixPermissions(const QString& dir,
					   const std::function<void(const QString&)>& onFailed);
	};
} // namespace Technic
