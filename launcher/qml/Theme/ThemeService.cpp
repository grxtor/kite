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

#include "qml/Theme/ThemeService.h"

#include <QDebug>
#include <QGuiApplication>
#include <QPalette>

namespace
{
	/* QStyleHints::colorScheme() would answer this directly, but it is Qt
	 * 6.5+ and the floor here is 6.4. lightnessF() < 0.5 on the window colour
	 * is the same heuristic ui/themes/ThemeManager.cpp already uses
	 * (resolveIconTheme()) to tell a dark system palette from a light one. */
	bool systemPrefersDark()
	{
		return qGuiApp->palette().color(QPalette::Window).lightnessF() < 0.5;
	}
} // namespace

ThemeService::ThemeService(QObject* parent)
	: QObject(parent)
{
	/* QGuiApplication::paletteChanged() has been deprecated in favour of
	 * QEvent::ApplicationPaletteChange since Qt 6.0, but it is still emitted
	 * on the 6.4 floor and needs no application-wide event filter just for
	 * this one listener. */
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_DEPRECATED
	connect(qGuiApp, &QGuiApplication::paletteChanged, this,
			&ThemeService::applySystemPalette);
	QT_WARNING_POP
}

ThemePalette ThemeService::palette() const
{
	return m_dark ? ThemePalette::meshDark() : ThemePalette::meshLight();
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

	// The mode itself changed either way, so the whole theme is announced as
	// one unit even on the rare switch that leaves `dark` unchanged (e.g.
	// "system" -> "light" while the system is already light).
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
