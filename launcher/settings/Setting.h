/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
#include <QStringList>
#include <memory>

class SettingsObject;

class Setting : public QObject
{
	Q_OBJECT
  public:
	/** Synonyms are all the names used in the settings object, in order of
	 * preference; the first is the ID that identifies the setting in Kite.
	 * defVal is returned when the settings object has no value for it. */
	explicit Setting(QStringList synonyms, QVariant defVal = QVariant());

	/* The setting's ID, used to refer to it within the application. Changing
	 * it while registered is undefined behaviour. */
	virtual QString id() const
	{
		return m_synonyms.first();
	}

	/* The config file keys the setting is stored under; may differ from the
	 * ID. */
	virtual QStringList configKeys() const
	{
		return m_synonyms;
	}

	/* The value, via the SettingsObject's retrieveValue(); an invalid QVariant
	 * if the Setting has no SettingsObject. */
	virtual QVariant get() const;

	virtual QVariant defValue() const;

  signals:
	void SettingChanged(const Setting& setting, QVariant value);

	void settingReset(const Setting& setting);

  public slots:
	/* Changes the value by emitting SettingChanged(), which the SettingsObject
	 * handles. */
	virtual void set(QVariant value);

	/* Resets to default by emitting settingReset(), which the SettingsObject
	 * handles. */
	virtual void reset();

  protected:
	friend class SettingsObject;
	SettingsObject* m_storage;
	QStringList m_synonyms;
	QVariant m_defVal;
};
