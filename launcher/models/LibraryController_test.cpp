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

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "InstanceList.h"
#include "models/LibraryController.h"
#include "settings/INISettingsObject.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* The global settings every BaseInstance constructor overrides or
	 * passes through - the same set InstanceList_test.cpp registers. */
	SettingsObjectPtr makeGlobalSettings(const QString& dir)
	{
		auto settings = std::make_shared<INISettingsObject>(
			QDir(dir).filePath("global.ini"));
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

	/* An instance folder InstanceList loads as a NullInstance: an
	 * InstanceType it does not know. */
	bool makeInstance(const QString& instancesDir, const QString& id,
					  const QString& name)
	{
		const QString root = QDir(instancesDir).filePath(id);
		if (!QDir().mkpath(root)) {
			return false;
		}
		QFile config(QDir(root).filePath("instance.cfg"));
		if (!config.open(QIODevice::WriteOnly)) {
			return false;
		}
		return config.write(QStringLiteral("InstanceType=NullTest\nname=%1\n")
								.arg(name)
								.toUtf8()) > 0;
	}

	QJsonObject readGroupFile(const QString& dir)
	{
		QFile file(QDir(dir).filePath("instgroups.json"));
		if (!file.open(QIODevice::ReadOnly)) {
			return {};
		}
		return QJsonDocument::fromJson(file.readAll())
			.object()
			.value("groups")
			.toObject();
	}
} // namespace

/*
 * LibraryController over a real InstanceList of NullInstances in a
 * temporary folder. The group file (instgroups.json) is written to the
 * current directory, so each test runs from its own temporary one.
 * Undoing a delete is only exercised for the "nothing to undo" case:
 * trashing for real would put test folders in the user's trash.
 */
class LibraryControllerTest : public QObject
{
	Q_OBJECT

	QString m_originalDir;
	std::unique_ptr<QTemporaryDir> m_dataDir;
	std::shared_ptr<InstanceList> m_list;
	std::unique_ptr<LibraryController> m_controller;

	QString instancesDir() const
	{
		return m_dataDir->filePath("instances");
	}

  private slots:
	void initTestCase()
	{
		m_originalDir = QDir::currentPath();
	}

	void init()
	{
		m_dataDir = std::make_unique<QTemporaryDir>();
		QVERIFY(m_dataDir->isValid());
		QVERIFY(QDir::setCurrent(m_dataDir->path()));
		QVERIFY(makeInstance(instancesDir(), "alpha", "Alpha"));
		QVERIFY(makeInstance(instancesDir(), "beta", "Beta"));
		QVERIFY(makeInstance(instancesDir(), "gamma", "Gamma"));

		m_list = std::make_shared<InstanceList>(
			makeGlobalSettings(m_dataDir->path()), QStringList{instancesDir()});
		QCOMPARE(m_list->loadList(), InstanceList::NoError);
		m_controller = std::make_unique<LibraryController>(m_list, nullptr);
	}

	void cleanup()
	{
		m_controller.reset();
		m_list.reset();
		QDir::setCurrent(m_originalDir);
		m_dataDir.reset();
	}

	void renameMovesEveryMemberAndKeepsTheFold()
	{
		m_controller->moveToGroup("alpha", "Modded");
		m_controller->moveToGroup("beta", "  Modded ");
		m_controller->moveToGroup("gamma", "Vanilla");
		QCOMPARE(m_list->getInstanceGroup("beta"), QStringLiteral("Modded"));

		QSignalSpy folds(m_controller.get(),
						 &LibraryController::collapsedGroupsChanged);
		m_controller->setGroupCollapsed("Modded", true);
		QCOMPARE(folds.count(), 1);
		QCOMPARE(m_controller->collapsedGroups(), QStringList{"Modded"});
		QVERIFY(readGroupFile(m_dataDir->path())
					.value("Modded")
					.toObject()
					.value("hidden")
					.toBool());

		QSignalSpy changed(m_list.get(), &QAbstractItemModel::dataChanged);
		m_controller->renameGroup("Modded", " Fabric ");
		QCOMPARE(m_list->getInstanceGroup("alpha"), QStringLiteral("Fabric"));
		QCOMPARE(m_list->getInstanceGroup("beta"), QStringLiteral("Fabric"));
		QCOMPARE(m_list->getInstanceGroup("gamma"), QStringLiteral("Vanilla"));
		QCOMPARE(changed.count(), 2);
		QVERIFY(!m_list->getGroups().contains("Modded"));
		QCOMPARE(m_controller->collapsedGroups(), QStringList{"Fabric"});

		const QJsonObject groups = readGroupFile(m_dataDir->path());
		QVERIFY(!groups.contains("Modded"));
		QVERIFY(groups.value("Fabric").toObject().value("hidden").toBool());

		// Renaming to nothing, or to the same name, changes nothing.
		m_controller->renameGroup("Fabric", "  ");
		m_controller->renameGroup("Fabric", "Fabric");
		QCOMPARE(m_list->getInstanceGroup("alpha"), QStringLiteral("Fabric"));
	}

	void deleteUngroupsEveryMember()
	{
		// "alpha" is row 0: the row that used to get no change signal.
		m_controller->moveToGroup("alpha", "Old");
		m_controller->moveToGroup("beta", "Old");
		m_controller->setGroupCollapsed("Old", true);

		QSignalSpy changed(m_list.get(), &QAbstractItemModel::dataChanged);
		m_controller->deleteGroup("Old");
		QCOMPARE(changed.count(), 2);
		QVERIFY(m_list->getInstanceGroup("alpha").isEmpty());
		QVERIFY(m_list->getInstanceGroup("beta").isEmpty());
		QVERIFY(!m_list->getGroups().contains("Old"));
		QVERIFY(m_controller->collapsedGroups().isEmpty());
		QVERIFY(m_list->getInstanceById("alpha"));
	}

	void ungroupedFoldsForTheSession()
	{
		m_controller->setGroupCollapsed(QString(), true);
		QCOMPARE(m_controller->collapsedGroups(), QStringList{QString()});
		m_controller->setGroupCollapsed(QString(), false);
		QVERIFY(m_controller->collapsedGroups().isEmpty());
	}

	void describeFoldersAndNothingToUndo()
	{
		const QVariantMap folders = m_controller->folders("alpha");
		QCOMPARE(QDir(folders.value("instance").toString()).dirName(),
				 QStringLiteral("alpha"));
		QVERIFY(!folders.value("game").toString().isEmpty());
		// Not Minecraft: no mods folder to offer.
		QVERIFY(folders.value("mods").toString().isEmpty());
		QVERIFY(m_controller->folders("nope").isEmpty());

		m_controller->moveToGroup("alpha", "Modded");
		const QVariantMap alpha = m_controller->describe("alpha");
		QCOMPARE(alpha.value("name").toString(), QStringLiteral("Alpha"));
		QCOMPARE(alpha.value("group").toString(), QStringLiteral("Modded"));
		QVERIFY(alpha.contains("iconKey"));
		QVERIFY(m_controller->describe("nope").isEmpty());

		QVERIFY(!m_controller->canUndoDelete());
		QVERIFY(m_controller->lastDeletedName().isEmpty());
		QVERIFY(!m_controller->undoDelete());
	}

	void duplicateCopiesUnderTheNewName()
	{
		QVERIFY(!m_controller->duplicate("nope", "Copy", QString(), true, true,
										 QString()));
		QVERIFY(!m_controller->duplicate("alpha", "  \n ", QString(), true,
										 true, QString()));

		auto* watcher = qobject_cast<TaskWatcher*>(m_controller->duplicate(
			"alpha", "Alpha copy", "Copies", false, false, "grass"));
		QVERIFY(watcher);
		if (!watcher->succeeded() && !watcher->failed()) {
			QSignalSpy finished(watcher, &TaskWatcher::finished);
			QVERIFY(finished.wait(10000));
		}
		QVERIFY2(watcher->succeeded(), qPrintable(watcher->error()));

		InstancePtr copy;
		for (int row = 0; row < m_list->count(); ++row) {
			if (m_list->at(row)->name() == QStringLiteral("Alpha copy")) {
				copy = m_list->at(row);
			}
		}
		QVERIFY(copy);
		QCOMPARE(copy->iconKey(), QStringLiteral("grass"));
		QCOMPARE(m_list->getInstanceGroup(copy->id()), QStringLiteral("Copies"));
	}
};

QTEST_GUILESS_MAIN(LibraryControllerTest)

#include "LibraryController_test.moc"
