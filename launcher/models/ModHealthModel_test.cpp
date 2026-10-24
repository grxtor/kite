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

#include <QSignalSpy>
#include <QTest>

#include "minecraft/mod/Mod.h"
#include "models/ModHealthModel.h"

using ModHealth::Issue;
using ModHealth::Kind;

namespace
{
	Issue issue(Kind kind, const QString& key, const QString& name,
				bool blocking = true)
	{
		Issue i;
		i.kind = kind;
		i.blocking = blocking;
		i.key = key;
		i.name = name;
		return i;
	}

	/* A dependency of @p key's mod on the mod with @p otherId, found (or
	 * not) as @p otherKey. */
	Issue dependency(Kind kind, const QString& key, const QString& name,
					 const QString& otherId, const QString& otherKey,
					 const QString& otherName)
	{
		Issue i = issue(kind, key, name);
		i.otherId = otherId;
		i.otherKey = otherKey;
		i.otherName = otherName;
		i.dependency.id = otherId;
		i.dependency.versions = {QStringLiteral(">=0.92.0")};
		return i;
	}

	QString roleOf(const ModHealthModel& model, int row, int role)
	{
		return model.index(row, 0).data(role).toString();
	}
} // namespace

/*
 * The health list over made-up issues: ModHealth::check() is tested on its
 * own, and InstanceContentController feeds this model from a real mods
 * folder, so here only what the model makes of an issue is checked. Texts
 * are only checked for the names they must mention, so the copy can change.
 */
class ModHealthModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void startsEmpty()
	{
		ModHealthModel model;
		QCOMPARE(model.count(), 0);
		QCOMPARE(model.blockingCount(), 0);
		QCOMPARE(model.revision(), 0);
		QVERIFY(model.problemsFor("a.jar").isEmpty());
		QVERIFY(!model.blockingFor("a.jar"));
	}

	void countsRowsAndBumpsRevision()
	{
		ModHealthModel model;
		QSignalSpy changed(&model, &ModHealthModel::changed);

		model.setIssues(
			{issue(Kind::MissingDependency, "a.jar", "Alpha"),
			 issue(Kind::Discouraged, "b.jar", "Beta", false),
			 issue(Kind::WrongLoader, "c.jar", "Gamma", false)});
		QCOMPARE(model.count(), 3);
		QCOMPARE(model.rowCount(), 3);
		QCOMPARE(model.blockingCount(), 1);
		QCOMPARE(model.revision(), 1);
		QCOMPARE(changed.count(), 1);

		model.setIssues({});
		QCOMPARE(model.count(), 0);
		QCOMPARE(model.blockingCount(), 0);
		QCOMPARE(model.revision(), 2);
		QCOMPARE(changed.count(), 2);
	}

	void rowsKeepTheOrderOfTheIssues()
	{
		ModHealthModel model;
		model.setIssues({issue(Kind::MissingDependency, "z.jar", "Zed"),
						 issue(Kind::Discouraged, "a.jar", "Alpha", false)});
		QCOMPARE(roleOf(model, 0, ModHealthModel::KeyRole),
				 QStringLiteral("z.jar"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::NameRole),
				 QStringLiteral("Zed"));
		QVERIFY(model.index(0, 0).data(ModHealthModel::BlockingRole).toBool());
		QVERIFY(!model.index(1, 0).data(ModHealthModel::BlockingRole).toBool());
		QCOMPARE(model.roleNames().value(ModHealthModel::ActionKeyRole),
				 QByteArray("actionKey"));
	}

	void missingDependencyInstallsTheId()
	{
		ModHealthModel model;
		model.setIssues({dependency(Kind::MissingDependency, "a.jar", "Alpha",
									"fabric-api", "", "fabric-api")});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole),
				 QStringLiteral("install"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionLabelRole),
				 QStringLiteral("Install"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::DependencyIdRole),
				 QStringLiteral("fabric-api"));
		/* Nothing is installed to replace. */
		QVERIFY(roleOf(model, 0, ModHealthModel::ReplacesKeyRole).isEmpty());
		QVERIFY(roleOf(model, 0, ModHealthModel::ActionKeyRole).isEmpty());
		QCOMPARE(model.roleNames().value(ModHealthModel::DependencyIdRole),
				 QByteArray("dependencyId"));
		QCOMPARE(model.roleNames().value(ModHealthModel::ReplacesKeyRole),
				 QByteArray("replacesKey"));
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole).contains("Alpha"));
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole)
					.contains("fabric-api"));
		QVERIFY(!roleOf(model, 0, ModHealthModel::DetailRole).isEmpty());
	}

	void disabledDependencyTurnsTheFileOn()
	{
		ModHealthModel model;
		model.setIssues({dependency(Kind::DisabledDependency, "a.jar", "Alpha",
									"beta", "beta-1.0.jar", "Beta")});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole),
				 QStringLiteral("enable"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionKeyRole),
				 QStringLiteral("beta-1.0.jar"));
		QVERIFY(roleOf(model, 0, ModHealthModel::ActionLabelRole)
					.contains("Beta"));
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole).contains("Alpha"));
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole).contains("Beta"));
	}

	void wrongDependencyVersionUpdatesTheFileInstalled()
	{
		Issue i = dependency(Kind::WrongDependencyVersion, "a.jar", "Alpha",
							 "beta", "beta-0.1.jar", "Beta");
		i.foundVersion = "0.1.0";
		ModHealthModel model;
		model.setIssues({i});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole),
				 QStringLiteral("install"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionLabelRole),
				 QStringLiteral("Update"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::DependencyIdRole),
				 QStringLiteral("beta"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ReplacesKeyRole),
				 QStringLiteral("beta-0.1.jar"));
		QVERIFY(roleOf(model, 0, ModHealthModel::DetailRole).contains("0.1.0"));
	}

	void installRowsListEveryInstallRowInOrder()
	{
		ModHealthModel model;
		QVERIFY(model.installRows().isEmpty());

		model.setIssues(
			{dependency(Kind::MissingDependency, "a.jar", "Alpha", "fabric-api",
						"", "fabric-api"),
			 dependency(Kind::DisabledDependency, "b.jar", "Beta", "gamma",
						"gamma-1.0.jar", "Gamma"),
			 dependency(Kind::WrongDependencyVersion, "c.jar", "Cee", "beta",
						"beta-0.1.jar", "Beta"),
			 issue(Kind::WrongLoader, "d.jar", "Dee", false)});
		const QVariantList rows = model.installRows();
		QCOMPARE(rows.size(), 2);

		const QVariantMap first = rows.at(0).toMap();
		QCOMPARE(first.value("key").toString(), QStringLiteral("a.jar"));
		QCOMPARE(first.value("dependencyId").toString(),
				 QStringLiteral("fabric-api"));
		QVERIFY(first.value("replacesKey").toString().isEmpty());

		const QVariantMap second = rows.at(1).toMap();
		QCOMPARE(second.value("key").toString(), QStringLiteral("c.jar"));
		QCOMPARE(second.value("dependencyId").toString(), QStringLiteral("beta"));
		QCOMPARE(second.value("replacesKey").toString(),
				 QStringLiteral("beta-0.1.jar"));

		/* Follows the check as it is redone. */
		model.setIssues({});
		QVERIFY(model.installRows().isEmpty());
	}

	void wrongMinecraftNamesTheVersionOfTheEnvironment()
	{
		ModHealth::Environment environment;
		environment.minecraft = "1.21.4";
		ModHealthModel model;
		model.setEnvironment(environment);
		model.setIssues(
			{dependency(Kind::WrongMinecraft, "a.jar", "Alpha", "minecraft", "",
						"minecraft")});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole),
				 QStringLiteral("disable"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionKeyRole),
				 QStringLiteral("a.jar"));
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole).contains("1.21.4"));
		QVERIFY(model.problemsFor("a.jar").first().contains("1.21.4"));
	}

	void wrongLoaderVersionOffersNoAction()
	{
		Issue i = dependency(Kind::WrongLoaderVersion, "a.jar", "Alpha",
							 "fabricloader", "", "fabricloader");
		i.foundVersion = "0.14.0";
		ModHealthModel model;
		model.setIssues({i});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole), QString());
		QVERIFY(roleOf(model, 0, ModHealthModel::ActionKeyRole).isEmpty());
		QVERIFY(roleOf(model, 0, ModHealthModel::ActionLabelRole).isEmpty());
		QVERIFY(roleOf(model, 0, ModHealthModel::ReplacesKeyRole).isEmpty());
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole)
					.contains("Fabric Loader"));
		QVERIFY(roleOf(model, 0, ModHealthModel::DetailRole)
					.contains("0.14.0"));
	}

	void clashesTurnTheOtherFileOff()
	{
		ModHealthModel model;
		model.setIssues(
			{dependency(Kind::Incompatible, "a.jar", "Alpha", "beta",
						"beta-1.0.jar", "Beta"),
			 dependency(Kind::Discouraged, "c.jar", "Gamma", "delta",
						"delta-1.0.jar", "Delta")});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole),
				 QStringLiteral("disable"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionKeyRole),
				 QStringLiteral("beta-1.0.jar"));
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole).contains("Beta"));
		QCOMPARE(roleOf(model, 1, ModHealthModel::ActionKeyRole),
				 QStringLiteral("delta-1.0.jar"));
		QVERIFY(roleOf(model, 1, ModHealthModel::TitleRole).contains("Delta"));
	}

	void clashWithTheLoaderTurnsTheModOff()
	{
		ModHealthModel model;
		model.setIssues({dependency(Kind::Incompatible, "a.jar", "Alpha",
									"quilt_loader", "", "quilt_loader")});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionKeyRole),
				 QStringLiteral("a.jar"));
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole)
					.contains("Quilt Loader"));
	}

	void wrongLoaderTurnsTheFileOffAndNamesItsLoaders()
	{
		ModHealth::Environment environment;
		environment.loaders = {"fabric"};
		ModHealthModel model;
		model.setEnvironment(environment);
		model.setFileLoaders({{"a.jar", {"forge", "neoforge"}}});
		model.setIssues({issue(Kind::WrongLoader, "a.jar", "Alpha", false)});
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole),
				 QStringLiteral("disable"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionKeyRole),
				 QStringLiteral("a.jar"));
		const QString title = roleOf(model, 0, ModHealthModel::TitleRole);
		QVERIFY(title.contains("Alpha"));
		QVERIFY(title.contains("Forge"));
		QVERIFY(title.contains("NeoForge"));
		QVERIFY(roleOf(model, 0, ModHealthModel::DetailRole).contains("Fabric"));

		/* No loaders known for the file: still a title, just not that one. */
		model.setFileLoaders({});
		model.setIssues({issue(Kind::WrongLoader, "a.jar", "Alpha", false)});
		QVERIFY(roleOf(model, 0, ModHealthModel::TitleRole).contains("Alpha"));
	}

	void aDuplicatePairIsOneRowButBothFilesAreAffected()
	{
		Issue first = issue(Kind::Duplicate, "sodium-0.5.jar", "Sodium");
		first.otherId = "sodium";
		first.otherKey = "sodium-0.6.jar";
		first.otherName = "Sodium";
		Issue second = first;
		second.key = "sodium-0.6.jar";
		second.otherKey = "sodium-0.5.jar";

		ModHealthModel model;
		model.setIssues({first, second});
		QCOMPARE(model.count(), 1);
		QCOMPARE(model.blockingCount(), 1);
		QCOMPARE(roleOf(model, 0, ModHealthModel::KeyRole),
				 QStringLiteral("sodium-0.5.jar"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionRole),
				 QStringLiteral("disable"));
		QCOMPARE(roleOf(model, 0, ModHealthModel::ActionKeyRole),
				 QStringLiteral("sodium-0.5.jar"));
		QVERIFY(roleOf(model, 0, ModHealthModel::ActionLabelRole)
					.contains("sodium-0.5.jar"));
		const QString detail = roleOf(model, 0, ModHealthModel::DetailRole);
		QVERIFY(detail.contains("sodium-0.5.jar"));
		QVERIFY(detail.contains("sodium-0.6.jar"));

		QCOMPARE(model.problemsFor("sodium-0.5.jar").size(), 1);
		QCOMPARE(model.problemsFor("sodium-0.6.jar").size(), 1);
		QVERIFY(model.blockingFor("sodium-0.5.jar"));
		QVERIFY(model.blockingFor("sodium-0.6.jar"));
	}

	void problemsForListsBlockingOnesFirst()
	{
		Issue warning = dependency(Kind::Discouraged, "a.jar", "Alpha", "beta",
								   "beta-1.0.jar", "Beta");
		warning.blocking = false;
		const Issue blocking = dependency(Kind::MissingDependency, "a.jar",
										  "Alpha", "gamma", "", "gamma");

		ModHealthModel model;
		model.setIssues({warning, blocking,
						 issue(Kind::MissingDependency, "other.jar", "Other")});
		const QStringList problems = model.problemsFor("a.jar");
		QCOMPARE(problems.size(), 2);
		QVERIFY(problems.at(0).contains("gamma"));
		QVERIFY(problems.at(1).contains("Beta"));
		QVERIFY(model.blockingFor("a.jar"));
		QVERIFY(model.blockingFor("other.jar"));
		QVERIFY(!model.blockingFor("beta-1.0.jar"));
		QVERIFY(model.problemsFor("beta-1.0.jar").isEmpty());
	}

	void aWarningAloneIsNotBlocking()
	{
		ModHealthModel model;
		model.setIssues({issue(Kind::Discouraged, "a.jar", "Alpha", false)});
		QVERIFY(!model.blockingFor("a.jar"));
		QCOMPARE(model.problemsFor("a.jar").size(), 1);
		QCOMPARE(model.blockingCount(), 0);
	}

	void environmentOfNoInstanceIsEmpty()
	{
		const ModHealth::Environment environment =
			ModHealthModel::environmentFor(nullptr);
		QVERIFY(environment.minecraft.isEmpty());
		QVERIFY(environment.loaders.isEmpty());
		QVERIFY(environment.builtIns.isEmpty());
	}

	void entriesOfNoModsAreEmpty()
	{
		QVERIFY(ModHealthModel::entriesFor({}).isEmpty());
	}

	void builtInsAndLoadersReadAsAPersonSaysThem()
	{
		QCOMPARE(ModHealthModel::builtInName("fabricloader"),
				 QStringLiteral("Fabric Loader"));
		QCOMPARE(ModHealthModel::builtInName("quilt_loader"),
				 QStringLiteral("Quilt Loader"));
		QCOMPARE(ModHealthModel::builtInName("minecraft"),
				 QStringLiteral("Minecraft"));
		QCOMPARE(ModHealthModel::builtInName("neoforge"),
				 QStringLiteral("NeoForge"));
		QCOMPARE(ModHealthModel::builtInName("sodium"),
				 QStringLiteral("sodium"));

		QCOMPARE(ModHealthModel::loaderName("fabric"),
				 QStringLiteral("Fabric"));
		QCOMPARE(ModHealthModel::loaderName("quilt"), QStringLiteral("Quilt"));
		QCOMPARE(ModHealthModel::loaderName("forge"), QStringLiteral("Forge"));
		QCOMPARE(ModHealthModel::loaderName("neoforge"),
				 QStringLiteral("NeoForge"));
		QCOMPARE(ModHealthModel::loaderName("liteloader"),
				 QStringLiteral("LiteLoader"));
		QCOMPARE(ModHealthModel::loaderName("other"), QStringLiteral("other"));
	}
};

QTEST_GUILESS_MAIN(ModHealthModelTest)

#include "ModHealthModel_test.moc"
