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

#include <QColor>
#include <QMetaType>
#include <QObject>

/*
 * The semantic colour tokens for the token-based QML theme -- "modern
 * launcher": dark-first, soft, with a strong accent.
 *
 * Every field is named for what it means (textPrimary, danger, accent...),
 * never for what it looks like -- there is no "cyan500" here, because a QML
 * component should ask for the role a colour plays, not memorise which raw
 * hue currently fills it. That is also why meshDark() and meshLight() are
 * the same token set with different values rather than two unrelated
 * colour lists: any component that binds to a token once works under
 * either theme without caring which one is active.
 *
 * Q_GADGET rather than QObject: a palette carries no identity and emits no
 * signals, it is a value copied around the way a QColor is, and MEMBER
 * properties let QML read every token without a getter written per field.
 *
 * Nothing in this header hand-types a contrast ratio. What each token has
 * to clear against what it sits on is asserted in ThemePalette_test.cpp
 * against Contrast::ratio() and Contrast::compositeOver(), not transcribed
 * into a comment where it could quietly go stale.
 */
class ThemePalette
{
	Q_GADGET

	Q_PROPERTY(QColor canvas MEMBER canvas)
	Q_PROPERTY(QColor surface MEMBER surface)
	Q_PROPERTY(QColor surfaceRaised MEMBER surfaceRaised)
	Q_PROPERTY(QColor surfaceOverlay MEMBER surfaceOverlay)
	Q_PROPERTY(QColor surfaceSunken MEMBER surfaceSunken)

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
	/* Elevation, darkest/most-recessed to lightest/most-elevated in both
	 * themes: surfaceSunken sits below canvas (inset fields), surface holds
	 * ordinary panels, surfaceRaised is for cards, surfaceOverlay is for
	 * popovers and modals -- the layer furthest off the canvas. */
	QColor canvas;
	QColor surface;
	QColor surfaceRaised;
	QColor surfaceOverlay;
	QColor surfaceSunken;

	QColor textPrimary;
	QColor textSecondary;

	/* WCAG AA only requires 3:1 for text this size or larger (18pt+, or
	 * 14pt+ bold) -- callers must not reach for textTertiary on ordinary
	 * body copy, since it is only guaranteed to clear the large-text
	 * minimum, not the normal-text one. */
	QColor textTertiary;

	/* No contrast minimum applies: a disabled control is meant to read as
	 * unavailable, not as dim body text held to the same bar. */
	QColor textDisabled;

	/* Text painted on top of a filled `accent` surface, e.g. a primary
	 * button's label. */
	QColor textOnAccent;

	QColor accent;
	QColor accentHover;
	QColor accentPressed;

	/* Tinted background, not a solid fill -- e.g. a selected filter chip.
	 * Translucent, so it always reads correctly composited over whatever
	 * surface it is painted on rather than only over one hard-coded colour. */
	QColor accentSubtle;

	/* The accent hue used AS text/icon colour directly on canvas or
	 * surface (a link, an active-tab label) -- kept separate from `accent`
	 * because a colour bright enough to read as a strong filled surface is
	 * not necessarily the shade that clears 4.5:1 as text on that theme's
	 * background, and vice versa. */
	QColor accentText;

	/* Decorative hairline. It is intentionally allowed to fall under the
	 * WCAG 1.4.11 non-text minimum of 3:1 (see the measured ratio in
	 * ThemePalette_test.cpp) because it only separates two surfaces that
	 * already read as distinct regions on their own, rather than carrying
	 * information by itself. Anything that must stay perceivable on its
	 * own -- an input outline, a required separator -- uses borderStrong
	 * instead, which the test does hold to 3:1. */
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

	/* Solid fills, not translucent overlays -- a selected row's background
	 * and a tooltip's background need to carry their own contrast without
	 * depending on what happens to be behind them. */
	QColor selection;
	QColor selectionText;

	/* Translucent black/white, meant to sit behind a modal (scrim) or
	 * under a raised surface (shadow). Neither is measured for text
	 * contrast -- nothing is ever read directly off them. */
	QColor scrim;
	QColor shadow;
	QColor tooltipBackground;
	QColor tooltipText;

	static ThemePalette meshDark();
	static ThemePalette meshLight();

	bool operator==(const ThemePalette& other) const;
	bool operator!=(const ThemePalette& other) const;
};

Q_DECLARE_METATYPE(ThemePalette)
