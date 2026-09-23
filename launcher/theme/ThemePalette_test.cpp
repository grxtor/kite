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

#include <QTest>

#include "theme/Contrast.h"
#include "theme/ThemePalette.h"

/*
 * Behavioural contract for ThemePalette.
 *
 * Every ratio a token has to clear is computed here from Contrast::ratio()
 * and Contrast::compositeOver(), the same way design notes should read
 * them -- never typed by hand, because a hand-typed ratio is exactly what
 * has gone stale on this project before (see Contrast_test.cpp). Every
 * checked pair is also printed with qInfo() so the numbers going into
 * design notes come from running this test, not from eyeballing hex codes.
 */

namespace
{

/// One row of the printed ratio table plus the assertion it backs. Kept as
/// a free function rather than repeating QVERIFY2 at each call site, since
/// the same "measure, print, assert" shape recurs for every token pair
/// below.
void expectRatio(const QColor& foreground, const QColor& background,
				  qreal minimum, const QString& label)
{
	const qreal measured = Contrast::ratio(foreground, background);
	qInfo("%-52s %6.3f  (>= %.2f)", qPrintable(label), measured, minimum);
	QVERIFY2(measured >= minimum,
			 qPrintable(QString("%1: measured %2, need >= %3")
							.arg(label)
							.arg(measured)
							.arg(minimum)));
}

/// Runs every contrast requirement from the ThemePalette task against one
/// theme. Shared between test_meshDark() and test_meshLight() so the two
/// themes are held to identical requirements rather than two hand-copied
/// lists that could drift apart.
void checkTheme(const ThemePalette& p, const QString& themeName)
{
	qInfo().noquote() << "----" << themeName << "----";

	const QList<QPair<QString, QColor>> textSurfaces = {
		{ "canvas", p.canvas },
		{ "surface", p.surface },
		{ "surfaceRaised", p.surfaceRaised },
	};

	for (const auto& s : textSurfaces) {
		expectRatio(p.textPrimary, s.second, 4.5,
					themeName + " textPrimary on " + s.first);
		expectRatio(p.textSecondary, s.second, 4.5,
					themeName + " textSecondary on " + s.first);
		// Large/secondary text only -- see the comment on textTertiary in
		// ThemePalette.h -- so 3.0 rather than the 4.5 used just above.
		expectRatio(p.textTertiary, s.second, 3.0,
					themeName + " textTertiary on " + s.first);
	}

	expectRatio(p.textOnAccent, p.accent, 4.5,
				themeName + " textOnAccent on accent");
	expectRatio(p.accentText, p.canvas, 4.5,
				themeName + " accentText on canvas");
	expectRatio(p.accentText, p.surface, 4.5,
				themeName + " accentText on surface");

	// selection and tooltipBackground are opaque solid fills by design
	// (see ThemePalette.h), so they carry their own contrast without
	// needing to be composited onto anything first.
	expectRatio(p.selectionText, p.selection, 4.5,
				themeName + " selectionText on selection");
	expectRatio(p.tooltipText, p.tooltipBackground, 4.5,
				themeName + " tooltipText on tooltipBackground");

	// Every "Subtle" token is a translucent tint (see ThemePalette.h), so
	// it has no contrast of its own until it is flattened onto the surface
	// it is drawn over -- Contrast::compositeOver() does that flattening
	// before the status colour is measured against it as text.
	const QList<QPair<QColor, QColor>> statusOnSubtle = {
		{ p.success, p.successSubtle },
		{ p.warning, p.warningSubtle },
		{ p.danger, p.dangerSubtle },
		{ p.info, p.infoSubtle },
	};
	const QList<QString> statusNames = { "success", "warning", "danger",
										  "info" };
	for (int i = 0; i < statusOnSubtle.size(); ++i) {
		const QColor subtleOverSurface =
			Contrast::compositeOver(statusOnSubtle[i].second, p.surface);
		expectRatio(statusOnSubtle[i].first, subtleOverSurface, 4.5,
					themeName + " " + statusNames[i] + " on " +
						statusNames[i] + "Subtle over surface");
	}

	// WCAG 1.4.11 non-text contrast: focusRing and borderStrong must stay
	// perceivable against both backgrounds a control might sit on.
	const QList<QPair<QString, QColor>> nonTextSurfaces = {
		{ "canvas", p.canvas },
		{ "surface", p.surface },
	};
	for (const auto& s : nonTextSurfaces) {
		expectRatio(p.borderStrong, s.second, 3.0,
					themeName + " borderStrong on " + s.first);
		expectRatio(p.focusRing, s.second, 3.0,
					themeName + " focusRing on " + s.first);

		// border is decorative and explicitly allowed to fall under 3:1
		// (see ThemePalette.h) -- measured and printed for visibility, but
		// not asserted on.
		const QColor borderOverSurface =
			Contrast::compositeOver(p.border, s.second);
		const qreal borderRatio =
			Contrast::ratio(borderOverSurface, s.second);
		qInfo("%-52s %6.3f  (decorative, no minimum)",
			  qPrintable(themeName + " border on " + s.first), borderRatio);
	}
}

} // namespace

class ThemePaletteTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_meshDark() { checkTheme(ThemePalette::meshDark(), "meshDark"); }

	void test_meshLight()
	{
		checkTheme(ThemePalette::meshLight(), "meshLight");
	}

	/// Every scheme, in both modes, clears the same bars as the default.
	void test_everyScheme()
	{
		using S = ThemePalette::Scheme;
		for (S scheme : {S::Amethyst, S::Ember, S::Diamond}) {
			const QString name = ThemePalette::schemeName(scheme);
			checkTheme(ThemePalette::forScheme(scheme, true),
					   qPrintable(name + QStringLiteral(" dark")));
			checkTheme(ThemePalette::forScheme(scheme, false),
					   qPrintable(name + QStringLiteral(" light")));
			QCOMPARE(ThemePalette::schemeFromName(name), scheme);
		}
		QCOMPARE(ThemePalette::schemeFromName(QStringLiteral("nonsense")),
				 S::Amethyst);
	}

	/// The two themes have to disagree somewhere, and a palette has to
	/// agree with an identical copy of itself -- the two ends of what
	/// operator==/operator!= are for.
	void test_equality()
	{
		const ThemePalette dark = ThemePalette::meshDark();
		const ThemePalette light = ThemePalette::meshLight();
		QVERIFY(dark != light);

		const ThemePalette darkCopy = dark;
		QVERIFY(darkCopy == dark);
		QVERIFY(!(darkCopy != dark));
	}
};

QTEST_GUILESS_MAIN(ThemePaletteTest)

#include "ThemePalette_test.moc"
