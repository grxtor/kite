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

#include "BaseInstance.h"
#include "launch/LaunchTask.h"

class NullInstance : public BaseInstance
{
	Q_OBJECT
  public:
	NullInstance(SettingsObjectPtr globalSettings, SettingsObjectPtr settings,
				 const QString& rootDir)
		: BaseInstance(globalSettings, settings, rootDir)
	{
		setVersionBroken(true);
	}
	virtual ~NullInstance() {};
	void saveNow() override {}
	QSet<QString> traits() const override
	{
		return {};
	};
	QString instanceConfigFolder() const override
	{
		return instanceRoot();
	};
	shared_qobject_ptr<LaunchTask>
	createLaunchTask(AuthSessionPtr, MinecraftServerTargetPtr) override
	{
		return nullptr;
	}
	shared_qobject_ptr<Task> createUpdateTask(Net::Mode) override
	{
		return nullptr;
	}
	QProcessEnvironment createEnvironment() override
	{
		return QProcessEnvironment();
	}
	QMap<QString, QString> getVariables() const override
	{
		return QMap<QString, QString>();
	}
	IPathMatcher::Ptr getLogFileMatcher() override
	{
		return nullptr;
	}
	QString getLogFileRoot() override
	{
		return instanceRoot();
	}
	QString typeName() const override
	{
		return "Null";
	}
	bool canExport() const override
	{
		return false;
	}
	bool canEdit() const override
	{
		return false;
	}
	bool canLaunch() const override
	{
		return false;
	}
	QStringList verboseDescription(AuthSessionPtr,
								   MinecraftServerTargetPtr) override
	{
		QStringList out;
		out << "Null instance - placeholder.";
		return out;
	}
	QString modsRoot() const override
	{
		return QString();
	}
};
