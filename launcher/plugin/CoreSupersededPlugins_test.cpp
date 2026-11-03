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

#include "plugin/CoreSupersededPlugins.h"

// A stale .mmco of a plugin core took over is refused, whatever its case.
class CoreSupersededPluginsTest : public QObject
{
	Q_OBJECT

  private slots:
	void errorOracleIsCore()
	{
		QVERIFY(findCoreSupersededPlugin(QStringLiteral("ErrorOracle")));
		QVERIFY(findCoreSupersededPlugin(QStringLiteral("errororacle")));
		QVERIFY(!QString(findCoreSupersededPlugin(QStringLiteral("ErrorOracle"))->detail).isEmpty());
	}

	void otherPluginsLoad()
	{
		QVERIFY(!findCoreSupersededPlugin(QStringLiteral("GitVersioning")));
		QVERIFY(!findCoreSupersededPlugin(QString()));
	}
};

QTEST_GUILESS_MAIN(CoreSupersededPluginsTest)

#include "CoreSupersededPlugins_test.moc"
