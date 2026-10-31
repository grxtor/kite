/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "updater/meshupdater/GitHubRelease.h"

/*! The "digest" field of a release asset, which the updater checks the download against. */
class GitHubReleaseTest : public QObject
{
	Q_OBJECT

  private:
	//! The one asset of a one-release page; \a digest is omitted from the JSON when null.
	static GitHubReleaseAsset parseAsset(const QString& digest)
	{
		QJsonObject asset{
			{QStringLiteral("id"), 7},
			{QStringLiteral("name"), QStringLiteral("Kite-Linux.tar.gz")},
			{QStringLiteral("size"), 1024},
			{QStringLiteral("browser_download_url"),
			 QStringLiteral("https://example.invalid/Kite-Linux.tar.gz")},
		};
		if (!digest.isNull())
			asset.insert(QStringLiteral("digest"), digest);

		const QJsonObject release{
			{QStringLiteral("id"), 1},
			{QStringLiteral("tag_name"), QStringLiteral("v1.0.0")},
			{QStringLiteral("assets"), QJsonArray{asset}},
		};
		const QByteArray page =
			QJsonDocument(QJsonArray{release}).toJson(QJsonDocument::Compact);

		QList<GitHubRelease> releases;
		QString error;
		if (GitHub::parseReleasePage(page, &releases, &error) != 1 ||
			releases.size() != 1 || releases.first().assets.size() != 1) {
			return {};
		}
		return releases.first().assets.first();
	}

  private slots:
	void tst_Sha256DigestIsParsed()
	{
		const QString hex = QStringLiteral(
			"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
		const GitHubReleaseAsset asset = parseAsset(QStringLiteral("sha256:") + hex);
		QVERIFY(asset.isValid());
		QCOMPARE(asset.sha256, hex);
	}

	void tst_Sha256DigestIsLowercased()
	{
		const QString hex = QStringLiteral(
			"E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
		QCOMPARE(parseAsset(QStringLiteral("sha256:") + hex).sha256, hex.toLower());
	}

	void tst_MissingDigestLeavesItEmpty()
	{
		const GitHubReleaseAsset asset = parseAsset(QString());
		QVERIFY(asset.isValid());
		QVERIFY(asset.sha256.isEmpty());
	}

	void tst_UnusableDigestsLeaveItEmpty_data()
	{
		QTest::addColumn<QString>("digest");
		const QString hex64(64, QLatin1Char('a'));
		QTest::newRow("empty") << QStringLiteral("");
		QTest::newRow("other algorithm") << QStringLiteral("sha512:") + hex64;
		QTest::newRow("no prefix") << hex64;
		QTest::newRow("too short") << QStringLiteral("sha256:") + hex64.left(63);
		QTest::newRow("too long") << QStringLiteral("sha256:") + hex64 + QLatin1Char('a');
		QTest::newRow("not hex") << QStringLiteral("sha256:") + QString(64, QLatin1Char('g'));
	}

	void tst_UnusableDigestsLeaveItEmpty()
	{
		QFETCH(QString, digest);
		const GitHubReleaseAsset asset = parseAsset(digest);
		QVERIFY(asset.isValid());
		QVERIFY(asset.sha256.isEmpty());
	}
};

QTEST_GUILESS_MAIN(GitHubReleaseTest)

#include "GitHubRelease_test.moc"
