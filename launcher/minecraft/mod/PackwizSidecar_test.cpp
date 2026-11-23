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

#include "minecraft/mod/PackwizSidecar.h"

/* The `x-kite-*` keys for what the platform knows about a project. */
class PackwizSidecarTest : public QObject
{
	Q_OBJECT

  private slots:
	void projectInfoRoundTrips()
	{
		ModMetadataIndex::Entry entry;
		entry.fileName = "ComplementaryReimagined_r5.10.zip";
		entry.name = "Complementary Shaders - Reimagined";
		entry.slug = "complementary-reimagined";
		entry.platform = "modrinth";
		entry.projectId = "HVnmMxH1";
		entry.versionId = "RKfUB9VC";
		entry.versionNumber = "r5.10";
		entry.iconUrl = "https://cdn.modrinth.com/data/HVnmMxH1/icon.png";
		entry.description = "Enhances visuals while keeping the vanilla feel";

		const QByteArray text = Packwiz::serialize(entry);
		QVERIFY(text.contains("x-kite-icon-url"));
		QVERIFY(text.contains("x-kite-description"));

		const auto back = Packwiz::parse(text, QString());
		QCOMPARE(back.fileName, entry.fileName);
		QCOMPARE(back.versionNumber, entry.versionNumber);
		QCOMPARE(back.iconUrl, entry.iconUrl);
		QCOMPARE(back.description, entry.description);
		QCOMPARE(back.projectId, entry.projectId);
		QCOMPARE(back.versionId, entry.versionId);
	}

	void emptyInfoWritesNoKeys()
	{
		ModMetadataIndex::Entry entry;
		entry.fileName = "a.zip";
		const QByteArray text = Packwiz::serialize(entry);
		QVERIFY(!text.contains("x-kite-icon-url"));
		QVERIFY(!text.contains("x-kite-description"));
	}

	/* Written before these keys existed (the shape on users' disks). */
	void olderSidecarStillParses()
	{
		const QByteArray old =
			"filename = 'ComplementaryReimagined_r5.10.zip'\n"
			"name = 'Complementary Shaders - Reimagined'\n"
			"x-kite-installed-at = '2026-11-20T10:00:00Z'\n"
			"x-kite-slug = 'complementary-reimagined'\n"
			"\n[download]\nhash = 'abc'\nhash-format = 'sha1'\nmode = 'url'\n"
			"url = 'https://cdn.modrinth.com/data/HVnmMxH1/versions/RKfUB9VC/"
			"ComplementaryReimagined_r5.10.zip'\n"
			"\n[update.modrinth]\nmod-id = 'HVnmMxH1'\nversion = 'RKfUB9VC'\n";
		const auto entry = Packwiz::parse(old, "complementary-reimagined");
		QVERIFY(entry.isValid());
		QCOMPARE(entry.name, QStringLiteral("Complementary Shaders - Reimagined"));
		QCOMPARE(entry.platform, QStringLiteral("modrinth"));
		QCOMPARE(entry.projectId, QStringLiteral("HVnmMxH1"));
		QCOMPARE(entry.versionId, QStringLiteral("RKfUB9VC"));
		QVERIFY(entry.versionNumber.isEmpty());
		QVERIFY(entry.iconUrl.isEmpty());
		QVERIFY(entry.description.isEmpty());
	}
};

QTEST_GUILESS_MAIN(PackwizSidecarTest)

#include "PackwizSidecar_test.moc"
