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

#include <QSignalSpy>
#include <QTest>

#include "models/PlayTimeSummary.h"

/* Only the no-library path: counting real play time needs a LauncherContext
 * with an InstanceList, which the InstanceList tests build for themselves. */
class PlayTimeSummaryTest : public QObject
{
	Q_OBJECT

  private slots:
	void isZeroWithoutALibrary();
};

void PlayTimeSummaryTest::isZeroWithoutALibrary()
{
	PlayTimeSummary summary;
	QSignalSpy changed(&summary, &PlayTimeSummary::totalChanged);
	QCOMPARE(summary.totalSeconds(), qint64(0));
	summary.refresh();
	QCOMPARE(summary.totalSeconds(), qint64(0));
	QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(PlayTimeSummaryTest)
#include "PlayTimeSummary_test.moc"
