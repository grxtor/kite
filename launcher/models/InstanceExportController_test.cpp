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

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "FileSystem.h"
#include "MMCZip.h"
#include "NullInstance.h"
#include "models/InstanceExportController.h"
#include "settings/INISettingsObject.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* The global settings BaseInstance's constructor overrides or passes
	 * through - the same set InstanceList_test.cpp registers. */
	SettingsObjectPtr makeGlobalSettings(const QString& dir)
	{
		auto settings = std::make_shared<INISettingsObject>(
			FS::PathCombine(dir, "global.ini"));
		settings->registerSetting("PreLaunchCommand", "");
		settings->registerSetting("WrapperCommand", "");
		settings->registerSetting("PostExitCommand", "");
		settings->registerSetting("ShowConsole", true);
		settings->registerSetting("AutoCloseConsole", false);
		settings->registerSetting("ShowConsoleOnError", true);
		settings->registerSetting("LogPrePostOutput", true);
		settings->registerSetting("ConsoleMaxLines", 100000);
		settings->registerSetting("ConsoleOverflowStop", true);
		return settings;
	}

	bool writeFile(const QString& path, const QByteArray& data = "data")
	{
		if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
			return false;
		}
		QFile file(path);
		return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
	}

	/* Until @p watcher's task is over, one way or the other. */
	void waitFor(TaskWatcher* watcher)
	{
		if (watcher->succeeded() || watcher->failed()) {
			return;
		}
		QSignalSpy finished(watcher, &TaskWatcher::finished);
		QVERIFY(finished.wait(10000));
	}

	QModelIndex childNamed(QAbstractItemModel* model, const QModelIndex& parent,
						   const QString& name)
	{
		for (int row = 0; row < model->rowCount(parent); ++row) {
			const QModelIndex index = model->index(row, 0, parent);
			if (index.data(Qt::DisplayRole).toString() == name) {
				return index;
			}
		}
		return QModelIndex();
	}

	int roleNamed(QAbstractItemModel* model, const QByteArray& name)
	{
		return model->roleNames().key(name, -1);
	}
} // namespace

/*
 * Drives the controller against a NullInstance over a temporary folder: a
 * plain zip export end to end (the ignore lists, `.packignore`, unchecking
 * a folder through the tree model the QML TreeView uses), and the ways an
 * export can fail to start or fail once started. The pack formats need a
 * MinecraftInstance and the network, and are not exercised here beyond
 * being refused for an instance that is not one.
 */
class InstanceExportControllerTest : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	InstancePtr makeInstance(const QString& name)
	{
		const QString root = m_dir.filePath(name);
		QDir().mkpath(root);
		auto settings = std::make_shared<INISettingsObject>(
			FS::PathCombine(root, "instance.cfg"));
		auto instance = std::make_shared<NullInstance>(
			makeGlobalSettings(m_dir.path()), settings, root);
		instance->setName(name);
		return instance;
	}

  private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
	}

	void zipLeavesOutDebrisAndBlockedPaths()
	{
		auto instance = makeInstance("zipped");
		const QString root = instance->instanceRoot();
		QVERIFY(writeFile(FS::PathCombine(root, "a.txt")));
		QVERIFY(writeFile(FS::PathCombine(root, "sub/b.txt")));
		QVERIFY(writeFile(FS::PathCombine(root, "skip/c.txt")));
		QVERIFY(writeFile(FS::PathCombine(root, "logs/latest.log")));
		QVERIFY(writeFile(FS::PathCombine(root, "crash-reports/crash.txt")));
		QVERIFY(writeFile(FS::PathCombine(root, ".DS_Store")));
		QVERIFY(writeFile(FS::PathCombine(root, ".backups/old.zip")));
		QVERIFY(writeFile(FS::PathCombine(root, ".packignore"), "skip\n"));

		InstanceExportController controller(instance);
		QCOMPARE(controller.instanceName(), QStringLiteral("zipped"));
		QCOMPARE(controller.format(), QStringLiteral("zip"));
		QVERIFY(controller.ready());
		QCOMPARE(controller.fileExtension(), QStringLiteral(".zip"));
		QVERIFY(controller.suggestedFileUrl().endsWith("/zipped.zip"));

		// No extension given: the format's own is added.
		const QString output = m_dir.filePath("zipped-export");
		auto* watcher = qobject_cast<TaskWatcher*>(
			controller.exportTo(QUrl::fromLocalFile(output).toString()));
		QVERIFY(watcher);
		QVERIFY(controller.error().isEmpty());
		waitFor(watcher);
		QVERIFY2(watcher->succeeded(), qPrintable(watcher->error()));
		QCOMPARE(controller.lastOutput(), output + ".zip");

		const QStringList entries = MMCZip::listEntries(output + ".zip");
		QVERIFY2(entries.contains("a.txt"), qPrintable(entries.join(", ")));
		QVERIFY(entries.contains("sub/b.txt"));
		QVERIFY(!entries.contains("skip/c.txt"));
		QVERIFY(!entries.contains("logs/latest.log"));
		QVERIFY(!entries.contains("crash-reports/crash.txt"));
		QVERIFY(!entries.contains(".DS_Store"));
		// The instance's own backups are zips; they do not go in a zip.
		QVERIFY(!entries.contains(".backups/old.zip"));
	}

	void uncheckingInTheTreeLeavesItOut()
	{
		auto instance = makeInstance("tree");
		const QString root = instance->instanceRoot();
		QVERIFY(writeFile(FS::PathCombine(root, "keep.txt")));
		QVERIFY(writeFile(FS::PathCombine(root, "sub/b.txt")));
		QVERIFY(writeFile(FS::PathCombine(root, "logs/latest.log")));
		QVERIFY(writeFile(FS::PathCombine(root, ".backups/old.zip")));

		InstanceExportController controller(instance);
		auto* model = qobject_cast<QAbstractItemModel*>(controller.files());
		QVERIFY(model);
		const int checkRole = roleNamed(model, "checkState");
		const int dirRole = roleNamed(model, "isDir");
		QCOMPARE(checkRole, int(Qt::CheckStateRole));
		QVERIFY(dirRole >= 0);
		QCOMPARE(model->columnCount(controller.filesRoot()), 1);

		// The file system model fills in on its own thread.
		QTRY_VERIFY_WITH_TIMEOUT(
			childNamed(model, controller.filesRoot(), "sub").isValid(), 10000);
		QVERIFY(childNamed(model, controller.filesRoot(), "keep.txt").isValid());
		// Debris is hidden, not offered.
		QVERIFY(!childNamed(model, controller.filesRoot(), "logs").isValid());
		QVERIFY(!childNamed(model, controller.filesRoot(), ".backups").isValid());

		const QModelIndex sub = childNamed(model, controller.filesRoot(), "sub");
		QVERIFY(sub.data(dirRole).toBool());
		QCOMPARE(sub.data(checkRole).toInt(), int(Qt::Checked));
		QVERIFY(model->setData(sub, int(Qt::Unchecked), checkRole));
		QCOMPARE(sub.data(checkRole).toInt(), int(Qt::Unchecked));

		auto* watcher = qobject_cast<TaskWatcher*>(
			controller.exportTo(m_dir.filePath("tree-export.zip")));
		QVERIFY(watcher);
		waitFor(watcher);
		QVERIFY2(watcher->succeeded(), qPrintable(watcher->error()));

		const QStringList entries =
			MMCZip::listEntries(m_dir.filePath("tree-export.zip"));
		QVERIFY(entries.contains("keep.txt"));
		QVERIFY(!entries.contains("sub/b.txt"));

		// Remembered for the next export of this instance.
		QFile ignore(FS::PathCombine(root, ".packignore"));
		QVERIFY(ignore.open(QIODevice::ReadOnly));
		QCOMPARE(QString::fromUtf8(ignore.readAll()).trimmed(),
				 QStringLiteral("sub"));
	}

	void failuresAreReported()
	{
		auto instance = makeInstance("broken");
		QVERIFY(writeFile(FS::PathCombine(instance->instanceRoot(), "a.txt")));
		InstanceExportController controller(instance);

		// Nowhere to write to: refused before anything starts.
		QVERIFY(!controller.exportTo(QString()));
		QVERIFY(!controller.error().isEmpty());

		// A folder that does not exist: the task starts and fails.
		auto* watcher = qobject_cast<TaskWatcher*>(controller.exportTo(
			m_dir.filePath("no/such/folder/out.zip")));
		QVERIFY(watcher);
		QVERIFY(controller.error().isEmpty());
		waitFor(watcher);
		QVERIFY(watcher->failed());
		QVERIFY(!watcher->error().isEmpty());
		QVERIFY(controller.lastOutput().isEmpty());
	}

	void packFormatsNeedMinecraft()
	{
		InstanceExportController controller(makeInstance("plain"));
		QVERIFY(!controller.packFormats());
		QSignalSpy changed(&controller,
						   &InstanceExportController::choicesChanged);
		controller.setFormat("modrinth");
		controller.setFormat("no-such-format");
		QCOMPARE(controller.format(), QStringLiteral("zip"));
		QCOMPARE(changed.count(), 0);
	}
};

QTEST_MAIN(InstanceExportControllerTest)

#include "InstanceExportController_test.moc"
