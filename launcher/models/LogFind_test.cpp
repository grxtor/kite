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

#include <QStringListModel>
#include <QTest>

#include "launch/LogModel.h"
#include "models/LogFind.h"

class LogFindTest : public QObject
{
	Q_OBJECT
  private slots:

	void findsRowsIgnoringCase()
	{
		QStringListModel model(
			{"Starting game", "loading Mods", "ERROR: boom", "mods loaded"});
		const QList<int> rows =
			LogFind::matchingRows(&model, Qt::DisplayRole, "MODS");
		QCOMPARE(rows, (QList<int>{1, 3}));
	}

	void noMatchesAndEmptyNeedleGiveNothing()
	{
		QStringListModel model({"a", "b"});
		QVERIFY(LogFind::matchingRows(&model, Qt::DisplayRole, "zzz").isEmpty());
		QVERIFY(LogFind::matchingRows(&model, Qt::DisplayRole, "").isEmpty());
		QVERIFY(LogFind::matchingRows(nullptr, Qt::DisplayRole, "a").isEmpty());
	}

	// The real thing LogTab.qml searches: a LogModel's `line` role.
	void searchesALogModelsLineRole()
	{
		LogModel log;
		log.setMaxLines(100);
		log.append(MessageLevel::Message, "[Render thread/INFO]: Setting user");
		log.append(MessageLevel::Error, "[Worker/ERROR]: Failed to load");
		log.append(MessageLevel::Message, "[Render thread/INFO]: Reloading");
		QCOMPARE(LogFind::matchingRows(&log, LogModel::LineRole, "render thread"),
				 (QList<int>{0, 2}));
		QCOMPARE(LogFind::matchingRows(&log, LogModel::LineRole, "error"),
				 (QList<int>{1}));
	}
};

QTEST_GUILESS_MAIN(LogFindTest)

#include "LogFind_test.moc"
