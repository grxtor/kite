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

#include <QTest>

#include "java/GpuPreference.h"

class GpuPreferenceTest : public QObject
{
	Q_OBJECT

  private slots:
	// The software renderer and Microsoft's adapters are no graphics card; a
	// machine with one real card never gets "use the faster card".
	void graphicsCardsAreRealOnes()
	{
		QVERIFY(GpuPreference::isGraphicsCard(0x1002, false));
		QVERIFY(GpuPreference::isGraphicsCard(0x8086, false));
		QVERIFY(!GpuPreference::isGraphicsCard(0x1414, false));
		QVERIFY(!GpuPreference::isGraphicsCard(0x10DE, true));
#ifndef Q_OS_WIN
		QVERIFY(!GpuPreference::hasSeveralGraphicsCards());
#endif
	}

	void highPerformanceIsWhatWindowsWrites()
	{
		QCOMPARE(GpuPreference::highPerformance(),
				 QStringLiteral("GpuPreference=2;"));
	}

	void valueNameUsesBackslashes()
	{
		QCOMPARE(GpuPreference::valueName(
					 "C:/Program Files/Java/jdk-21/bin/javaw.exe"),
				 QStringLiteral("C:\\Program Files\\Java\\jdk-21\\bin\\javaw.exe"));
		QCOMPARE(GpuPreference::valueName("C:\\Java\\bin\\java.exe"),
				 QStringLiteral("C:\\Java\\bin\\java.exe"));
	}

	void writesOnlyWhereThereIsNoValue()
	{
		QVERIFY(GpuPreference::shouldWrite(std::nullopt));
		/* The player picked one in Windows: theirs, whichever it is. */
		QVERIFY(!GpuPreference::shouldWrite(QStringLiteral("GpuPreference=1;")));
		QVERIFY(!GpuPreference::shouldWrite(QStringLiteral("GpuPreference=0;")));
		QVERIFY(!GpuPreference::shouldWrite(GpuPreference::highPerformance()));
		QVERIFY(!GpuPreference::shouldWrite(QString()));
	}

	void removesOnlyWhatIsStillKites()
	{
		QVERIFY(GpuPreference::shouldRemove(GpuPreference::highPerformance()));
		/* Changed in Windows since Kite wrote it. */
		QVERIFY(!GpuPreference::shouldRemove(QStringLiteral("GpuPreference=1;")));
		QVERIFY(!GpuPreference::shouldRemove(QString()));
		QVERIFY(!GpuPreference::shouldRemove(std::nullopt));
	}

	void writtenListRoundTrips()
	{
		const QStringList names = {"C:\\Java\\bin\\javaw.exe",
								   "D:\\Kite\\java\\jre-17\\bin\\javaw.exe"};
		QCOMPARE(GpuPreference::decodeWritten(
					 GpuPreference::encodeWritten(names)),
				 names);
		QCOMPARE(GpuPreference::decodeWritten(QString()), QStringList());
		QCOMPARE(GpuPreference::encodeWritten({}), QString());
	}
};

QTEST_GUILESS_MAIN(GpuPreferenceTest)

#include "GpuPreference_test.moc"
