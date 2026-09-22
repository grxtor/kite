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

#include <QtMath>

#include "theme/Contrast.h"

/*
 * Behavioural contract for Contrast.
 *
 * Design notes have hand-written the wrong WCAG ratio for a token more than
 * once, so nothing here is allowed to compare against a literal that was not
 * itself derived from the WCAG formula -- either the exact values (black on
 * white, a colour on itself) or a pair worked out by hand in the comment
 * next to the assertion.
 */

namespace
{

/// Inverse of the WCAG sRGB transfer function used by Contrast::relativeLuminance().
/// Reimplemented independently here, rather than reused from the production
/// code, so the boundary tests below do not just echo Contrast.cpp back at
/// itself.
qreal channelForLinear(qreal linear)
{
	return linear <= 0.0030186 ? linear * 12.92
								: 1.055 * qPow(linear, 1.0 / 2.4) - 0.055;
}

/// A grey QColor(c, c, c) has relative luminance exactly equal to
/// channelForLinear's input, because the three WCAG coefficients
/// (0.2126 + 0.7152 + 0.0722) sum to 1.
QColor grayWithLuminance(qreal luminance)
{
	const qreal c = channelForLinear(luminance);
	return QColor::fromRgbF(c, c, c);
}

/// A grey against white with a chosen contrast ratio, solved from the WCAG
/// ratio formula with white's luminance fixed at 1: ratio = (1 + 0.05) /
/// (L + 0.05), so L = 1.05 / ratio - 0.05.
QColor grayOnWhiteWithRatio(qreal targetRatio)
{
	return grayWithLuminance(1.05 / targetRatio - 0.05);
}

} // namespace

class ContrastTest : public QObject
{
	Q_OBJECT

  private slots:
	/// Black (L=0) against white (L=1): (1+0.05)/(0+0.05) = 21 exactly, up to
	/// the decimal-to-binary rounding of the 0.2126/0.7152/0.0722 literals.
	void test_blackOnWhiteIsMaximum()
	{
		QVERIFY(qAbs(Contrast::ratio(QColor(Qt::black), QColor(Qt::white)) -
					 21.0) < 0.0001);
	}

	/// Same pair, arguments swapped -- still 21, not 1/21.
	void test_whiteOnBlackIsMaximumToo()
	{
		QVERIFY(qAbs(Contrast::ratio(QColor(Qt::white), QColor(Qt::black)) -
					 21.0) < 0.0001);
	}

	/// A colour against itself divides its own luminance by itself, so this
	/// is exact regardless of what that luminance happens to be.
	void test_colourAgainstItselfIsOne()
	{
		QCOMPARE(Contrast::ratio(QColor("#336699"), QColor("#336699")), 1.0);
		QCOMPARE(Contrast::ratio(QColor(Qt::white), QColor(Qt::white)), 1.0);
		QCOMPARE(Contrast::ratio(QColor(Qt::black), QColor(Qt::black)), 1.0);
	}

	/// The lighter colour is always the numerator, whichever argument it
	/// arrives as.
	void test_ratioIsSymmetric()
	{
		const QColor a("#204080");
		const QColor b("#eeeecc");
		QCOMPARE(Contrast::ratio(a, b), Contrast::ratio(b, a));
	}

	/// Reference pair worked out by hand from the WCAG formula: pure red
	/// (255, 0, 0) linearises R=1 to 1 and has G=B=0, so its luminance is
	/// just the R coefficient, 0.2126. White's luminance is 1 (the three
	/// coefficients sum to 1). ratio = (1 + 0.05) / (0.2126 + 0.05) =
	/// 1.05 / 0.2626 = 3.998477 (long division to six places).
	void test_knownReferencePair_redOnWhite()
	{
		const qreal r = Contrast::ratio(QColor(255, 0, 0), QColor(Qt::white));
		QVERIFY(qAbs(r - 3.998477) < 0.0001);
	}

	/// AA normal text: 4.5:1. Margins of +-0.1 around the threshold keep the
	/// check well clear of QColor's internal 16-bit-per-channel rounding
	/// while still exercising the boundary rather than an arbitrary point.
	void test_meetsAA_normalText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAA(white, grayOnWhiteWithRatio(4.6)));
		QVERIFY(!Contrast::meetsAA(white, grayOnWhiteWithRatio(4.4)));
	}

	/// AA large text relaxes the same boundary to 3:1.
	void test_meetsAA_largeText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAA(white, grayOnWhiteWithRatio(3.1), true));
		QVERIFY(!Contrast::meetsAA(white, grayOnWhiteWithRatio(2.9), true));
	}

	/// AAA normal text: 7:1.
	void test_meetsAAA_normalText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAAA(white, grayOnWhiteWithRatio(7.1)));
		QVERIFY(!Contrast::meetsAAA(white, grayOnWhiteWithRatio(6.9)));
	}

	/// AAA large text relaxes the same boundary to 4.5:1.
	void test_meetsAAA_largeText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAAA(white, grayOnWhiteWithRatio(4.6), true));
		QVERIFY(!Contrast::meetsAAA(white, grayOnWhiteWithRatio(4.4), true));
	}

	/// A translucent foreground has no contrast of its own until it is
	/// flattened onto what it sits on: 128/255 alpha white over black works
	/// out to exactly (128, 128, 128), opaque.
	void test_compositeOver_flattensTranslucentForeground()
	{
		const QColor result = Contrast::compositeOver(
			QColor(255, 255, 255, 128), QColor(0, 0, 0));
		QCOMPARE(result.red(), 128);
		QCOMPARE(result.green(), 128);
		QCOMPARE(result.blue(), 128);
		QCOMPARE(result.alpha(), 255);
	}

	/// A fully opaque foreground has nothing behind it to blend in, so
	/// compositing must be a no-op.
	void test_compositeOver_opaqueForegroundIsUnchanged()
	{
		const QColor fg(30, 60, 90);
		QCOMPARE(Contrast::compositeOver(fg, QColor(Qt::white)), fg);
	}
};

QTEST_GUILESS_MAIN(ContrastTest)

#include "Contrast_test.moc"
