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

#include <QColor>
#include <QMetaType>
#include <QObject>
#include <QString>

/* Semantic colour tokens for the QML theme, named for the role a colour
 * plays, never its hue. meshDark() and meshLight() are the same token set
 * with different values.
 *
 * A Q_GADGET: a value type like QColor, with MEMBER properties readable
 * from QML. Contrast minimums per token are asserted in
 * ThemePalette_test.cpp, not written here. */
class ThemePalette
{
	Q_GADGET

	Q_PROPERTY(QColor canvas MEMBER canvas)
	Q_PROPERTY(QColor surface MEMBER surface)
	Q_PROPERTY(QColor surfaceRaised MEMBER surfaceRaised)
	Q_PROPERTY(QColor surfaceOverlay MEMBER surfaceOverlay)
	Q_PROPERTY(QColor surfaceSunken MEMBER surfaceSunken)
	Q_PROPERTY(QColor surfaceHover MEMBER surfaceHover)

	Q_PROPERTY(QColor textPrimary MEMBER textPrimary)
	Q_PROPERTY(QColor textSecondary MEMBER textSecondary)
	Q_PROPERTY(QColor textTertiary MEMBER textTertiary)
	Q_PROPERTY(QColor textDisabled MEMBER textDisabled)
	Q_PROPERTY(QColor textOnAccent MEMBER textOnAccent)

	Q_PROPERTY(QColor accent MEMBER accent)
	Q_PROPERTY(QColor accentHover MEMBER accentHover)
	Q_PROPERTY(QColor accentPressed MEMBER accentPressed)
	Q_PROPERTY(QColor accentSubtle MEMBER accentSubtle)
	Q_PROPERTY(QColor accentText MEMBER accentText)

	Q_PROPERTY(QColor border MEMBER border)
	Q_PROPERTY(QColor borderStrong MEMBER borderStrong)
	Q_PROPERTY(QColor divider MEMBER divider)
	Q_PROPERTY(QColor focusRing MEMBER focusRing)

	Q_PROPERTY(QColor success MEMBER success)
	Q_PROPERTY(QColor successSubtle MEMBER successSubtle)
	Q_PROPERTY(QColor warning MEMBER warning)
	Q_PROPERTY(QColor warningSubtle MEMBER warningSubtle)
	Q_PROPERTY(QColor danger MEMBER danger)
	Q_PROPERTY(QColor dangerSubtle MEMBER dangerSubtle)
	Q_PROPERTY(QColor info MEMBER info)
	Q_PROPERTY(QColor infoSubtle MEMBER infoSubtle)

	Q_PROPERTY(QColor hoverOverlay MEMBER hoverOverlay)
	Q_PROPERTY(QColor pressedOverlay MEMBER pressedOverlay)
	Q_PROPERTY(QColor selection MEMBER selection)
	Q_PROPERTY(QColor selectionText MEMBER selectionText)

	Q_PROPERTY(QColor scrim MEMBER scrim)
	Q_PROPERTY(QColor shadow MEMBER shadow)
	Q_PROPERTY(QColor tooltipBackground MEMBER tooltipBackground)
	Q_PROPERTY(QColor tooltipText MEMBER tooltipText)

  public:
	/* Elevation, most recessed to most elevated: surfaceSunken (inset
	 * fields), canvas, surface (panels), surfaceRaised (cards),
	 * surfaceOverlay (popovers, modals). */
	QColor canvas;
	QColor surface;
	QColor surfaceRaised;
	QColor surfaceOverlay;
	QColor surfaceSunken;
	/* Fill of a card or row on canvas/surface under the pointer. Controls
	 * on surfaceRaised use hoverOverlay instead. */
	QColor surfaceHover;

	QColor textPrimary;
	QColor textSecondary;

	/* Only guaranteed 3:1 (large text): not for ordinary body copy. */
	QColor textTertiary;

	/* No contrast minimum: disabled controls are meant to read as
	 * unavailable. */
	QColor textDisabled;

	/* Text on a filled `accent` surface, e.g. a primary button label. */
	QColor textOnAccent;

	QColor accent;
	QColor accentHover;
	QColor accentPressed;

	/* Translucent tint (e.g. selected filter chip), so it composites over
	 * any surface. */
	QColor accentSubtle;

	/* The accent as text/icon colour on canvas or surface (links, active
	 * tab). Separate from `accent`, whose shade may not clear 4.5:1 as text. */
	QColor accentText;

	/* Decorative hairline, allowed under the WCAG 1.4.11 3:1 minimum since
	 * it only separates regions that already read as distinct. Anything that
	 * must stay perceivable on its own uses borderStrong. */
	QColor border;
	QColor borderStrong;
	QColor divider;
	QColor focusRing;

	QColor success;
	QColor successSubtle;
	QColor warning;
	QColor warningSubtle;
	QColor danger;
	QColor dangerSubtle;
	QColor info;
	QColor infoSubtle;

	QColor hoverOverlay;
	QColor pressedOverlay;

	/* Solid, not translucent: they must carry their own contrast. */
	QColor selection;
	QColor selectionText;

	/* Translucent black/white behind a modal (scrim) or under a raised
	 * surface (shadow). Not measured for text contrast. */
	QColor scrim;
	QColor shadow;
	QColor tooltipBackground;
	QColor tooltipText;

	/* The launcher's colour schemes: one accent over its own neutrals, in a
	 * dark and a light variant, picked independently of the mode. */
	enum class Scheme
	{
		Amethyst, // violet-black neutrals, electric violet
		Ember,	  // warm charcoal, lava orange
		Diamond,  // cool navy, diamond blue
		Grass,	  // hue-less graphite, Minecraft-grass green
		/* The looks' own palettes (ThemeService::look), dark only: forScheme()
		 * ignores `light` for them and the palette picker never offers them. */
		Stage, // Sahne: neutral black, a white Play
		Calm,  // Sade: ink-blue neutrals, a fresh green Play
		Night  // Gece: the website's night sky, a white Play
	};

	static ThemePalette forScheme(Scheme scheme, bool dark);
	/* "amethyst", "ember", "diamond", "grass" (the stored setting value) or
	 * a look's "stage", "calm", "night". Unknown falls back to Amethyst. */
	static Scheme schemeFromName(const QString& name);
	static QString schemeName(Scheme scheme);

	/* Amethyst's two variants, for callers (mostly tests) wanting a fixed
	 * palette independent of the user's pick; see forScheme(). */
	static ThemePalette meshDark();
	static ThemePalette meshLight();

	bool operator==(const ThemePalette& other) const;
	bool operator!=(const ThemePalette& other) const;
};

Q_DECLARE_METATYPE(ThemePalette)
