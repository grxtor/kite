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

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "MMCZip.h"
#include "models/ContentDrop.h"

namespace
{
	using Entries = QList<QPair<QString, QByteArray>>;

	QString writeZip(const QString& path, const Entries& entries)
	{
		MMCZip::ZipWriter zip(path);
		if (!zip.open()) {
			return {};
		}
		for (const auto& entry : entries) {
			if (!zip.addFile(entry.first, entry.second)) {
				return {};
			}
		}
		return zip.close() ? path : QString();
	}

	QStringList kindsOf(const QVariantMap& sorted)
	{
		QStringList kinds;
		for (const QVariant& row : sorted.value("rows").toList()) {
			kinds.append(row.toMap().value("kind").toString());
		}
		return kinds;
	}
} // namespace

/* ContentDrop::sort(): a drop that is on no instance, split into what an
 * instance can take and what Import takes. */
class ContentDropTest : public QObject
{
	Q_OBJECT

  private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		m_resourcePack = writeZip(
			m_dir.filePath("pack.zip"),
			{ { "pack.mcmeta",
				R"({"pack":{"pack_format":15,"description":"x"}})" },
			  { "assets/minecraft/textures/a.png", "x" } });
		m_modpack = writeZip(m_dir.filePath("pack.mrpack"),
							 { { "modrinth.index.json", "{}" } });
		m_secondModpack = writeZip(m_dir.filePath("other.mrpack"),
								   { { "modrinth.index.json", "{}" } });
		m_unknown = m_dir.filePath("notes.txt");
		QFile file(m_unknown);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("hello");
		QVERIFY(!m_resourcePack.isEmpty() && !m_modpack.isEmpty() &&
				!m_secondModpack.isEmpty());
	}

	/// Packs and project links are an instance's; nothing is left for Import.
	void test_sort_contentGoesToAnInstance()
	{
		const QString link = "https://modrinth.com/mod/sodium";
		const QVariantMap sorted = ContentDrop::sort(
			{ QUrl::fromLocalFile(m_resourcePack).toString(), link });

		QCOMPARE(sorted.value("content").toStringList(),
				 QStringList({ QUrl::fromLocalFile(m_resourcePack).toString(),
							   link }));
		QVERIFY(sorted.value("imports").toStringList().isEmpty());
		QCOMPARE(kindsOf(sorted), QStringList({ "mods", "resourcepacks" }));
		QVERIFY(sorted.value("unknown").toString().isEmpty());
	}

	/// Modpack files and links that are no project page are Import's, in the
	/// order dropped.
	void test_sort_modpacksAndOtherLinksAreImports()
	{
		const QString link = "https://example.com/pack.zip";
		const QVariantMap sorted =
			ContentDrop::sort({ m_modpack, link, m_secondModpack });

		QCOMPARE(sorted.value("imports").toStringList(),
				 QStringList({ m_modpack, link, m_secondModpack }));
		QVERIFY(sorted.value("content").toStringList().isEmpty());
		QCOMPARE(kindsOf(sorted), QStringList({ "modpacks" }));
	}

	/// A mix keeps both lists; a file that is neither is only counted.
	void test_sort_mixAndUnknown()
	{
		const QVariantMap sorted =
			ContentDrop::sort({ m_unknown, m_modpack, m_resourcePack });

		QCOMPARE(sorted.value("content").toStringList(),
				 QStringList({ m_resourcePack }));
		QCOMPARE(sorted.value("imports").toStringList(),
				 QStringList({ m_modpack }));
		QVERIFY(!sorted.value("unknown").toString().isEmpty());

		const QVariantMap onlyUnknown = ContentDrop::sort({ m_unknown });
		QVERIFY(onlyUnknown.value("content").toStringList().isEmpty());
		QVERIFY(onlyUnknown.value("imports").toStringList().isEmpty());
		QVERIFY(onlyUnknown.value("rows").toList().isEmpty());
		QVERIFY(!onlyUnknown.value("unknown").toString().isEmpty());
	}

	void test_sort_nothingDropped()
	{
		const QVariantMap sorted = ContentDrop::sort({});
		QVERIFY(sorted.value("rows").toList().isEmpty());
		QVERIFY(sorted.value("content").toStringList().isEmpty());
		QVERIFY(sorted.value("imports").toStringList().isEmpty());
		QVERIFY(sorted.value("unknown").toString().isEmpty());
	}

  private:
	QTemporaryDir m_dir;
	QString m_resourcePack;
	QString m_modpack;
	QString m_secondModpack;
	QString m_unknown;
};

QTEST_GUILESS_MAIN(ContentDropTest)

#include "ContentDrop_test.moc"
