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

#include "SettingsAdapter.h"

#include <QDebug>

#include "core/LauncherContext.h"
#include "settings/Setting.h"
#include "tools/JProfiler.h"
#include "tools/JVisualVM.h"
#include "tools/MCEditTool.h"

SettingsAdapter::SettingsAdapter(SettingsObjectPtr settings, QObject* parent)
	: QObject(parent), m_settings(std::move(settings))
{
	if (!m_settings) {
		return;
	}

	connect(m_settings.get(), &SettingsObject::SettingChanged, this,
			[this](const Setting& setting, QVariant value) {
				emit valueChanged(setting.id(), value);
			});
	// Resetting removes the stored value, so the effective value becomes
	// the default again -- report that, not an invalid QVariant.
	connect(m_settings.get(), &SettingsObject::settingReset, this,
			[this](const Setting& setting) {
				emit valueChanged(setting.id(), setting.get());
			});
}

QVariant SettingsAdapter::value(const QString& id) const
{
	if (!m_settings) {
		return QVariant();
	}
	return m_settings->get(id);
}

QVariant SettingsAdapter::defaultValue(const QString& id) const
{
	if (!m_settings) {
		return QVariant();
	}
	auto setting = m_settings->getSetting(id);
	return setting ? setting->defValue() : QVariant();
}

bool SettingsAdapter::contains(const QString& id) const
{
	return m_settings && m_settings->contains(id);
}

void SettingsAdapter::setValue(const QString& id, const QVariant& value)
{
	if (!m_settings) {
		return;
	}
	auto setting = m_settings->getSetting(id);
	if (!setting) {
		qWarning() << "SettingsAdapter::setValue: unknown setting id" << id;
		return;
	}

	QVariant converted = value;
	QVariant defVal = setting->defValue();
	// Only convert against a default that actually pins down a type --
	// settings registered with no default (an invalid QVariant) have
	// nothing to convert to, so whatever QML sent is stored as-is.
	if (defVal.isValid() && converted.isValid() &&
		converted.metaType() != defVal.metaType() &&
		!converted.convert(defVal.metaType())) {
		qWarning() << "SettingsAdapter::setValue: could not convert value"
				   << "for" << id << "to" << defVal.typeName();
		return;
	}

	m_settings->set(id, converted);
}

void SettingsAdapter::reset(const QString& id)
{
	if (!m_settings) {
		return;
	}
	m_settings->reset(id);
}

void SettingsAdapter::applyProxySettings(const QString& proxyType,
										 const QString& addr, int port,
										 const QString& user,
										 const QString& password)
{
	if (!LAUNCHER) {
		return;
	}
	LAUNCHER->updateProxySettings(proxyType, addr, port, user, password);
}

QString SettingsAdapter::checkExternalTool(const QString& tool,
										   const QString& path) const
{
	QString error;
	bool ok = false;
	if (tool == QLatin1String("jprofiler")) {
		ok = JProfilerFactory().check(path, &error);
	} else if (tool == QLatin1String("jvisualvm")) {
		ok = JVisualVMFactory().check(path, &error);
	} else if (tool == QLatin1String("mcedit")) {
		ok = m_settings && MCEditTool(m_settings).check(path, error);
	} else {
		qWarning() << "SettingsAdapter::checkExternalTool: unknown tool"
				   << tool;
		return tr("Unknown tool.");
	}
	return ok ? QString() : error;
}
