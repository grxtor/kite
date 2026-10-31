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
#include <QString>
#include <QVariant>

#include "settings/SettingsObject.h"

/*
 * QML-facing wrapper around a SettingsObject: the one object a settings page
 * binds against. Values from QML are JS values (numbers arrive as double), so
 * setValue() converts them to the type of the setting's registered default;
 * otherwise an int setting would be stored as "4096.0". Also hosts the
 * settings-adjacent actions applyProxySettings() and checkExternalTool().
 */
class SettingsAdapter : public QObject
{
	Q_OBJECT

  public:
	explicit SettingsAdapter(SettingsObjectPtr settings,
							  QObject* parent = nullptr);

	/// Invalid QVariant if @p id is unknown.
	Q_INVOKABLE QVariant value(const QString& id) const;
	/// Invalid QVariant if @p id is unknown.
	Q_INVOKABLE QVariant defaultValue(const QString& id) const;
	Q_INVOKABLE bool contains(const QString& id) const;
	/// Converted to the type of the setting's default first. No-op (with a warning) if @p id is unknown.
	Q_INVOKABLE void setValue(const QString& id, const QVariant& value);
	Q_INVOKABLE void reset(const QString& id);

	/// Applies a proxy configuration at once: the QML Proxy section saves each
	/// field as typed, with no "Apply" step. @p proxyType: "None", "Default",
	/// "SOCKS5" or "HTTP".
	Q_INVOKABLE void applyProxySettings(const QString& proxyType,
										const QString& addr, int port,
										const QString& user,
										const QString& password);

	/// Checks whether @p path looks like a working install of @p tool ("jprofiler",
	/// "jvisualvm" or "mcedit"). Empty string if fine, else a user-facing reason.
	Q_INVOKABLE QString checkExternalTool(const QString& tool,
										  const QString& path) const;

  signals:
	/// Emitted whenever a wrapped setting's effective value changes, from any source.
	void valueChanged(const QString& id, const QVariant& value);

  private:
	SettingsObjectPtr m_settings;
};
