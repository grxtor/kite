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

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "BaseVersionList.h"
#include "models/NewInstanceController.h"
#include "tasks/Task.h"

namespace
{
	/* Stand-in for Meta::VersionList: exposes the same role NAMES (version-
	 * Id, type, parentGameVersion, sort) but deliberately different role
	 * NUMBERS. A test that passes against this only passes because the
	 * proxies resolve roles by name, not because they happen to reuse
	 * Meta::VersionList's own numbers - same reasoning as
	 * InstanceFilterModel_test.cpp's FakeInstanceModel. */
	class FakeVersionList : public BaseVersionList
	{
	  public:
		struct Row {
			QString versionId;
			QString type;
			QString parentGameVersion;
			qint64 sort = 0;
		};

		enum Roles {
			VersionIdRole = Qt::UserRole + 40,
			TypeRole,
			ParentVersionRole,
			SortRole
		};

		explicit FakeVersionList(QList<Row> rows, QObject* parent = nullptr)
			: BaseVersionList(parent), m_rows(std::move(rows))
		{
		}

		void setLoaded(bool loaded)
		{
			m_isLoaded = loaded;
		}
		void setLoadTask(Task::Ptr task)
		{
			m_loadTask = task;
		}

		// BaseVersionList
		Task::Ptr getLoadTask() override
		{
			return m_loadTask;
		}
		bool isLoaded() override
		{
			return m_isLoaded;
		}
		const BaseVersionPtr at(int) const override
		{
			return BaseVersionPtr();
		}
		int count() const override
		{
			return m_rows.count();
		}
		void sortVersions() override {}

		// QAbstractListModel
		QVariant data(const QModelIndex& index, int role) const override
		{
			if (!index.isValid() || index.row() < 0 ||
				index.row() >= m_rows.count()) {
				return QVariant();
			}
			const Row& row = m_rows.at(index.row());
			switch (role) {
				case VersionIdRole:
					return row.versionId;
				case TypeRole:
					return row.type;
				case ParentVersionRole:
					return row.parentGameVersion;
				case SortRole:
					return row.sort;
				default:
					return QVariant();
			}
		}

		QHash<int, QByteArray> roleNames() const override
		{
			return {
				{ VersionIdRole, "versionId" },
				{ TypeRole, "type" },
				{ ParentVersionRole, "parentGameVersion" },
				{ SortRole, "sort" },
			};
		}

	  protected:
		// Overrides a slot, but adds none of its own - no Q_OBJECT needed
		// here, same as FakeInstanceModel in InstanceFilterModel_test.cpp.
		void updateListData(QList<BaseVersionPtr>) override {}

	  private:
		QList<Row> m_rows;
		bool m_isLoaded = true;
		Task::Ptr m_loadTask;
	};

	/* A Task that does nothing on its own - executeTask() is a no-op - so
	 * the test can drive success/failure by hand, same idiom as
	 * tasks/TaskWatcher_test.cpp's ScriptedTask. */
	class ScriptedTask : public Task
	{
		Q_OBJECT
	  public:
		using Task::Task;

		void driveSuccess()
		{
			emitSucceeded();
		}
		void driveFailure(const QString& reason)
		{
			emitFailed(reason);
		}

	  protected:
		void executeTask() override {}
	};
} // namespace

class NewInstanceControllerTest : public QObject
{
	Q_OBJECT

  private slots:

	/// Release-only, newest first, is the default - same starting point as
	/// VanillaPage's checkboxes (ui/pages/modplatform/VanillaPage.cpp).
	void test_minecraftVersions_defaultsToReleaseOnly_newestFirst()
	{
		FakeVersionList list({
			{ "1.19", "release", "", 50 },
			{ "1.20", "release", "", 100 },
			{ "1.20.1-rc1", "snapshot", "", 150 },
			{ "b1.7.3", "old_beta", "", 10 },
		});

		MinecraftVersionListProxy proxy;
		proxy.setSourceModel(&list);

		QCOMPARE(proxy.count(), 2);
		QCOMPARE(proxy.index(0, 0).data(FakeVersionList::VersionIdRole).toString(),
				 QString("1.20"));
		QCOMPARE(proxy.index(1, 0).data(FakeVersionList::VersionIdRole).toString(),
				 QString("1.19"));
		QCOMPARE(proxy.firstVersionId(), QString("1.20"));
	}

	/// showSnapshots/showOldVersions widen the filter without disturbing the
	/// newest-first order.
	void test_minecraftVersions_showSnapshotsAndShowOldVersions_widenFilter()
	{
		FakeVersionList list({
			{ "1.19", "release", "", 50 },
			{ "1.20", "release", "", 100 },
			{ "1.20.1-rc1", "snapshot", "", 150 },
			{ "b1.7.3", "old_beta", "", 10 },
		});

		MinecraftVersionListProxy proxy;
		proxy.setSourceModel(&list);

		QSignalSpy countSpy(&proxy, &VersionListLoadingProxy::countChanged);
		proxy.setShowSnapshots(true);
		QCOMPARE(proxy.count(), 3);
		QCOMPARE(proxy.firstVersionId(), QString("1.20.1-rc1"));
		QVERIFY(!countSpy.isEmpty());

		proxy.setShowOldVersions(true);
		QCOMPARE(proxy.count(), 4);
	}

	/// A type neither toggle names (an "experiment" build, say) always stays
	/// hidden - see the class comment in NewInstanceController.h.
	void test_minecraftVersions_hidesUnrecognizedType()
	{
		FakeVersionList list({
			{ "1.20", "release", "", 100 },
			{ "20w14infinite", "experiment", "", 200 },
		});

		MinecraftVersionListProxy proxy;
		proxy.setSourceModel(&list);
		proxy.setShowSnapshots(true);
		proxy.setShowOldVersions(true);

		QCOMPARE(proxy.count(), 1);
		QCOMPARE(proxy.firstVersionId(), QString("1.20"));
	}

	/// loading() / error() mirror the load task exactly like TaskWatcher
	/// mirrors the tasks it watches (tasks/TaskWatcher.h).
	void test_minecraftVersions_loading_tracksLoadTaskLifecycle()
	{
		FakeVersionList list({});
		list.setLoaded(false);
		auto* scripted = new ScriptedTask();
		list.setLoadTask(Task::Ptr(scripted));

		MinecraftVersionListProxy proxy;
		QSignalSpy loadingSpy(&proxy, &VersionListLoadingProxy::loadingChanged);
		proxy.setSourceModel(&list);

		QVERIFY(proxy.loading());
		QVERIFY(scripted->isRunning());
		QCOMPARE(loadingSpy.count(), 1);

		scripted->driveFailure("could not reach the metadata server");
		QVERIFY(!proxy.loading());
		QCOMPARE(proxy.error(),
				 QString("could not reach the metadata server"));
	}

	/// "Exact if present": a build naming no Minecraft version (Fabric,
	/// Quilt) is always kept; one that does must match exactly - the same
	/// rule LoaderVersionPage applies (ui/dialogs/InstallLoaderDialog.cpp).
	void test_loaderVersions_exactIfPresentFilter()
	{
		FakeVersionList list({
			{ "47.1.0", "release", "1.20.1", 90 },
			{ "47.2.0", "release", "1.20.1", 100 },
			{ "48.0.0", "release", "1.21", 110 },
			{ "0.15.0", "release", "", 120 },
		});

		LoaderVersionListProxy proxy;
		proxy.setSourceModel(&list);
		QCOMPARE(proxy.count(), 4);

		proxy.setMinecraftVersion("1.20.1");
		QCOMPARE(proxy.count(), 3);
		// Newest first among what is left: the parent-less build (120)
		// still outranks both matching Forge builds.
		QCOMPARE(proxy.firstVersionId(), QString("0.15.0"));

		proxy.setMinecraftVersion("1.21");
		QCOMPARE(proxy.count(), 2);
		QCOMPARE(proxy.firstVersionId(), QString("0.15.0"));
	}

	void test_composeSuggestedInstanceName()
	{
		QCOMPARE(composeSuggestedInstanceName(QString(), QString()), QString());
		QCOMPARE(composeSuggestedInstanceName("1.21.1", QString()),
				 QString("1.21.1"));
		QCOMPARE(composeSuggestedInstanceName("1.21.1", "fabric"),
				 QString("1.21.1 Fabric"));
		QCOMPARE(composeSuggestedInstanceName("1.21.1", "quilt"),
				 QString("1.21.1 Quilt"));
		QCOMPARE(composeSuggestedInstanceName("1.21.1", "forge"),
				 QString("1.21.1 Forge"));
		QCOMPARE(composeSuggestedInstanceName("1.21.1", "neoforge"),
				 QString("1.21.1 NeoForge"));
		// An unrecognised loader name is treated like "none" rather than
		// crashing or guessing.
		QCOMPARE(composeSuggestedInstanceName("1.21.1", "not-a-loader"),
				 QString("1.21.1"));
	}

	void test_suggestedImportName()
	{
		QCOMPARE(suggestedImportName(""), QString());
		QCOMPARE(suggestedImportName("   "), QString());

		// A bare local path - the common case for a FileDialog pick or a
		// drag-drop.
		QCOMPARE(suggestedImportName("/home/user/Downloads/Cool Modpack.mrpack"),
				 QString("Cool Modpack"));
		// A "file://" URL, percent-encoded, is just as local.
		QCOMPARE(
			suggestedImportName("file:///home/user/Downloads/Cool%20Pack.zip"),
			QString("Cool Pack"));

		// A pasted direct-download link.
		QCOMPARE(suggestedImportName(
					 "https://cdn.modrinth.com/data/AAAA/versions/1.0/"
					 "My-Pack-1.0.mrpack"),
				 QString("My-Pack-1.0"));

		// CurseForge's own "download" button links end this way; the real
		// file name sits behind the same rewrite ImportPage::updateState()
		// applies before reading it - see suggestedImportName()'s comment.
		QCOMPARE(suggestedImportName("https://www.curseforge.com/api/v1/mods/1/"
									 "files/2/download?client=y"),
				 QString("file"));
	}

	/// The gate the QML Import button uses before it lets InstanceImportTask
	/// even try - see importSourceLooksValid()'s comment.
	void test_importSourceLooksValid()
	{
		QVERIFY(!importSourceLooksValid(""));
		QVERIFY(!importSourceLooksValid("   "));

		// A remote link can't be checked locally - only InstanceImportTask
		// can tell whether it actually resolves to something importable.
		QVERIFY(importSourceLooksValid(
			"https://cdn.modrinth.com/data/AAAA/versions/1.0/"
			"My-Pack-1.0.mrpack"));

		// A local path that does not exist is always rejected, archive
		// extension or not.
		QVERIFY(!importSourceLooksValid("/no/such/path/Cool Modpack.mrpack"));

		QTemporaryDir dir;
		QVERIFY(dir.isValid());

		// Exists, but not an archive extension.
		const QString textPath = dir.filePath("notes.txt");
		QFile textFile(textPath);
		QVERIFY(textFile.open(QIODevice::WriteOnly));
		textFile.close();
		QVERIFY(!importSourceLooksValid(textPath));

		// Exists and looks like a modpack archive.
		const QString packPath = dir.filePath("Cool Modpack.mrpack");
		QFile packFile(packPath);
		QVERIFY(packFile.open(QIODevice::WriteOnly));
		packFile.close();
		QVERIFY(importSourceLooksValid(packPath));
	}
};

QTEST_GUILESS_MAIN(NewInstanceControllerTest)

#include "NewInstanceController_test.moc"
