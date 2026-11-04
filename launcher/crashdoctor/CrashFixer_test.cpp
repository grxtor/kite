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

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "crashdoctor/CrashFixer.h"
#include "settings/INISettingsObject.h"

using namespace CrashDoctor;

/*
 * The crash doctor's fixes on a throwaway instance folder: what each changes,
 * that nothing is deleted, and that Undo puts every file and setting back.
 */
namespace
{
	const QStringList kSettings{"JavaMajorAtLeast", "JavaRepick", "OverrideJavaLocation",
								"JavaAutoDownload", "JavaAutoDownloadVendor",
								"OverrideJavaAutoDownloadVendor", "MaxMemAlloc", "MinMemAlloc",
								"OverrideMemory", "JvmArgs", "OverrideJavaArgs",
								"MacStartOnFirstThread", "KiteMod", "RepairGameFiles",
								"CrashDoctorUndo"};

	struct Fixture {
		QTemporaryDir root;
		std::shared_ptr<INISettingsObject> settings =
			std::make_shared<INISettingsObject>(root.filePath("instance.cfg"));
		QString gameRoot = root.filePath("minecraft");
		QString modsDir = root.filePath("minecraft/mods");
		Report report;

		Fixture()
		{
			for (const QString& id : kSettings) {
				settings->registerSetting(id, QVariant());
			}
			settings->set("MaxMemAlloc", 4096);
			settings->set("MinMemAlloc", 512);
			settings->set("KiteMod", true);
			QDir().mkpath(modsDir);
			QDir().mkpath(QDir(gameRoot).filePath("config"));
			report.gameRoot = gameRoot;
			report.modsDir = modsDir;
		}

		Target target()
		{
			return Target{*settings, gameRoot, modsDir, nullptr};
		}

		QString write(const QString& relative, const QByteArray& content)
		{
			const QString path = QDir(gameRoot).filePath(relative);
			QDir().mkpath(QFileInfo(path).absolutePath());
			QFile file(path);
			file.open(QIODevice::WriteOnly);
			file.write(content);
			return path;
		}

		QString addMod(const QString& id, const QString& file, const QString& version)
		{
			const QString path = write("mods/" + file, "jar");
			report.modFiles[id].append({path, version});
			return path;
		}
	};

	ResolvedFix fix(const QString& action, const QVariantMap& params = {})
	{
		return ResolvedFix{action, params, action};
	}

	QByteArray read(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}
} // namespace

class CrashFixerTest : public QObject
{
	Q_OBJECT

  private slots:
	void setMemoryAndUndo()
	{
		Fixture f;
		const auto undo = applyFix(f.target(), fix("setMemory", {{"maxMb", 6144}}), f.report, nullptr);
		QVERIFY(undo.has_value());
		QCOMPARE(f.settings->get("MaxMemAlloc").toInt(), 6144);
		QCOMPARE(f.settings->get("MinMemAlloc").toInt(), 512);
		QVERIFY(f.settings->get("OverrideMemory").toBool());
		QVERIFY(!f.settings->get("CrashDoctorUndo").toString().isEmpty());
		QVERIFY(undoFix(f.target(), *undo, nullptr));
		QCOMPARE(f.settings->get("MaxMemAlloc").toInt(), 4096);
		QVERIFY(!f.settings->get("OverrideMemory").toBool());
		QVERIFY(f.settings->get("CrashDoctorUndo").toString().isEmpty());
	}

	void memoryIsClampedToTheComputer()
	{
		Fixture f;
		QVERIFY(applyFix(f.target(), fix("setMemory", {{"maxMb", 1}}), f.report, nullptr));
		QCOMPARE(f.settings->get("MaxMemAlloc").toInt(), kMinMemoryMb);
	}

	void javaChoiceIsForTheNextLaunch()
	{
		Fixture f;
		f.settings->set("OverrideJavaLocation", true);
		QVERIFY(applyFix(f.target(), fix("useJavaForInstance", {{"major", 21}}), f.report, nullptr));
		QCOMPARE(f.settings->get("JavaMajorAtLeast").toInt(), 21);
		QVERIFY(f.settings->get("JavaRepick").toBool());
		const auto undo = applyFix(f.target(), fix("useRecommendedJava"), f.report, nullptr);
		QVERIFY(undo.has_value());
		QCOMPARE(f.settings->get("JavaMajorAtLeast").toInt(), 0);
		QVERIFY(!f.settings->get("OverrideJavaLocation").toBool());
		QVERIFY(undoFix(f.target(), *undo, nullptr));
		QCOMPARE(f.settings->get("JavaMajorAtLeast").toInt(), 21);
		QVERIFY(f.settings->get("OverrideJavaLocation").toBool());
	}

	void disableModRenamesAndUndoRestores()
	{
		Fixture f;
		const QString jar = f.addMod("betterf3", "BetterF3-11.0.jar", "11.0");
		const ResolvedFix turnOff = fix("disableMod", {{"target", "betterf3"}});
		QCOMPARE(filesTouched(turnOff, f.report), QStringList({"BetterF3-11.0.jar"}));
		const auto undo = applyFix(f.target(), turnOff, f.report, nullptr);
		QVERIFY(undo.has_value());
		QVERIFY(!QFileInfo::exists(jar));
		QVERIFY(QFileInfo::exists(jar + ".disabled"));
		QVERIFY(undoFix(f.target(), *undo, nullptr));
		QVERIFY(QFileInfo::exists(jar));
	}

	void disableSeveralAtOnce()
	{
		Fixture f;
		const QString a = f.addMod("a", "a.jar", "1");
		const QString b = f.addMod("b", "b.jar", "1");
		QVERIFY(applyFix(f.target(), fix("disableMod", {{"target", QStringList{"a", "b.jar"}}}),
						 f.report, nullptr));
		QVERIFY(QFileInfo::exists(a + ".disabled"));
		QVERIFY(QFileInfo::exists(b + ".disabled"));
	}

	void missingModIsAnError()
	{
		Fixture f;
		QString error;
		QVERIFY(!applyFix(f.target(), fix("disableMod", {{"target", "gone"}}), f.report, &error));
		QVERIFY(!error.isEmpty());
	}

	void duplicateKeepsTheNewest()
	{
		Fixture f;
		const QString older = f.addMod("jei", "jei-15.2.0.27.jar", "15.2.0.27");
		const QString newer = f.addMod("jei", "jei-15.3.0.4.jar", "15.3.0.4");
		const ResolvedFix dedupe = fix("removeDuplicateMod", {{"mod", "jei"}});
		QCOMPARE(filesTouched(dedupe, f.report), QStringList({"jei-15.2.0.27.jar"}));
		QVERIFY(applyFix(f.target(), dedupe, f.report, nullptr));
		QVERIFY(QFileInfo::exists(newer));
		QVERIFY(QFileInfo::exists(older + ".disabled"));
	}

	void configResetKeepsABackup()
	{
		Fixture f;
		const QString config = f.write("config/create-client.toml", "");
		const auto undo =
			applyFix(f.target(), fix("resetConfigFile", {{"file", "create-client.toml"}}), f.report, nullptr);
		QVERIFY(undo.has_value());
		QVERIFY(!QFileInfo::exists(config));
		QCOMPARE(undo->renames.size(), 1);
		QVERIFY(undo->renames.first().first.startsWith(config + ".kite-backup-"));
		QVERIFY(undoFix(f.target(), *undo, nullptr));
		QVERIFY(QFileInfo::exists(config));
	}

	void configResetStaysInsideTheInstance()
	{
		Fixture f;
		const QString outside = QDir(f.root.path()).filePath("secret.toml");
		QFile file(outside);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.close();
		QString error;
		QVERIFY(!applyFix(f.target(), fix("resetConfigFile", {{"file", "../../secret.toml"}}),
						  f.report, &error));
		QVERIFY(QFileInfo::exists(outside));
	}

	void shadersOffKeepsOtherLines()
	{
		Fixture f;
		const QString iris = f.write("config/iris.properties",
									 "shaderPack=Complementary.zip\nenableShaders=true\n");
		const auto undo = applyFix(f.target(), fix("turnOffShaders"), f.report, nullptr);
		QVERIFY(undo.has_value());
		QCOMPARE(read(iris), QByteArray("shaderPack=Complementary.zip\nenableShaders=false\n"));
		QVERIFY(undoFix(f.target(), *undo, nullptr));
		QCOMPARE(read(iris), QByteArray("shaderPack=Complementary.zip\nenableShaders=true\n"));
	}

	void shadersOffWithoutAFileUndoesToNoFile()
	{
		Fixture f;
		const QString iris = QDir(f.gameRoot).filePath("config/iris.properties");
		const auto undo = applyFix(f.target(), fix("turnOffShaders"), f.report, nullptr);
		QVERIFY(undo.has_value());
		QCOMPARE(read(iris), QByteArray("enableShaders=false\n"));
		QVERIFY(undoFix(f.target(), *undo, nullptr));
		QVERIFY(!QFileInfo::exists(iris));
	}

	void flowsAndUnknownActionsAreNotFixerWork()
	{
		Fixture f;
		QString error;
		QVERIFY(!applyFix(f.target(), fix("installMissingDependency", {{"mod", "curios"}}), f.report, &error));
		QVERIFY(!applyFix(f.target(), fix("runCommand"), f.report, &error));
	}

	void undoRecordSurvivesARestart()
	{
		Undo undo;
		undo.action = "turnOffShaders";
		undo.settings = {{"KiteMod", true}};
		undo.renames = {{"/a.jar.disabled", "/a.jar"}};
		undo.contents = {{"/x", QByteArray("old\n")}, {"/y", QByteArray()}};
		undo.graphicsBackend = "vulkan";
		const auto back = Undo::fromJson(undo.toJson());
		QVERIFY(back.has_value());
		QCOMPARE(back->action, undo.action);
		QCOMPARE(back->settings, undo.settings);
		QCOMPARE(back->renames, undo.renames);
		QCOMPARE(back->contents.value("/x"), QByteArray("old\n"));
		QVERIFY(back->contents.value("/y").isNull());
		QCOMPARE(back->graphicsBackend, undo.graphicsBackend);
		QVERIFY(!Undo::fromJson("{}").has_value());
	}
};

QTEST_GUILESS_MAIN(CrashFixerTest)

#include "CrashFixer_test.moc"
