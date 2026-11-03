/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "GpuPreference.h"

#ifdef Q_OS_WIN
#include <QDebug>
#include <vector>
#include <windows.h>

#include "settings/SettingsObject.h"
#endif

const QString GpuPreference::kSetting =
	QStringLiteral("UseHighPerformanceGpu");
const QString GpuPreference::kWrittenSetting =
	QStringLiteral("HighPerformanceGpuWritten");

namespace
{
	/* Paths cannot hold a line break, so one per line is unambiguous. */
	constexpr QChar kWrittenSeparator = QLatin1Char('\n');
} // namespace

QString GpuPreference::highPerformance()
{
	return QStringLiteral("GpuPreference=2;");
}

QString GpuPreference::valueName(const QString& javaPath)
{
	QString name = javaPath;
	return name.replace(QLatin1Char('/'), QLatin1Char('\\'));
}

bool GpuPreference::shouldWrite(const std::optional<QString>& current)
{
	return !current.has_value();
}

bool GpuPreference::shouldRemove(const std::optional<QString>& current)
{
	return current.has_value() && *current == highPerformance();
}

QString GpuPreference::encodeWritten(const QStringList& names)
{
	return names.join(kWrittenSeparator);
}

QStringList GpuPreference::decodeWritten(const QString& stored)
{
	return stored.split(kWrittenSeparator, Qt::SkipEmptyParts);
}

#ifdef Q_OS_WIN
namespace
{
	const wchar_t kPreferencesKey[] =
		L"Software\\Microsoft\\DirectX\\UserGpuPreferences";

	/// The value @p name holds, nullopt when there is none. A value that is
	/// not a string reads as an empty one: still there, still not Kite's.
	std::optional<QString> readValue(const QString& name)
	{
		HKEY key;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, kPreferencesKey, 0,
						  KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
			return std::nullopt;
		}
		const std::wstring wideName = name.toStdWString();
		DWORD type = 0;
		DWORD size = 0;
		std::optional<QString> value;
		if (RegQueryValueExW(key, wideName.c_str(), nullptr, &type, nullptr,
							 &size) == ERROR_SUCCESS) {
			value = QString();
			/* The data need not end in a NUL: one spare, zeroed. */
			std::vector<wchar_t> data(size / sizeof(wchar_t) + 1, L'\0');
			if (type == REG_SZ &&
				RegQueryValueExW(key, wideName.c_str(), nullptr, nullptr,
								 reinterpret_cast<BYTE*>(data.data()),
								 &size) == ERROR_SUCCESS) {
				value = QString::fromWCharArray(data.data());
			}
		}
		RegCloseKey(key);
		return value;
	}

	bool writeValue(const QString& name, const QString& data)
	{
		HKEY key;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, kPreferencesKey, 0, nullptr, 0,
							KEY_SET_VALUE, nullptr, &key,
							nullptr) != ERROR_SUCCESS) {
			return false;
		}
		const std::wstring wideName = name.toStdWString();
		const std::wstring wideData = data.toStdWString();
		const DWORD bytes = DWORD((wideData.size() + 1) * sizeof(wchar_t));
		const bool written =
			RegSetValueExW(key, wideName.c_str(), 0, REG_SZ,
						   reinterpret_cast<const BYTE*>(wideData.c_str()),
						   bytes) == ERROR_SUCCESS;
		RegCloseKey(key);
		return written;
	}

	bool deleteValue(const QString& name)
	{
		HKEY key;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, kPreferencesKey, 0, KEY_SET_VALUE,
						  &key) != ERROR_SUCCESS) {
			return false;
		}
		const LONG status =
			RegDeleteValueW(key, name.toStdWString().c_str());
		RegCloseKey(key);
		return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
	}
} // namespace

void GpuPreference::applyForLaunch(SettingsObject& settings,
								   const QString& javaPath)
{
	if (!settings.get(kSetting).toBool() || javaPath.isEmpty()) {
		return;
	}
	const QString name = valueName(javaPath);
	if (!shouldWrite(readValue(name))) {
		return;
	}
	if (!writeValue(name, highPerformance())) {
		qWarning() << "Could not set the high-performance graphics card for"
				   << name;
		return;
	}
	QStringList written =
		decodeWritten(settings.get(kWrittenSetting).toString());
	if (!written.contains(name, Qt::CaseInsensitive)) {
		written.append(name);
		settings.set(kWrittenSetting, encodeWritten(written));
	}
}

void GpuPreference::removeWritten(SettingsObject& settings)
{
	QStringList kept;
	for (const QString& name :
		 decodeWritten(settings.get(kWrittenSetting).toString())) {
		if (shouldRemove(readValue(name)) && !deleteValue(name)) {
			qWarning() << "Could not remove the graphics card choice for"
					   << name;
			kept.append(name);
		}
	}
	settings.set(kWrittenSetting, encodeWritten(kept));
}
#endif
