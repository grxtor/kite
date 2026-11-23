/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
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

#include "modplatform/ContentInfoBackfill.h"

/* The pure half of the backfill: reading the platforms' answers and deciding
 * what a sidecar lacks. No network. */
class ContentInfoBackfillTest : public QObject
{
	Q_OBJECT

  private slots:
	void parsesModrinthProjects()
	{
		const QByteArray body =
			R"([{"id":"HVnmMxH1","slug":"complementary-reimagined",)"
			R"("title":"Complementary Shaders - Reimagined",)"
			R"("description":" Vanilla feel ","icon_url":"https://cdn/x.png"},)"
			R"({"slug":"no-id"}])";
		const auto projects = ContentInfo::parseProjects(body);
		QCOMPARE(projects.size(), 1);
		const auto project = projects.value("HVnmMxH1");
		QCOMPARE(project.name, QStringLiteral("Complementary Shaders - Reimagined"));
		QCOMPARE(project.slug, QStringLiteral("complementary-reimagined"));
		QCOMPARE(project.iconUrl, QStringLiteral("https://cdn/x.png"));
		QCOMPARE(project.description, QStringLiteral("Vanilla feel"));
	}

	void parsesCurseForgeMods()
	{
		const QByteArray body =
			R"({"data":[{"id":394468,"name":"Sodium","slug":"sodium",)"
			R"("summary":"Fast","logo":{"title":"x","thumbnailUrl":"https://cf/t.png"}}]})";
		const auto projects = ContentInfo::parseProjects(body);
		QCOMPARE(projects.size(), 1);
		const auto project = projects.value("394468");
		QCOMPARE(project.name, QStringLiteral("Sodium"));
		QCOMPARE(project.iconUrl, QStringLiteral("https://cf/t.png"));
		QCOMPARE(project.description, QStringLiteral("Fast"));
	}

	void parsesVersionNumbers()
	{
		const auto modrinth = ContentInfo::parseVersionNumbers(
			R"([{"id":"RKfUB9VC","version_number":"r5.10"},{"id":"empty"}])");
		QCOMPARE(modrinth.size(), 1);
		QCOMPARE(modrinth.value("RKfUB9VC"), QStringLiteral("r5.10"));

		const auto curseForge = ContentInfo::parseVersionNumbers(
			R"({"data":[{"id":5101,"modId":394468,"displayName":"Sodium 0.6.5"}]})");
		QCOMPARE(curseForge.value("5101"), QStringLiteral("Sodium 0.6.5"));
	}

	void brokenAnswersGiveNothing()
	{
		QVERIFY(ContentInfo::parseProjects("not json").isEmpty());
		QVERIFY(ContentInfo::parseProjects(R"({"error":"x"})").isEmpty());
		QVERIFY(ContentInfo::parseVersionNumbers("").isEmpty());
	}

	void needsInfo()
	{
		ModMetadataIndex::Entry entry;
		entry.fileName = "a.zip";
		entry.platform = "modrinth";
		entry.projectId = "HVnmMxH1";
		entry.versionId = "RKfUB9VC";
		QVERIFY(ContentInfo::needsInfo(entry));

		entry.versionNumber = "r5.10";
		entry.description = "Vanilla feel";
		/* A project without a logo is not asked about again. */
		QVERIFY(!ContentInfo::needsInfo(entry));

		entry.versionNumber.clear();
		QVERIFY(ContentInfo::needsInfo(entry));
		/* An mrpack file: no version id, so no version to ask for. */
		entry.versionId.clear();
		QVERIFY(!ContentInfo::needsInfo(entry));

		ModMetadataIndex::Entry local;
		local.fileName = "b.zip";
		local.platform = "local";
		local.projectId = "x";
		QVERIFY(!ContentInfo::needsInfo(local));
	}

	void fillKeepsWhatIsThere()
	{
		ModMetadataIndex::Entry entry;
		entry.fileName = "a.zip";
		entry.platform = "modrinth";
		entry.projectId = "HVnmMxH1";
		entry.versionId = "RKfUB9VC";
		entry.name = "Recorded name";

		const QHash<QString, ContentInfo::Project> projects{
			{"HVnmMxH1", {"Platform name", "slug", "https://cdn/x.png", "Summary"}}};
		const QHash<QString, QString> versions{{"RKfUB9VC", "r5.10"}};
		QVERIFY(ContentInfo::fill(entry, projects, versions));
		QCOMPARE(entry.name, QStringLiteral("Recorded name"));
		QCOMPARE(entry.iconUrl, QStringLiteral("https://cdn/x.png"));
		QCOMPARE(entry.description, QStringLiteral("Summary"));
		QCOMPARE(entry.versionNumber, QStringLiteral("r5.10"));
		QVERIFY(entry.slug.isEmpty());

		/* Nothing new the second time. */
		QVERIFY(!ContentInfo::fill(entry, projects, versions));
		ModMetadataIndex::Entry other = entry;
		other.projectId = "unknown";
		other.iconUrl.clear();
		QVERIFY(!ContentInfo::fill(other, projects, {}));
	}
};

QTEST_GUILESS_MAIN(ContentInfoBackfillTest)

#include "ContentInfoBackfill_test.moc"
