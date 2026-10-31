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
#include "TestUtil.h"

#include "GZip.h"
#include <random>

void fib(int& prev, int& cur)
{
	auto ret = prev + cur;
	prev = cur;
	cur = ret;
}

class GZipTest : public QObject
{
	Q_OBJECT
  private slots:

	void test_Through()
	{
		static const int size = 10 * 1024 * 1024;
		QByteArray random;
		QByteArray compressed;
		QByteArray decompressed;
		std::default_random_engine eng((std::random_device())());
		std::uniform_int_distribution<unsigned short> idis(
			0, std::numeric_limits<uint8_t>::max());

		for (int i = 0; i < size; i++) {
			random.append((char)idis(eng));
		}

		int prev = 1;
		int cur = 1;

		do {
			QByteArray copy = random;
			copy.resize(cur);
			compressed.clear();
			decompressed.clear();
			QVERIFY(GZip::zip(copy, compressed));
			QVERIFY(GZip::unzip(compressed, decompressed));
			QCOMPARE(decompressed, copy);
			fib(prev, cur);
		} while (cur < size);
	}

	void test_UnzipStopsPastTheCap()
	{
		const QByteArray plain(100000, 'a');
		QByteArray compressed;
		QByteArray out;
		QVERIFY(GZip::zip(plain, compressed));
		QVERIFY(GZip::unzip(compressed, out, plain.size()));
		QCOMPARE(out, plain);
		bool exceeded = true;
		QVERIFY(GZip::unzip(compressed, out, plain.size(), &exceeded));
		QVERIFY(!exceeded);
		QVERIFY(!GZip::unzip(compressed, out, plain.size() - 1, &exceeded));
		QVERIFY(exceeded);
		QVERIFY(!GZip::unzip(compressed, out, 0));
		QVERIFY(!GZip::unzip(QByteArray("not gzip data"), out, 10, &exceeded));
		QVERIFY(!exceeded);
		QVERIFY(GZip::unzip(compressed, out));
		QCOMPARE(out, plain);
	}
};

QTEST_GUILESS_MAIN(GZipTest)

#include "GZip_test.moc"
