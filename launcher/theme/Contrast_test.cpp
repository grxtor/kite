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

#include <QtMath>

#include "theme/Contrast.h"

/* Contract for Contrast. Nothing compares against a literal that was not
 * derived from the WCAG formula. */

namespace
{

/// Inverse of the WCAG sRGB transfer function, reimplemented here so the
/// boundary tests do not echo Contrast.cpp back at itself.
qreal channelForLinear(qreal linear)
{
	return linear <= 0.0030186 ? linear * 12.92
								: 1.055 * qPow(linear, 1.0 / 2.4) - 0.055;
}

/// A grey QColor(c, c, c) has luminance equal to channelForLinear's input,
/// since the WCAG coefficients sum to 1.
QColor grayWithLuminance(qreal luminance)
{
	const qreal c = channelForLinear(luminance);
	return QColor::fromRgbF(c, c, c);
}

/// Grey against white with the given ratio: L = 1.05 / ratio - 0.05.
QColor grayOnWhiteWithRatio(qreal targetRatio)
{
	return grayWithLuminance(1.05 / targetRatio - 0.05);
}

} // namespace

class ContrastTest : public QObject
{
	Q_OBJECT

  private slots:
	/// Black against white is exactly 21, up to the rounding of the
	/// 0.2126/0.7152/0.0722 literals.
	void test_blackOnWhiteIsMaximum()
	{
		QVERIFY(qAbs(Contrast::ratio(QColor(Qt::black), QColor(Qt::white)) -
					 21.0) < 0.0001);
	}

	/// Swapped arguments still give 21.
	void test_whiteOnBlackIsMaximumToo()
	{
		QVERIFY(qAbs(Contrast::ratio(QColor(Qt::white), QColor(Qt::black)) -
					 21.0) < 0.0001);
	}

	/// A colour against itself is exactly 1.
	void test_colourAgainstItselfIsOne()
	{
		QCOMPARE(Contrast::ratio(QColor("#336699"), QColor("#336699")), 1.0);
		QCOMPARE(Contrast::ratio(QColor(Qt::white), QColor(Qt::white)), 1.0);
		QCOMPARE(Contrast::ratio(QColor(Qt::black), QColor(Qt::black)), 1.0);
	}

	void test_ratioIsSymmetric()
	{
		const QColor a("#204080");
		const QColor b("#eeeecc");
		QCOMPARE(Contrast::ratio(a, b), Contrast::ratio(b, a));
	}

	/// Pure red on white by hand: L = 0.2126, ratio = 1.05 / 0.2626 = 3.998477.
	void test_knownReferencePair_redOnWhite()
	{
		const qreal r = Contrast::ratio(QColor(255, 0, 0), QColor(Qt::white));
		QVERIFY(qAbs(r - 3.998477) < 0.0001);
	}

	/// AA normal text: 4.5:1. The +-0.1 margins stay clear of QColor's 16-bit
	/// channel rounding.
	void test_meetsAA_normalText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAA(white, grayOnWhiteWithRatio(4.6)));
		QVERIFY(!Contrast::meetsAA(white, grayOnWhiteWithRatio(4.4)));
	}

	void test_meetsAA_largeText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAA(white, grayOnWhiteWithRatio(3.1), true));
		QVERIFY(!Contrast::meetsAA(white, grayOnWhiteWithRatio(2.9), true));
	}

	void test_meetsAAA_normalText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAAA(white, grayOnWhiteWithRatio(7.1)));
		QVERIFY(!Contrast::meetsAAA(white, grayOnWhiteWithRatio(6.9)));
	}

	void test_meetsAAA_largeText_boundary()
	{
		const QColor white(Qt::white);
		QVERIFY(Contrast::meetsAAA(white, grayOnWhiteWithRatio(4.6), true));
		QVERIFY(!Contrast::meetsAAA(white, grayOnWhiteWithRatio(4.4), true));
	}

	/// 128/255 alpha white over black flattens to opaque (128, 128, 128).
	void test_compositeOver_flattensTranslucentForeground()
	{
		const QColor result = Contrast::compositeOver(
			QColor(255, 255, 255, 128), QColor(0, 0, 0));
		QCOMPARE(result.red(), 128);
		QCOMPARE(result.green(), 128);
		QCOMPARE(result.blue(), 128);
		QCOMPARE(result.alpha(), 255);
	}

	/// An opaque foreground is unchanged by compositing.
	void test_compositeOver_opaqueForegroundIsUnchanged()
	{
		const QColor fg(30, 60, 90);
		QCOMPARE(Contrast::compositeOver(fg, QColor(Qt::white)), fg);
	}
};

QTEST_GUILESS_MAIN(ContrastTest)

#include "Contrast_test.moc"
