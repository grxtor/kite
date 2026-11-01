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

#include <QRegularExpression>
#include <QTest>

#include "Json.h"

class JsonTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_tryParse_valid()
	{
		QString error = "stale";
		const auto doc = Json::tryParse("{\"a\": 1}", nullptr, &error);
		QVERIFY(doc.has_value());
		QVERIFY(doc->isObject());
		QCOMPARE(doc->object().value("a").toInt(), 1);
		QVERIFY(error.isEmpty());
	}

	void test_tryParse_silentWithoutContext()
	{
		QString error;
		QVERIFY(!Json::tryParse("{not json", nullptr, &error).has_value());
		QVERIFY(!error.isEmpty());
	}

	void test_tryParse_logsContext()
	{
		QTest::ignoreMessage(QtWarningMsg,
							 QRegularExpression("^Error parsing test: .+"));
		QVERIFY(!Json::tryParse("[1,", "Error parsing test:").has_value());
	}

	void test_tryParse_emptyInput()
	{
		QVERIFY(!Json::tryParse(QByteArray()).has_value());
	}

	void test_tryParseWithOffset()
	{
		QTest::ignoreMessage(
			QtWarningMsg,
			QRegularExpression("^Error while parsing test at \\d+ reason: .+"));
		QString error;
		QVERIFY(!Json::tryParseWithOffset("{\"a\":", "Error while parsing test at",
										  &error)
					 .has_value());
		QVERIFY(!error.isEmpty());

		const auto doc = Json::tryParseWithOffset("[]", "unused");
		QVERIFY(doc.has_value());
		QVERIFY(doc->isArray());
	}
};

QTEST_GUILESS_MAIN(JsonTest)

#include "Json_test.moc"
