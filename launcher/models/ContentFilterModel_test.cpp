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
#include <QTemporaryDir>
#include <QTest>

#include "MMCZip.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "models/ContentFilterModel.h"
#include "models/InstanceContentController.h"

namespace
{
	/* A fabric mod jar with just enough metadata for the list. */
	QString writeMod(const QString& dir, const QString& file,
					 const QString& json)
	{
		const QString path = QDir(dir).filePath(file);
		MMCZip::ZipWriter zip(path);
		if (!zip.open() || !zip.addFile("fabric.mod.json", json.toUtf8())) {
			return {};
		}
		return zip.close() ? path : QString();
	}

	QStringList names(const ContentFilterModel& model)
	{
		QStringList list;
		for (int row = 0; row < model.rowCount(); ++row) {
			list << model.index(row, 0).data(ModFolderModel::NameRole).toString();
		}
		return list;
	}
} // namespace

/*
 * The search proxy over a real ModFolderModel (three jars in a temp
 * folder, parsed the way the launcher parses them), and the list export
 * InstanceContentController builds from the same rows.
 */
class ContentFilterModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void init()
	{
		QVERIFY(m_dir.isValid());
		const QString root = m_dir.path();
		const QString sodium = QStringLiteral(
			"{\"schemaVersion\":1,\"id\":\"sodium\",\"name\":\"Sodium\","
			"\"version\":\"0.6.5\",\"description\":\"Rendering engine\","
			"\"authors\":[\"jellysquid3\"],"
			"\"contact\":{\"homepage\":\"https://example.org/sodium\"}}");
		const QString lithium = QStringLiteral(
			"{\"schemaVersion\":1,\"id\":\"lithium\",\"name\":\"Lithium\","
			"\"version\":\"0.14.3\",\"description\":\"Game logic optimisations\","
			"\"authors\":[\"CaffeineMC\"]}");
		const QString api = QStringLiteral(
			"{\"schemaVersion\":1,\"id\":\"fabric-api\",\"name\":\"Fabric API\","
			"\"description\":\"Hooks for other mods\",\"authors\":[\"FabricMC\"]}");
		const bool written = !writeMod(root, "sodium.jar", sodium).isEmpty() &&
							 !writeMod(root, "lithium.jar", lithium).isEmpty() &&
							 !writeMod(root, "api.jar", api).isEmpty();
		QVERIFY(written);

		m_folder = std::make_shared<ModFolderModel>(root);
		QSignalSpy finished(m_folder.get(), &ModFolderModel::updateFinished);
		QVERIFY(m_folder->update());
		QVERIFY(finished.wait(5000));
		QCOMPARE(m_folder->size(), size_t(3));
		/* Names come from the parse, which runs on the thread pool. */
		QTRY_VERIFY_WITH_TIMEOUT(resolved(), 5000);

		m_model = std::make_unique<ContentFilterModel>();
		m_model->setFolderModel(m_folder);
	}

	void cleanup()
	{
		m_model.reset();
		m_folder.reset();
		QDir(m_dir.path()).removeRecursively();
		QDir().mkpath(m_dir.path());
	}

	void sortsByNameAndCounts()
	{
		QCOMPARE(names(*m_model),
				 (QStringList{"Fabric API", "Lithium", "Sodium"}));
		QCOMPARE(m_model->count(), 3);
		QCOMPARE(m_model->totalCount(), 3);
	}

	void searchMatchesNameAuthorDescriptionAndFile()
	{
		QSignalSpy counted(m_model.get(), &ContentFilterModel::countChanged);

		m_model->setFilterText("SODIUM");
		QCOMPARE(names(*m_model), QStringList{"Sodium"});
		QCOMPARE(m_model->count(), 1);
		QCOMPARE(m_model->totalCount(), 3);
		QVERIFY(counted.count() > 0);

		m_model->setFilterText("caffeine");
		QCOMPARE(names(*m_model), QStringList{"Lithium"});

		m_model->setFilterText("hooks");
		QCOMPARE(names(*m_model), QStringList{"Fabric API"});

		m_model->setFilterText("api.jar");
		QCOMPARE(names(*m_model), QStringList{"Fabric API"});

		m_model->setFilterText("");
		QCOMPARE(m_model->count(), 3);
	}

	void searchWithNoMatchIsEmpty()
	{
		m_model->setFilterText("does not exist");
		QCOMPARE(m_model->count(), 0);
		QVERIFY(m_model->visibleKeys().isEmpty());
	}

	void detailRoles()
	{
		m_model->setFilterText("sodium");
		const QModelIndex row = m_model->index(0, 0);
		QCOMPARE(row.data(ContentFilterModel::KeyRole).toString(),
				 QStringLiteral("sodium.jar"));
		QCOMPARE(row.data(ContentFilterModel::DescriptionRole).toString(),
				 QStringLiteral("Rendering engine"));
		QCOMPARE(row.data(ContentFilterModel::AuthorsRole).toStringList(),
				 QStringList{"jellysquid3"});
		/* No platform origin recorded: the address the jar declares. */
		QCOMPARE(row.data(ContentFilterModel::HomepageRole).toString(),
				 QStringLiteral("https://example.org/sodium"));
		QCOMPARE(row.data(ContentFilterModel::SourceRole).toString(),
				 QString());
		QVERIFY(m_model->roleNames().values().contains("homepage"));
	}

	void platformOriginWinsForHomepage()
	{
		ModMetadataIndex::Entry entry;
		entry.fileName = "sodium.jar";
		entry.platform = "modrinth";
		entry.projectId = "AANobbMI";
		entry.versionId = "abc";
		QVERIFY(m_folder->metadataIndex()->put(entry));

		m_model->setFilterText("sodium");
		const QModelIndex row = m_model->index(0, 0);
		QCOMPARE(row.data(ContentFilterModel::HomepageRole).toString(),
				 QStringLiteral("https://modrinth.com/mod/AANobbMI"));
		QCOMPARE(row.data(ContentFilterModel::SourceRole).toString(),
				 QStringLiteral("Modrinth"));
	}

	void keySurvivesDisabling()
	{
		const auto before = m_model->sourceIndexesForKeys({"lithium.jar"});
		QCOMPARE(before.size(), 1);
		QVERIFY(m_folder->setModStatus(before, ModFolderModel::Disable));
		QVERIFY(!m_folder->at(size_t(before.first().row())).enabled());

		const auto after = m_model->sourceIndexesForKeys({"lithium.jar"});
		QCOMPARE(after.size(), 1);
		QCOMPARE(ContentFilterModel::keyFor(m_folder->at(size_t(after.first().row()))),
				 QStringLiteral("lithium.jar"));
		QVERIFY(m_model->sourceIndexesForKeys({"nothing.jar"}).isEmpty());
	}

	void exportMarkdownAndCsv()
	{
		QList<ContentListExport::Item> items;
		for (size_t row = 0; row < m_folder->size(); ++row) {
			items << InstanceContentController::exportItemFor(m_folder->at(row),
															  nullptr);
		}
		std::sort(items.begin(), items.end(),
				  [](const auto& a, const auto& b) { return a.name < b.name; });

		const QString markdown = InstanceContentController::renderExport(
			items, "markdown", {"version"}, QString());
		QCOMPARE(markdown, QStringLiteral("- Fabric API\n"
										  "- Lithium [0\\.14\\.3]\n"
										  "- Sodium [0\\.6\\.5]"));

		const QString csv = InstanceContentController::renderExport(
			items, "csv", {"version", "authors"}, QString());
		QCOMPARE(csv.split('\n').last(),
				 QStringLiteral("Sodium,0.6.5,jellysquid3"));

		const QString custom = InstanceContentController::renderExport(
			items, "custom", {}, "{name} ({filename})");
		QVERIFY(custom.contains("Sodium (sodium.jar)"));
	}

	void exportVersionFallsBackToRecordedNumber()
	{
		ModMetadataIndex::Entry entry;
		entry.fileName = "api.jar";
		entry.platform = "modrinth";
		entry.projectId = "P7dR8mSH";
		entry.versionId = "opaque";
		entry.versionNumber = "0.110.0";
		QVERIFY(m_folder->metadataIndex()->put(entry));

		const auto indexes = m_model->sourceIndexesForKeys({"api.jar"});
		QCOMPARE(indexes.size(), 1);
		const auto item = InstanceContentController::exportItemFor(
			m_folder->at(size_t(indexes.first().row())),
			m_folder->metadataIndex().get());
		QCOMPARE(item.version, QStringLiteral("0.110.0"));
		QCOMPARE(item.url, QStringLiteral("https://modrinth.com/mod/P7dR8mSH"));
		QCOMPARE(item.modId, QStringLiteral("fabric-api"));
	}

	void formatAndFieldStrings()
	{
		QCOMPARE(InstanceContentController::formatFromString("HTML"),
				 ContentListExport::Format::Html);
		QCOMPARE(InstanceContentController::formatFromString("unknown"),
				 ContentListExport::Format::PlainText);
		const auto fields = InstanceContentController::fieldsFromStrings(
			{"url", "filename", "bogus"});
		QVERIFY(fields.testFlag(ContentListExport::Url));
		QVERIFY(fields.testFlag(ContentListExport::FileName));
		QVERIFY(!fields.testFlag(ContentListExport::Version));
	}

	void runningRuleAndPaths()
	{
		QVERIFY(InstanceContentController::kindAllowsChangesWhileRunning(
			"resourcepacks"));
		QVERIFY(InstanceContentController::kindAllowsChangesWhileRunning(
			"datapacks"));
		QVERIFY(!InstanceContentController::kindAllowsChangesWhileRunning("mods"));
		QVERIFY(!InstanceContentController::kindAllowsChangesWhileRunning(
			"coremods"));
		QCOMPARE(InstanceContentController::localPath("file:///tmp/a b.jar"),
				 QStringLiteral("/tmp/a b.jar"));
		QCOMPARE(InstanceContentController::localPath("/tmp/plain.jar"),
				 QStringLiteral("/tmp/plain.jar"));
	}

  private:
	bool resolved() const
	{
		QStringList seen;
		for (size_t row = 0; row < m_folder->size(); ++row) {
			seen << m_folder->at(row).name();
		}
		return seen.contains("Sodium") && seen.contains("Lithium") &&
			   seen.contains("Fabric API");
	}

	QTemporaryDir m_dir;
	std::shared_ptr<ModFolderModel> m_folder;
	std::unique_ptr<ContentFilterModel> m_model;
};

QTEST_GUILESS_MAIN(ContentFilterModelTest)

#include "ContentFilterModel_test.moc"
