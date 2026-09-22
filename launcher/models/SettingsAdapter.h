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

#include <QObject>
#include <QString>
#include <QVariant>

#include "settings/SettingsObject.h"

/*
 * QML-facing wrapper around a SettingsObject. QML cannot hold a
 * std::shared_ptr or call into Setting/SettingsObject directly (they are not
 * Q_INVOKABLE-friendly and the settings id is looked up by string from the
 * page, not by a Setting pointer held on the QML side), so this is the one
 * object a settings page binds against.
 *
 * Values coming back from QML are always JS values: numbers arrive as
 * double, so an int setting (e.g. a memory slider) would otherwise get
 * stored as "4096.0" instead of "4096". setValue() converts the incoming
 * QVariant to the type of the setting's registered default value before
 * handing it to SettingsObject, so the config file keeps the type it always
 * had.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class SettingsAdapter : public QObject
{
	Q_OBJECT

  public:
	explicit SettingsAdapter(SettingsObjectPtr settings,
							  QObject* parent = nullptr);

	/// Current value of setting @p id, or an invalid QVariant if @p id is
	/// unknown.
	Q_INVOKABLE QVariant value(const QString& id) const;
	/// Registered default value of setting @p id, or an invalid QVariant if
	/// @p id is unknown.
	Q_INVOKABLE QVariant defaultValue(const QString& id) const;
	/// Whether a setting with this id is registered.
	Q_INVOKABLE bool contains(const QString& id) const;
	/// Stores @p value for setting @p id, converted to the type of that
	/// setting's default value first. No-op (with a warning) if @p id is
	/// unknown.
	Q_INVOKABLE void setValue(const QString& id, const QVariant& value);
	/// Reverts setting @p id to its registered default.
	Q_INVOKABLE void reset(const QString& id);

  signals:
	/// Emitted whenever a wrapped setting's effective value changes,
	/// whether from setValue(), reset(), or anything else that changes the
	/// underlying SettingsObject (another page, a plugin, ...).
	void valueChanged(const QString& id, const QVariant& value);

  private:
	SettingsObjectPtr m_settings;
};
