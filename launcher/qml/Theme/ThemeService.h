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

#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

#include "theme/ThemePalette.h"

/*
 * The C++ half of Kite.Theme: owns which palette is active and follows the OS
 * when asked to. Theme.qml forwards to this singleton; other QML goes through
 * Theme.*.
 *
 * One `changed()` signal covers palette, dark and mode together, so a binding
 * never sees the new `dark` with the previous `palette` mid-switch.
 */
class ThemeService : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(ThemePalette palette READ palette NOTIFY changed)
	Q_PROPERTY(bool dark READ dark NOTIFY changed)
	Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
	/* "amethyst", "ember", "diamond" or "grass". */
	Q_PROPERTY(QString scheme READ scheme WRITE setScheme NOTIFY changed)
	/* The "UiLook" setting: "stage" (Sahne), "calm" (Sade) or "night" (Gece),
	 * each with its own dark palette; or "classic", which follows `scheme`
	 * and `mode`. */
	Q_PROPERTY(QString look READ look WRITE setLook NOTIFY changed)

  public:
	explicit ThemeService(QObject* parent = nullptr);

	ThemePalette palette() const;
	// A look other than "classic" is always dark.
	bool dark() const { return m_dark || m_look != QStringLiteral("classic"); }
	QString mode() const { return m_mode; }

	/* Accepts "system", "dark" or "light"; anything else is rejected with a
	 * warning. */
	void setMode(const QString& mode);

	QString scheme() const { return ThemePalette::schemeName(m_scheme); }
	/* Unknown names are rejected with a warning, like setMode(). */
	void setScheme(const QString& scheme);

	/* @p scheme's palette in the current mode, for a picker that shows
	 * every scheme side by side without switching to it. */
	Q_INVOKABLE ThemePalette previewPalette(const QString& scheme) const;

	QString look() const { return m_look; }
	/* Unknown names are rejected with a warning, like setMode(). */
	void setLook(const QString& look);

  signals:
	void changed();

  private:
	/* Follows a live OS theme switch. No-op outside "system" mode or without a
	 * real change. */
	void applySystemPalette();

	// Dark by default; "system" follows the OS.
	QString m_mode = QStringLiteral("dark");
	bool m_dark = true;
	// Until Main.qml applies the persisted "UiPalette" setting.
	ThemePalette::Scheme m_scheme = ThemePalette::Scheme::Amethyst;
	// "classic" until Main.qml applies the persisted "UiLook" setting.
	QString m_look = QStringLiteral("classic");
};
