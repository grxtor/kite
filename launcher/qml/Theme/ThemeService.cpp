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

#include "qml/Theme/ThemeService.h"

#include <QDebug>
#include <QGuiApplication>
#include <QPalette>

namespace
{
	// QStyleHints::colorScheme() needs Qt 6.5, so a window colour with
	// lightnessF() < 0.5 stands for a dark system palette.
	bool systemPrefersDark()
	{
		return qGuiApp->palette().color(QPalette::Window).lightnessF() < 0.5;
	}
} // namespace

ThemeService::ThemeService(QObject* parent)
	: QObject(parent)
{
	// paletteChanged() is deprecated since Qt 6.0 but still emitted, and
	// needs no application-wide event filter.
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_DEPRECATED
	connect(qGuiApp, &QGuiApplication::paletteChanged, this,
			&ThemeService::applySystemPalette);
	QT_WARNING_POP
}

ThemePalette ThemeService::palette() const
{
	// A look brings its own palette; "classic" follows scheme and mode.
	if (m_look != QStringLiteral("classic"))
		return ThemePalette::forScheme(ThemePalette::schemeFromName(m_look), true);
	return ThemePalette::forScheme(m_scheme, m_dark);
}

void ThemeService::setLook(const QString& look)
{
	if (look != QStringLiteral("classic") && look != QStringLiteral("stage") &&
		look != QStringLiteral("calm") && look != QStringLiteral("night")) {
		qWarning() << "ThemeService: ignoring unknown look" << look;
		return;
	}
	if (look == m_look)
		return;
	m_look = look;
	emit changed();
}

void ThemeService::setScheme(const QString& scheme)
{
	const ThemePalette::Scheme parsed = ThemePalette::schemeFromName(scheme);
	if (ThemePalette::schemeName(parsed) != scheme) {
		qWarning() << "ThemeService: ignoring unknown colour scheme" << scheme;
		return;
	}
	if (parsed == m_scheme)
		return;
	m_scheme = parsed;
	emit changed();
}

ThemePalette ThemeService::previewPalette(const QString& scheme) const
{
	return ThemePalette::forScheme(ThemePalette::schemeFromName(scheme), m_dark);
}

void ThemeService::setMode(const QString& mode)
{
	if (mode != QStringLiteral("system") && mode != QStringLiteral("dark") &&
		mode != QStringLiteral("light")) {
		qWarning() << "ThemeService: ignoring unknown theme mode" << mode;
		return;
	}
	if (mode == m_mode)
		return;

	m_mode = mode;
	if (m_mode == QStringLiteral("dark"))
		m_dark = true;
	else if (m_mode == QStringLiteral("light"))
		m_dark = false;
	else
		m_dark = systemPrefersDark();

	// Announced as one unit even when `dark` is unchanged ("system" -> "light"
	// on a light system).
	emit changed();
}

void ThemeService::applySystemPalette()
{
	if (m_mode != QStringLiteral("system"))
		return;

	const bool dark = systemPrefersDark();
	if (dark == m_dark)
		return;

	m_dark = dark;
	emit changed();
}
