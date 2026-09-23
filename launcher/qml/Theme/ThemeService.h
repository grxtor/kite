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
#include <qqmlintegration.h>

#include "theme/ThemePalette.h"

/*
 * The C++ half of MeshMC.Theme: owns which palette is active and follows the
 * OS when asked to.
 *
 * Registered as this module's QML singleton because Theme.qml -- the
 * documented public surface every other QML file binds to -- needs exactly
 * one instance to forward palette/dark/mode to. Consumers are expected to go
 * through Theme.*, not this type; it is deliberately absent from the
 * contract, ThemePalette's colours pass through untouched.
 *
 * One `changed()` signal covers palette, dark and mode's effect together, so
 * a binding can never observe, say, the new `dark` with the previous
 * `palette` still attached mid-switch.
 */
class ThemeService : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(ThemePalette palette READ palette NOTIFY changed)
	Q_PROPERTY(bool dark READ dark NOTIFY changed)
	Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
	/* "amethyst", "ember" or "diamond" -- see ThemePalette::Scheme. */
	Q_PROPERTY(QString scheme READ scheme WRITE setScheme NOTIFY changed)

  public:
	explicit ThemeService(QObject* parent = nullptr);

	ThemePalette palette() const;
	bool dark() const { return m_dark; }
	QString mode() const { return m_mode; }

	/* Accepts "system", "dark" or "light"; anything else is rejected with a
	 * warning and leaves the current mode untouched. */
	void setMode(const QString& mode);

	QString scheme() const { return ThemePalette::schemeName(m_scheme); }
	/* Unknown names are rejected with a warning, like setMode(). */
	void setScheme(const QString& scheme);

	/* @p scheme's palette in the current mode, for a picker that shows
	 * every scheme side by side without switching to it. */
	Q_INVOKABLE ThemePalette previewPalette(const QString& scheme) const;

  signals:
	void changed();

  private:
	/* Re-reads the OS palette while in "system" mode; connected to
	 * QGuiApplication so a live OS theme switch is picked up without a
	 * restart. No-op outside "system" mode, and outside a real value change. */
	void applySystemPalette();

	/* Dark by default: the launcher is designed dark-first. "system" follows
	 * the OS instead, and is one setting away. */
	QString m_mode = QStringLiteral("dark");
	bool m_dark = true;
	ThemePalette::Scheme m_scheme = ThemePalette::Scheme::Amethyst;
};
