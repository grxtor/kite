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

#include <QTest>

#include "theme/Contrast.h"
#include "theme/ThemePalette.h"

/* Contract for ThemePalette. Ratios are computed from Contrast::ratio() and
 * Contrast::compositeOver(), never typed by hand, and printed with qInfo()
 * so numbers quoted elsewhere come from running this test. */

namespace
{

/// Prints one row of the ratio table and asserts it.
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

/// Every contrast requirement for one theme; shared so both themes are held
/// to the same list.
void checkTheme(const ThemePalette& p, const QString& themeName)
{
	qInfo().noquote() << "----" << themeName << "----";

	// surfaceOverlay too: popup text most often sits directly on it.
	const QList<QPair<QString, QColor>> textSurfaces = {
		{ "canvas", p.canvas },
		{ "surface", p.surface },
		{ "surfaceRaised", p.surfaceRaised },
		{ "surfaceOverlay", p.surfaceOverlay },
	};

	for (const auto& s : textSurfaces) {
		expectRatio(p.textPrimary, s.second, 4.5,
					themeName + " textPrimary on " + s.first);
		expectRatio(p.textSecondary, s.second, 4.5,
					themeName + " textSecondary on " + s.first);
		// Large/secondary text only (see textTertiary), so 3.0 instead of 4.5.
		expectRatio(p.textTertiary, s.second, 3.0,
					themeName + " textTertiary on " + s.first);
	}

	// The surface ladder must read as a ladder: surfaceRaised needs a
	// perceptibly different luminance from canvas. 0.012 fails the old ramp
	// (~0.0083-0.0088 dark) and passes the current one (~0.013-0.0142 dark,
	// ~0.10 light).
	{
		const qreal step =
			Contrast::relativeLuminance(p.surfaceRaised) -
			Contrast::relativeLuminance(p.canvas);
		const QString label = themeName + " surfaceRaised vs canvas luminance step";
		qInfo("%-52s %6.4f  (>= 0.0120)", qPrintable(label), step);
		QVERIFY2(step >= 0.012,
				 qPrintable(QString("%1: measured %2, need >= 0.012")
								.arg(label)
								.arg(step)));
	}

	expectRatio(p.textOnAccent, p.accent, 4.5,
				themeName + " textOnAccent on accent");
	expectRatio(p.accentText, p.canvas, 4.5,
				themeName + " accentText on canvas");
	expectRatio(p.accentText, p.surface, 4.5,
				themeName + " accentText on surface");

	// selection and tooltipBackground are opaque, so no compositing is needed.
	expectRatio(p.selectionText, p.selection, 4.5,
				themeName + " selectionText on selection");
	expectRatio(p.tooltipText, p.tooltipBackground, 4.5,
				themeName + " tooltipText on tooltipBackground");

	// Every "Subtle" token is translucent: flatten it onto its surface with
	// Contrast::compositeOver() before measuring the status colour on it.
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

	// WCAG 1.4.11 non-text contrast: focusRing and borderStrong against both
	// backgrounds a control might sit on.
	const QList<QPair<QString, QColor>> nonTextSurfaces = {
		{ "canvas", p.canvas },
		{ "surface", p.surface },
	};
	for (const auto& s : nonTextSurfaces) {
		expectRatio(p.borderStrong, s.second, 3.0,
					themeName + " borderStrong on " + s.first);
		expectRatio(p.focusRing, s.second, 3.0,
					themeName + " focusRing on " + s.first);

		// border is decorative and may fall under 3:1 (borderStrong carries that
		// bar). 1.28 holds its alpha above the old 20/255 (~1.19-1.24:1), which
		// was too faint to read as an edge.
		const QColor borderOverSurface =
			Contrast::compositeOver(p.border, s.second);
		const qreal borderRatio =
			Contrast::ratio(borderOverSurface, s.second);
		const QString borderLabel = themeName + " border visible over " + s.first;
		qInfo("%-52s %6.3f  (>= 1.28, decorative, not WCAG 3:1)",
			  qPrintable(borderLabel), borderRatio);
		QVERIFY2(borderRatio >= 1.28,
				 qPrintable(QString("%1: measured %2, need >= 1.28")
								.arg(borderLabel)
								.arg(borderRatio)));
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

	void test_everyScheme()
	{
		using S = ThemePalette::Scheme;
		for (S scheme : {S::Amethyst, S::Ember, S::Diamond, S::Grass}) {
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

	void test_everyLook()
	{
		using S = ThemePalette::Scheme;
		for (S scheme : {S::Stage, S::Calm, S::Night}) {
			const QString name = ThemePalette::schemeName(scheme);
			checkTheme(ThemePalette::forScheme(scheme, true), qPrintable(name));
			QCOMPARE(ThemePalette::forScheme(scheme, false),
					 ThemePalette::forScheme(scheme, true));
			QCOMPARE(ThemePalette::schemeFromName(name), scheme);
		}
	}

	/// Different themes differ; a palette equals an identical copy.
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
