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

#include "tasks/Task.h"
#include "net/NetJob.h"
#include <QUrl>
#include "settings/SettingsObject.h"
#include "BaseVersion.h"
#include "InstanceTask.h"

class InstanceCreationTask : public InstanceTask
{
	Q_OBJECT
  public:
	/* @p loaderUid/@p loaderVersion name a mod loader component to install
	 * alongside net.minecraft, for a caller that lets someone pick a loader
	 * at creation time (the widget's NewInstanceDialog + VanillaPage do
	 * not; the QML "New instance" flow does - see
	 * models/NewInstanceController.cpp). Leave both empty for a plain
	 * vanilla instance. */
	explicit InstanceCreationTask(BaseVersionPtr version,
								  const QString& loaderUid = QString(),
								  const QString& loaderVersion = QString());

  protected:
	//! Entry point for tasks.
	virtual void executeTask() override;

  private: /* data */
	BaseVersionPtr m_version;
	QString m_loaderUid;
	QString m_loaderVersion;
};
