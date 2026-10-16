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
#include <QTimeZone>

#include <memory>

#include "models/NewsModel.h"

class NewsModelTest : public QObject
{
	Q_OBJECT

	static NewsEntryPtr entry(const QString& title, const QString& link)
	{
		return std::make_shared<NewsEntry>(
			title, QStringLiteral("<p>body</p>"), link, QStringLiteral("Team"),
			QDateTime(QDate(2026, 10, 1), QTime(12, 0), QTimeZone::UTC));
	}

  private slots:
	void startsEmptyWithoutAChecker()
	{
		NewsModel model;
		QCOMPARE(model.count(), 0);
		QCOMPARE(model.rowCount(), 0);
		QVERIFY(model.latest().isEmpty());
	}

	// Rows in the order given (NewsChecker hands them over newest first),
	// the first one as `latest`, entries without a title left out.
	void setEntriesFillsRowsAndLatest()
	{
		NewsModel model;
		QSignalSpy changed(&model, &NewsModel::entriesChanged);

		model.setEntries({ entry(QStringLiteral("Newest"), QStringLiteral("https://a")),
						   entry(QStringLiteral("  "), QStringLiteral("https://b")),
						   entry(QStringLiteral("Older"), QStringLiteral("https://c")) });

		QCOMPARE(changed.count(), 1);
		QCOMPARE(model.count(), 2);
		QCOMPARE(model.latest().value("title").toString(), QStringLiteral("Newest"));
		QCOMPARE(model.latest().value("link").toString(), QStringLiteral("https://a"));

		const QModelIndex second = model.index(1, 0);
		QCOMPARE(model.data(second, NewsModel::TitleRole).toString(),
				 QStringLiteral("Older"));
		QCOMPARE(model.data(second, NewsModel::LinkRole).toString(),
				 QStringLiteral("https://c"));
		QCOMPARE(model.data(second, NewsModel::DateRole).toDateTime().date(),
				 QDate(2026, 10, 1));
		QCOMPARE(model.roleNames().value(NewsModel::TitleRole), QByteArray("title"));
	}
};

QTEST_GUILESS_MAIN(NewsModelTest)

#include "NewsModel_test.moc"
