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

#include "theme/ThemePalette.h"

bool ThemePalette::operator==(const ThemePalette& other) const
{
	return canvas == other.canvas && surface == other.surface &&
		   surfaceRaised == other.surfaceRaised &&
		   surfaceOverlay == other.surfaceOverlay &&
		   surfaceSunken == other.surfaceSunken &&

		   textPrimary == other.textPrimary &&
		   textSecondary == other.textSecondary &&
		   textTertiary == other.textTertiary &&
		   textDisabled == other.textDisabled &&
		   textOnAccent == other.textOnAccent &&

		   accent == other.accent && accentHover == other.accentHover &&
		   accentPressed == other.accentPressed &&
		   accentSubtle == other.accentSubtle &&
		   accentText == other.accentText &&

		   border == other.border && borderStrong == other.borderStrong &&
		   divider == other.divider && focusRing == other.focusRing &&

		   success == other.success && successSubtle == other.successSubtle &&
		   warning == other.warning && warningSubtle == other.warningSubtle &&
		   danger == other.danger && dangerSubtle == other.dangerSubtle &&
		   info == other.info && infoSubtle == other.infoSubtle &&

		   hoverOverlay == other.hoverOverlay &&
		   pressedOverlay == other.pressedOverlay &&
		   selection == other.selection &&
		   selectionText == other.selectionText &&

		   scrim == other.scrim && shadow == other.shadow &&
		   tooltipBackground == other.tooltipBackground &&
		   tooltipText == other.tooltipText;
}

bool ThemePalette::operator!=(const ThemePalette& other) const
{
	return !(*this == other);
}

namespace
{
	/* What differs between schemes. Everything else -- status colours,
	 * overlays, scrims -- is shared per mode, so switching scheme never
	 * changes what "danger" or "success" looks like. */
	struct SchemeColors
	{
		QColor canvas, surface, surfaceRaised, surfaceOverlay, surfaceSunken;
		QColor textPrimary, textSecondary, textTertiary, textDisabled;
		QColor accent, accentHover, accentPressed, accentText, textOnAccent;
		QColor borderStrong, selection, selectionText, tooltipBackground;
	};

	QColor withAlpha(QColor color, int alpha)
	{
		color.setAlpha(alpha);
		return color;
	}

	ThemePalette build(const SchemeColors& c, bool dark)
	{
		ThemePalette p;

		p.canvas = c.canvas;
		p.surface = c.surface;
		p.surfaceRaised = c.surfaceRaised;
		p.surfaceOverlay = c.surfaceOverlay;
		p.surfaceSunken = c.surfaceSunken;

		p.textPrimary = c.textPrimary;
		p.textSecondary = c.textSecondary;
		p.textTertiary = c.textTertiary;
		p.textDisabled = c.textDisabled;
		p.textOnAccent = c.textOnAccent;

		p.accent = c.accent;
		p.accentHover = c.accentHover;
		p.accentPressed = c.accentPressed;
		p.accentSubtle = withAlpha(c.accent, dark ? 41 : 31); // ~16% / ~12%
		p.accentText = c.accentText;

		// Hairlines are translucent so they read the same over every
		// surface; see the header comment for why they sit under 3:1.
		// Alpha widened from 20 (~8%, measured ~1.19-1.24:1 composited over a
		// panel -- barely there) to a step that composites to a visibly
		// distinct edge (~1.3-1.6:1) without approaching borderStrong's own
		// 3:1 floor; see ThemePalette_test.cpp's "border visible over
		// surface" check.
		p.border = dark ? QColor(0xFF, 0xFF, 0xFF, 38) : QColor(0x00, 0x00, 0x00, 32);
		p.borderStrong = c.borderStrong;
		p.divider = p.border;
		// A light theme's bright accent (Ember's orange) can sit under the
		// 3:1 a focus indicator needs; its darker text shade never does.
		p.focusRing = dark ? c.accent : c.accentText;

		if (dark) {
			p.success = QColor(0x4A, 0xDE, 0x80);
			p.warning = QColor(0xFB, 0xBF, 0x24);
			p.danger = QColor(0xFB, 0x71, 0x85);
			p.info = QColor(0x60, 0xA5, 0xFA);
		} else {
			p.success = QColor(0x15, 0x6B, 0x35);
			p.warning = QColor(0x8A, 0x4B, 0x06);
			p.danger = QColor(0xB0, 0x10, 0x3A);
			p.info = QColor(0x1D, 0x4E, 0xD8);
		}
		const int subtle = dark ? 41 : 31;
		p.successSubtle = withAlpha(p.success, subtle);
		p.warningSubtle = withAlpha(p.warning, subtle);
		p.dangerSubtle = withAlpha(p.danger, subtle);
		p.infoSubtle = withAlpha(p.info, subtle);

		p.hoverOverlay = dark ? QColor(0xFF, 0xFF, 0xFF, 15) : QColor(0x00, 0x00, 0x00, 13);
		p.pressedOverlay = dark ? QColor(0xFF, 0xFF, 0xFF, 31) : QColor(0x00, 0x00, 0x00, 26);
		p.selection = c.selection;
		p.selectionText = c.selectionText;

		p.scrim = QColor(0x00, 0x00, 0x00, dark ? 140 : 115);
		p.shadow = QColor(0x00, 0x00, 0x00, dark ? 102 : 51);
		p.tooltipBackground = c.tooltipBackground;
		p.tooltipText = dark ? c.textPrimary : QColor(0xF6, 0xF6, 0xF9);

		return p;
	}

	/* Neutrals carry no hue at all, so nothing but the accent is coloured
	 * -- the fix for the earlier teal-grey that read as washed out. The
	 * violet filled surface is deep enough for white labels; lighter
	 * accentText is what reads as text on the dark surfaces. */
	SchemeColors amethyst(bool dark)
	{
		if (dark)
			// canvas unchanged; surface/surfaceRaised/surfaceOverlay widened
			// so the ladder reads as distinct layers instead of a near-flat
			// +8/+8/+9 ramp -- see design-plan.md §3's proposed Amethyst-dark
			// ramp table.
			return {QColor(0x0D, 0x0D, 0x11), QColor(0x19, 0x19, 0x20), QColor(0x23, 0x23, 0x30),
					QColor(0x2E, 0x2E, 0x3D), QColor(0x08, 0x08, 0x0B),
					QColor(0xF4, 0xF4, 0xF7), QColor(0xB9, 0xB9, 0xC6), QColor(0x8B, 0x8B, 0x9A),
					QColor(0x5C, 0x5C, 0x69),
					QColor(0x76, 0x57, 0xF7), QColor(0x86, 0x6A, 0xFF), QColor(0x62, 0x44, 0xE0),
					QColor(0xA9, 0x93, 0xFF), QColor(0xFF, 0xFF, 0xFF),
					QColor(0x70, 0x70, 0x82), QColor(0x2B, 0x22, 0x4A), QColor(0xF4, 0xF4, 0xF7),
					QColor(0x2A, 0x2A, 0x33)};
		return {QColor(0xF3, 0xF3, 0xF6), QColor(0xFA, 0xFA, 0xFC), QColor(0xFF, 0xFF, 0xFF),
				QColor(0xFF, 0xFF, 0xFF), QColor(0xE8, 0xE8, 0xEE),
				QColor(0x13, 0x13, 0x18), QColor(0x48, 0x48, 0x56), QColor(0x60, 0x60, 0x70),
				QColor(0xA0, 0xA0, 0xAE),
				QColor(0x6A, 0x4A, 0xEE), QColor(0x5B, 0x3C, 0xDC), QColor(0x4C, 0x30, 0xC2),
				QColor(0x56, 0x36, 0xD6), QColor(0xFF, 0xFF, 0xFF),
				QColor(0x6E, 0x6E, 0x7E), QColor(0xE6, 0xE0, 0xFF), QColor(0x2C, 0x1C, 0x7A),
				QColor(0x13, 0x13, 0x18)};
	}

	/* Warm, but only just: neutrals with a trace of warmth so the orange
	 * belongs, without drifting into brown. Dark text on the orange --
	 * white on a lava orange cannot reach 4.5:1. */
	SchemeColors ember(bool dark)
	{
		if (dark)
			// Same widening methodology as Amethyst (design-plan.md §3,
			// Wave 0b), scaled to this ramp's own warmer, tighter per-channel
			// shape rather than copying Amethyst's deltas verbatim.
			return {QColor(0x10, 0x0E, 0x0D), QColor(0x1B, 0x18, 0x17), QColor(0x28, 0x23, 0x21),
					QColor(0x36, 0x30, 0x28), QColor(0x0A, 0x09, 0x08),
					QColor(0xF7, 0xF3, 0xF0), QColor(0xC6, 0xBC, 0xB5), QColor(0x96, 0x8B, 0x85),
					QColor(0x63, 0x5A, 0x55),
					QColor(0xFF, 0x7A, 0x1A), QColor(0xFF, 0x8F, 0x3D), QColor(0xE8, 0x68, 0x0C),
					QColor(0xFF, 0x9A, 0x52), QColor(0x1F, 0x0E, 0x02),
					QColor(0x7A, 0x70, 0x6A), QColor(0x3D, 0x26, 0x16), QColor(0xF7, 0xF3, 0xF0),
					QColor(0x2D, 0x28, 0x25)};
		return {QColor(0xF6, 0xF3, 0xF1), QColor(0xFC, 0xFA, 0xF9), QColor(0xFF, 0xFF, 0xFF),
				QColor(0xFF, 0xFF, 0xFF), QColor(0xEC, 0xE7, 0xE3),
				QColor(0x1A, 0x15, 0x12), QColor(0x54, 0x49, 0x41), QColor(0x6C, 0x60, 0x58),
				QColor(0xAA, 0xA0, 0x99),
				QColor(0xF2, 0x6B, 0x0F), QColor(0xFF, 0x7E, 0x24), QColor(0xD9, 0x5C, 0x06),
				QColor(0xA8, 0x45, 0x00), QColor(0x1F, 0x0E, 0x02),
				QColor(0x74, 0x69, 0x62), QColor(0xFF, 0xE3, 0xCC), QColor(0x5C, 0x28, 0x00),
				QColor(0x1A, 0x15, 0x12)};
	}

	/* Cool navy with a bright diamond blue; like Ember, the bright filled
	 * surface takes dark text in the dark variant. */
	SchemeColors diamond(bool dark)
	{
		if (dark)
			// Same widening methodology as Amethyst (design-plan.md §3,
			// Wave 0b), scaled to this ramp's own looser, uneven navy shape
			// rather than copying Amethyst's deltas verbatim.
			return {QColor(0x0A, 0x0D, 0x14), QColor(0x12, 0x18, 0x23), QColor(0x1C, 0x25, 0x34),
					QColor(0x29, 0x34, 0x48), QColor(0x06, 0x09, 0x0F),
					QColor(0xEE, 0xF3, 0xFF), QColor(0xAF, 0xBA, 0xCF), QColor(0x80, 0x8C, 0xA4),
					QColor(0x53, 0x5D, 0x71),
					QColor(0x3D, 0x9B, 0xFF), QColor(0x5C, 0xAC, 0xFF), QColor(0x26, 0x84, 0xEA),
					QColor(0x6D, 0xB4, 0xFF), QColor(0x03, 0x14, 0x29),
					QColor(0x68, 0x74, 0x8C), QColor(0x14, 0x2F, 0x52), QColor(0xEE, 0xF3, 0xFF),
					QColor(0x22, 0x2B, 0x3B)};
		return {QColor(0xF1, 0xF4, 0xF9), QColor(0xF9, 0xFB, 0xFE), QColor(0xFF, 0xFF, 0xFF),
				QColor(0xFF, 0xFF, 0xFF), QColor(0xE5, 0xEA, 0xF2),
				QColor(0x0F, 0x15, 0x22), QColor(0x45, 0x4F, 0x64), QColor(0x5C, 0x67, 0x7D),
				QColor(0x9C, 0xA5, 0xB6),
				QColor(0x1D, 0x6C, 0xE3), QColor(0x17, 0x5D, 0xCC), QColor(0x13, 0x4F, 0xB0),
				QColor(0x16, 0x5A, 0xC4), QColor(0xFF, 0xFF, 0xFF),
				QColor(0x67, 0x71, 0x84), QColor(0xDA, 0xE8, 0xFF), QColor(0x0B, 0x33, 0x75),
				QColor(0x0F, 0x15, 0x22)};
	}
	/* Hue-less graphite neutrals -- literally so (R == G == B at every
	 * step), not merely low-saturation like the other three schemes' own
	 * "neutrals" (Amethyst's carry a cool violet cast, Ember's a warm one,
	 * Diamond's a cool navy one). The user rejected an earlier attempt at
	 * this scheme for tinting its *surfaces* green -- a wash of "faded
	 * greenish tints" over every panel read as sickly, not Minecraft-like.
	 * Green here lives only in the accent family (accent/Hover/Pressed/
	 * Subtle/Text) and the selection tint, the same place every other
	 * scheme keeps its own hue -- never in canvas/surface/surfaceRaised/
	 * surfaceOverlay/surfaceSunken. Values re-derived and contrast-checked
	 * directly against Contrast::ratio()/relativeLuminance() (not
	 * hand-typed) before landing here; see ThemePalette_test.cpp. */
	SchemeColors grass(bool dark)
	{
		if (dark)
			// A saturated Minecraft-grass green (the official launcher's own
			// PLAY button and a grass block's top face both sit in this
			// range) -- bright enough that near-black text clears 4.5:1
			// (measured 5.64:1), the same "dark text on a bright accent"
			// shape Ember/Diamond's dark variants already use.
			return {QColor(0x0D, 0x0D, 0x0D), QColor(0x19, 0x19, 0x19), QColor(0x23, 0x23, 0x23),
					QColor(0x2E, 0x2E, 0x2E), QColor(0x08, 0x08, 0x08),
					QColor(0xF4, 0xF4, 0xF4), QColor(0xB9, 0xB9, 0xB9), QColor(0x8B, 0x8B, 0x8B),
					QColor(0x5C, 0x5C, 0x5C),
					QColor(0x56, 0x9C, 0x3D), QColor(0x64, 0xAC, 0x48), QColor(0x3D, 0x7A, 0x28),
					QColor(0x7E, 0xD9, 0x57), QColor(0x0A, 0x12, 0x06),
					QColor(0x70, 0x70, 0x70), QColor(0x1E, 0x2E, 0x17), QColor(0xF4, 0xF4, 0xF4),
					QColor(0x24, 0x24, 0x24)};
		// A deep enough forest green that white text clears 4.5:1 (measured
		// 6.49:1) -- the light variant's own fill needs a darker green than
		// the dark variant's, the same shape Amethyst/Diamond's light
		// variants already use with white-on-accent.
		return {QColor(0xF3, 0xF3, 0xF3), QColor(0xFA, 0xFA, 0xFA), QColor(0xFF, 0xFF, 0xFF),
				QColor(0xFF, 0xFF, 0xFF), QColor(0xE8, 0xE8, 0xE8),
				QColor(0x13, 0x13, 0x13), QColor(0x48, 0x48, 0x48), QColor(0x60, 0x60, 0x60),
				QColor(0xA0, 0xA0, 0xA0),
				// accentText a distinct, darker shade of accent rather than a
				// verbatim copy -- the margin pattern Amethyst/Ember/Diamond's
				// own light variants each use for their own accentText, even
				// though accent alone already clears 4.5:1 as text (~6.5:1).
				QColor(0x2E, 0x6B, 0x1B), QColor(0x25, 0x58, 0x16), QColor(0x1D, 0x47, 0x11),
				QColor(0x24, 0x57, 0x14), QColor(0xFF, 0xFF, 0xFF),
				QColor(0x60, 0x60, 0x60), QColor(0xDC, 0xED, 0xCB), QColor(0x17, 0x31, 0x10),
				QColor(0x13, 0x13, 0x13)};
	}
} // namespace

ThemePalette ThemePalette::forScheme(Scheme scheme, bool dark)
{
	switch (scheme) {
		case Scheme::Ember:
			return build(ember(dark), dark);
		case Scheme::Diamond:
			return build(diamond(dark), dark);
		case Scheme::Grass:
			return build(grass(dark), dark);
		case Scheme::Amethyst:
			break;
	}
	return build(amethyst(dark), dark);
}

ThemePalette::Scheme ThemePalette::schemeFromName(const QString& name)
{
	if (name == QStringLiteral("ember"))
		return Scheme::Ember;
	if (name == QStringLiteral("diamond"))
		return Scheme::Diamond;
	if (name == QStringLiteral("grass"))
		return Scheme::Grass;
	return Scheme::Amethyst;
}

QString ThemePalette::schemeName(Scheme scheme)
{
	switch (scheme) {
		case Scheme::Ember:
			return QStringLiteral("ember");
		case Scheme::Diamond:
			return QStringLiteral("diamond");
		case Scheme::Grass:
			return QStringLiteral("grass");
		case Scheme::Amethyst:
			break;
	}
	return QStringLiteral("amethyst");
}

ThemePalette ThemePalette::meshDark()
{
	return forScheme(Scheme::Amethyst, true);
}

ThemePalette ThemePalette::meshLight()
{
	return forScheme(Scheme::Amethyst, false);
}
