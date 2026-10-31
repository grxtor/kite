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
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <memory>

class Setting;
class SettingsObject;

typedef std::shared_ptr<SettingsObject> SettingsObjectPtr;

/* Communicates settings between the application and a settings file; keeps a
 * list of registered Setting objects. */
class SettingsObject : public QObject
{
	Q_OBJECT
  public:
	class Lock
	{
	  public:
		Lock(SettingsObjectPtr locked) : m_locked(locked)
		{
			m_locked->suspendSave();
		}
		~Lock()
		{
			m_locked->resumeSave();
		}

	  private:
		SettingsObjectPtr m_locked;
	};

  public:
	explicit SettingsObject(QObject* parent = 0);
	virtual ~SettingsObject();
	/* Registers an override for the given setting. @p gate decides whether the
	 * override (true) or the original (false) is used. Null if the ID is
	 * already registered. */
	std::shared_ptr<Setting> registerOverride(std::shared_ptr<Setting> original,
											  std::shared_ptr<Setting> gate);

	/* Registers a passthrough for the given setting. @p gate decides whether
	 * the passthrough (true) or the original (false) is used. Null if the ID
	 * is already registered. */
	std::shared_ptr<Setting>
	registerPassthrough(std::shared_ptr<Setting> original,
						std::shared_ptr<Setting> gate);

	/* Registers the setting and connects its signals. Null if the ID is
	 * already registered. */
	std::shared_ptr<Setting> registerSetting(QStringList synonyms,
											 QVariant defVal = QVariant());

	/* Registers the setting and connects its signals. Null if the ID is
	 * already registered. */
	std::shared_ptr<Setting> registerSetting(QString id,
											 QVariant defVal = QVariant())
	{
		return registerSetting(QStringList(id), defVal);
	}

	/* The setting with the given ID, or null. */
	std::shared_ptr<Setting> getSetting(const QString& id) const;

	/* The setting's value, or an invalid QVariant if there is no such setting. */
	QVariant get(const QString& id) const;

	/* Sets the value; false if there is no such setting or it failed. */
	bool set(const QString& id, QVariant value);

	void reset(const QString& id) const;

	bool contains(const QString& id);

	/* Reloads the settings and emits signals for those that changed. */
	virtual bool reload();

	virtual void suspendSave() = 0;
	virtual void resumeSave() = 0;
  signals:
	void SettingChanged(const Setting& setting, QVariant value);

	void settingReset(const Setting& setting);

  protected slots:
	virtual void changeSetting(const Setting& setting, QVariant value) = 0;

	virtual void resetSetting(const Setting& setting) = 0;

  protected:
	void connectSignals(const Setting& setting);

	virtual QVariant retrieveValue(const Setting& setting) = 0;

	friend class Setting;

  private:
	QMap<QString, std::shared_ptr<Setting>> m_settings;

  protected:
	bool m_suspendSave = false;
	bool m_doSave = false;
};
