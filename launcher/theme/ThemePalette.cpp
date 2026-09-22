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

ThemePalette ThemePalette::meshDark()
{
	ThemePalette p;

	p.canvas = QColor(0x0B, 0x0E, 0x13);
	p.surface = QColor(0x12, 0x16, 0x1E);
	p.surfaceRaised = QColor(0x1B, 0x21, 0x2C);
	p.surfaceOverlay = QColor(0x24, 0x2C, 0x39);
	p.surfaceSunken = QColor(0x07, 0x09, 0x11);

	p.textPrimary = QColor(0xF3, 0xF5, 0xF8);
	p.textSecondary = QColor(0xB7, 0xC0, 0xCC);
	p.textTertiary = QColor(0x87, 0x91, 0xA1);
	p.textDisabled = QColor(0x5A, 0x64, 0x72);
	p.textOnAccent = QColor(0x00, 0x23, 0x2A);

	// The logo's cyan, used as-is: it is already bright enough to carry
	// 4.5:1 as text/icon colour on this theme's dark surfaces.
	p.accent = QColor(0x00, 0xE5, 0xFF);
	p.accentHover = QColor(0x3E, 0xEB, 0xFF);
	p.accentPressed = QColor(0x00, 0xB3, 0xC9);
	p.accentSubtle = QColor(0x00, 0xE5, 0xFF, 41); // ~16% opacity
	p.accentText = p.accent;

	p.border = QColor(0xFF, 0xFF, 0xFF, 20); // ~8% opacity, see header comment
	p.borderStrong = QColor(0x69, 0x74, 0x8A);
	p.divider = QColor(0xFF, 0xFF, 0xFF, 20); // ~8% opacity
	p.focusRing = p.accent;

	// success keeps the legacy theme's green close to as-is -- against a
	// dark surface it already clears the ratio below with room to spare.
	p.success = QColor(0x96, 0xDB, 0x59);
	p.successSubtle = QColor(0x96, 0xDB, 0x59, 41); // ~16% opacity
	p.warning = QColor(0xFF, 0xB4, 0x54);
	p.warningSubtle = QColor(0xFF, 0xB4, 0x54, 41); // ~16% opacity
	// danger is the logo's rose, lightened from #FF003C: the raw brand hue
	// (relative luminance 0.216) falls just short (4.42:1, see the report)
	// against its own subtle background over a dark surface, so it is
	// pushed lighter until it clears 4.5:1.
	p.danger = QColor(0xFF, 0x5C, 0x79);
	p.dangerSubtle = QColor(0xFF, 0x5C, 0x79, 41); // ~16% opacity
	p.info = QColor(0x5A, 0xB8, 0xFF);
	p.infoSubtle = QColor(0x5A, 0xB8, 0xFF, 41); // ~16% opacity

	p.hoverOverlay = QColor(0xFF, 0xFF, 0xFF, 15); // ~6% opacity
	p.pressedOverlay = QColor(0xFF, 0xFF, 0xFF, 31); // ~12% opacity
	p.selection = QColor(0x0F, 0x46, 0x50);
	p.selectionText = p.textPrimary;

	p.scrim = QColor(0x00, 0x00, 0x00, 140); // ~55% opacity
	p.shadow = QColor(0x00, 0x00, 0x00, 89); // ~35% opacity
	p.tooltipBackground = QColor(0x20, 0x26, 0x32);
	p.tooltipText = p.textPrimary;

	return p;
}

ThemePalette ThemePalette::meshLight()
{
	ThemePalette p;

	p.canvas = QColor(0xEE, 0xF1, 0xF5);
	p.surface = QColor(0xF7, 0xF9, 0xFC);
	p.surfaceRaised = QColor(0xFB, 0xFC, 0xFE);
	p.surfaceOverlay = QColor(0xFF, 0xFF, 0xFF);
	p.surfaceSunken = QColor(0xE3, 0xE7, 0xED);

	p.textPrimary = QColor(0x12, 0x16, 0x1D);
	p.textSecondary = QColor(0x45, 0x4C, 0x58);
	p.textTertiary = QColor(0x5C, 0x64, 0x72);
	p.textDisabled = QColor(0x9A, 0xA2, 0xAF);
	p.textOnAccent = QColor(0xFF, 0xFF, 0xFF);

	// The logo's cyan, darkened for a light surface (as specified).
	p.accent = QColor(0x00, 0x79, 0x8F);
	p.accentHover = QColor(0x00, 0x63, 0x7A);
	p.accentPressed = QColor(0x00, 0x4E, 0x61);
	p.accentSubtle = QColor(0x00, 0x79, 0x8F, 31); // ~12% opacity
	// Darkened further than the surface `accent`: #00798F on its own is
	// 4.49:1 against canvas, just under the 4.5:1 bar (see the report), so
	// accentText goes one step darker along the same hue.
	p.accentText = QColor(0x00, 0x5F, 0x73);

	p.border = QColor(0x00, 0x00, 0x00, 20); // ~8% opacity, see header comment
	p.borderStrong = QColor(0x6B, 0x72, 0x80);
	p.divider = QColor(0x00, 0x00, 0x00, 20); // ~8% opacity
	p.focusRing = p.accent;

	// Every status colour below is the brand/legacy hue driven dark enough
	// to clear 4.5:1 against its own subtle tint over a near-white surface
	// -- the light-theme mirror of what dark theme needed brightened.
	p.success = QColor(0x1E, 0x68, 0x23); // hue of the legacy #96DB59, darkened
	p.successSubtle = QColor(0x1E, 0x68, 0x23, 31); // ~12% opacity
	p.warning = QColor(0x8A, 0x53, 0x00);
	p.warningSubtle = QColor(0x8A, 0x53, 0x00, 31); // ~12% opacity
	p.danger = QColor(0xB8, 0x00, 0x35); // hue of the logo's #FF003C, darkened
	p.dangerSubtle = QColor(0xB8, 0x00, 0x35, 31); // ~12% opacity
	p.info = QColor(0x0A, 0x58, 0xAF);
	p.infoSubtle = QColor(0x0A, 0x58, 0xAF, 31); // ~12% opacity

	p.hoverOverlay = QColor(0x00, 0x00, 0x00, 13); // ~5% opacity
	p.pressedOverlay = QColor(0x00, 0x00, 0x00, 26); // ~10% opacity
	p.selection = QColor(0xD6, 0xEE, 0xF2);
	p.selectionText = QColor(0x00, 0x40, 0x4D);

	p.scrim = QColor(0x00, 0x00, 0x00, 115); // ~45% opacity
	p.shadow = QColor(0x00, 0x00, 0x00, 51); // ~20% opacity
	// Inverted relative to the theme, like most tooltips: a dark chip reads
	// clearly no matter which surface it floats over.
	p.tooltipBackground = p.textPrimary;
	p.tooltipText = QColor(0xF5, 0xF7, 0xFA);

	return p;
}
