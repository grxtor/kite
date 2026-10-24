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

#include <QTest>

#include "minecraft/mod/ModHealth.h"

using namespace ModHealth;

namespace
{
	const QStringList FabricOnly = { QStringLiteral("fabric") };
	const QStringList ForgeOnly = { QStringLiteral("forge") };

	QString kindName(Kind kind)
	{
		switch (kind) {
		case Kind::MissingDependency:
			return QStringLiteral("Missing");
		case Kind::DisabledDependency:
			return QStringLiteral("Disabled");
		case Kind::WrongDependencyVersion:
			return QStringLiteral("WrongVersion");
		case Kind::WrongMinecraft:
			return QStringLiteral("WrongMinecraft");
		case Kind::WrongLoaderVersion:
			return QStringLiteral("WrongLoaderVersion");
		case Kind::Incompatible:
			return QStringLiteral("Incompatible");
		case Kind::Discouraged:
			return QStringLiteral("Discouraged");
		case Kind::Duplicate:
			return QStringLiteral("Duplicate");
		case Kind::WrongLoader:
			return QStringLiteral("WrongLoader");
		}
		return QString();
	}

	//! "Kind file other", one line per issue, so a whole result reads at once.
	QStringList summary(const QList<Issue>& issues)
	{
		QStringList lines;
		for (const Issue& issue : issues) {
			lines.append(QStringLiteral("%1 %2 %3")
							 .arg(kindName(issue.kind), issue.key, issue.otherId)
							 .trimmed());
		}
		return lines;
	}

	ModDependency declaration(ModDependency::Kind kind,
							  ModDependency::Syntax syntax, const QString& id,
							  const QString& range, const QString& loader)
	{
		ModDependency result;
		result.id = id;
		result.kind = kind;
		result.syntax = syntax;
		if (!range.isEmpty())
			result.versions = { range };
		result.loader = loader;
		return result;
	}

	ModDependency need(const QString& id, const QString& range = QString(),
					   const QString& loader = QStringLiteral("fabric"))
	{
		return declaration(ModDependency::Kind::Required,
						   ModDependency::Syntax::Fabric, id, range, loader);
	}

	ModDependency needMaven(const QString& id, const QString& range,
							const QString& loader = QStringLiteral("forge"))
	{
		return declaration(ModDependency::Kind::Required,
						   ModDependency::Syntax::Maven, id, range, loader);
	}

	ModDependency breaks(const QString& id, const QString& range = QString())
	{
		return declaration(ModDependency::Kind::Incompatible,
						   ModDependency::Syntax::Fabric, id, range,
						   QStringLiteral("fabric"));
	}

	ModDependency conflicts(const QString& id,
							const QString& range = QString())
	{
		return declaration(ModDependency::Kind::Discouraged,
						   ModDependency::Syntax::Fabric, id, range,
						   QStringLiteral("fabric"));
	}

	ProvidedMod provided(const QString& id, const QString& version)
	{
		return { id, version };
	}

	Entry mod(const QString& id, const QString& version,
			  const QStringList& loaders = FabricOnly,
			  const QList<ModDependency>& dependencies = {},
			  const QList<ProvidedMod>& provides = {})
	{
		Entry entry;
		entry.key = id + QStringLiteral(".jar");
		entry.name = id;
		entry.details.mod_id = id;
		entry.details.version = version;
		entry.details.loaders = loaders;
		entry.details.dependencies = dependencies;
		entry.details.provides = provides;
		return entry;
	}

	//! A file the caller has not read yet: details left as they were made.
	Entry unread(const QString& key)
	{
		Entry entry;
		entry.key = key;
		entry.name = key;
		return entry;
	}

	Entry switchedOff(Entry entry)
	{
		entry.enabled = false;
		return entry;
	}

	Entry named(Entry entry, const QString& key, const QString& name)
	{
		entry.key = key;
		entry.name = name;
		return entry;
	}

	Environment environment(const QString& minecraft, const QStringList& loaders,
							const QHash<QString, QString>& builtIns)
	{
		Environment result;
		result.minecraft = minecraft;
		result.loaders = loaders;
		result.builtIns = builtIns;
		return result;
	}

	Environment fabricEnvironment()
	{
		return environment(QStringLiteral("1.20.1"), FabricOnly,
						   { { QStringLiteral("minecraft"), QStringLiteral("1.20.1") },
							 { QStringLiteral("fabricloader"),
							   QStringLiteral("0.15.7") } });
	}

	Environment loaderEnvironment(const QString& minecraft,
								  const QString& loader)
	{
		return environment(minecraft, { loader },
						   { { QStringLiteral("minecraft"), minecraft } });
	}

	//! A jar with no metadata of its own that only carries mods
	//! (kotlinforforge-all.jar, the Connector jar): it provides ids and is
	//! nothing else.
	Entry wrapperJar(const QString& key, const QList<ProvidedMod>& provides)
	{
		Entry entry;
		entry.key = key;
		entry.name = key;
		entry.details.provides = provides;
		return entry;
	}

	Environment neoForgeEnvironment(const QString& minecraft)
	{
		return environment(minecraft, { QStringLiteral("neoforge") },
						   { { QStringLiteral("minecraft"), minecraft },
							 { QStringLiteral("neoforge"),
							   QStringLiteral("21.1.248") } });
	}

	Environment forgeEnvironment(const QString& minecraft,
								 const QString& forge)
	{
		return environment(minecraft, ForgeOnly,
						   { { QStringLiteral("minecraft"), minecraft },
							 { QStringLiteral("forge"), forge } });
	}

	//! Every readable loader as one word, in order: "neoforge,forge".
	QString readable(const Environment& env, bool connector = false)
	{
		return readableLoaders(env, connector).join(QLatin1Char(','));
	}
} // namespace

/*!
 * The mod health check over made-up folders. Each test builds the few mods
 * it needs and compares the whole result, so an issue that appears where it
 * should not fails the test just as one that goes missing does.
 *
 * Half of these are "nothing is reported": the check names a mod as broken,
 * and every way of being unsure (a version that is free text, a file not
 * read yet, a mod that is switched off) has to end in silence.
 */
class ModHealthTest : public QObject
{
	Q_OBJECT

  private slots:
	void healthyFabricFolder()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly,
				{ need("fabric-api", ">=0.90"), need("minecraft", "1.20.x"),
				  need("fabricloader", ">=0.14.21"), need("java", ">=17") }),
			mod("fabric-api", "0.92.2+1.20.1", FabricOnly, {},
				{ provided("fabric", "0.92.2+1.20.1"),
				  provided("fabric-api-base", "0.4.31"),
				  provided("fabric-resource-loader-v0", "0.11.10") }),
			mod("lithium", "0.11.2", FabricOnly,
				{ need("fabric-api-base", ">=0.4"),
				  need("fabric-resource-loader-v0") }),
			mod("modmenu", "7.2.2", FabricOnly, { need("fabric", "*") }),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())), QStringList());
	}

	void missingDependency()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly,
				{ need("fabric-api", ">=0.90") }),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		QCOMPARE(summary(issues),
				 QStringList{ QStringLiteral("Missing sodium.jar fabric-api") });
		QVERIFY(issues.first().blocking);
		QCOMPARE(issues.first().otherKey, QString());
		QCOMPARE(issues.first().otherName, QStringLiteral("fabric-api"));
		QCOMPARE(issues.first().dependency.versions,
				 QStringList{ QStringLiteral(">=0.90") });
	}

	void dependencyOnlyInASwitchedOffFile()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly, { need("fabric-api", ">=0.90") }),
			named(switchedOff(mod("fabric-api", "0.92.2")), "fabric-api.jar.disabled",
				  "Fabric API"),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		QCOMPARE(summary(issues),
				 QStringList{
					 QStringLiteral("Disabled sodium.jar fabric-api") });
		QVERIFY(issues.first().blocking);
		QCOMPARE(issues.first().otherKey,
				 QStringLiteral("fabric-api.jar.disabled"));
		QCOMPARE(issues.first().otherName, QStringLiteral("Fabric API"));
	}

	void dependencyTooOld()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly, { need("fabric-api", ">=0.90") }),
			mod("fabric-api", "0.80.0"),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		QCOMPARE(summary(issues),
				 QStringList{
					 QStringLiteral("WrongVersion sodium.jar fabric-api") });
		QVERIFY(issues.first().blocking);
		QCOMPARE(issues.first().foundVersion, QStringLiteral("0.80.0"));
		QCOMPARE(issues.first().otherKey, QStringLiteral("fabric-api.jar"));
		QCOMPARE(issues.first().otherName, QStringLiteral("fabric-api"));
	}

	void oneProviderAtTheRightVersionIsEnough()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly, { need("fabric-api", ">=0.90") }),
			mod("fabric-api", "0.80.0"),
			// Some other file that also answers to the id.
			mod("api-bundle", "1.0", FabricOnly, {},
				{ provided("fabric-api", "0.95.0") }),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())), QStringList());
	}

	void dependencyWithAVersionNobodyCanRead()
	{
		for (const QString& version :
			 { QStringLiteral("${version}"), QStringLiteral("23w45a"),
			   QString() }) {
			const QList<Entry> folder = {
				mod("sodium", "0.5.3", FabricOnly,
					{ need("fabric-api", ">=0.90") }),
				mod("fabric-api", version),
			};
			QCOMPARE(summary(check(folder, fabricEnvironment())),
					 QStringList());
		}
	}

	void wrongMinecraftAndLoaderVersion()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly,
				{ need("minecraft", ">=1.21"), need("fabricloader", ">=0.16") }),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		QCOMPARE(summary(issues),
				 (QStringList{
					 QStringLiteral("WrongMinecraft sodium.jar minecraft"),
					 QStringLiteral(
						 "WrongLoaderVersion sodium.jar fabricloader") }));
		QCOMPARE(issues.at(0).foundVersion, QStringLiteral("1.20.1"));
		QCOMPARE(issues.at(0).otherKey, QString());
		QCOMPARE(issues.at(1).foundVersion, QStringLiteral("0.15.7"));
		QVERIFY(issues.at(0).blocking && issues.at(1).blocking);
	}

	void builtInWithoutAVersionIsNotChecked()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly,
				{ need("minecraft", ">=1.21"), need("fabricloader", ">=0.16") }),
		};
		const Environment unknown = environment(
			QString(), FabricOnly,
			{ { QStringLiteral("minecraft"), QString() },
			  { QStringLiteral("fabricloader"), QString() } });
		QCOMPARE(summary(check(folder, unknown)), QStringList());

		// Not listed at all, and no version of Minecraft either: still
		// something the game provides, never a missing mod.
		QCOMPARE(summary(check(folder, environment(QString(), FabricOnly, {}))),
				 QStringList());
	}

	void minecraftDefaultsToTheInstanceVersion()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly, { need("minecraft", ">=1.21") }),
		};
		QCOMPARE(summary(check(folder,
							   environment(QStringLiteral("1.20.1"), FabricOnly,
										   {}))),
				 QStringList{
					 QStringLiteral("WrongMinecraft sodium.jar minecraft") });
	}

	void breaksIsIncompatibleAndConflictsIsOnlyAWarning()
	{
		const QList<Entry> folder = {
			mod("optifine-compat", "1.0", FabricOnly,
				{ breaks("sodium"), conflicts("iris") }),
			mod("sodium", "0.5.3"),
			mod("iris", "1.6.0"),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		QCOMPARE(summary(issues),
				 (QStringList{
					 QStringLiteral("Incompatible optifine-compat.jar sodium"),
					 QStringLiteral("Discouraged optifine-compat.jar iris") }));
		QVERIFY(issues.at(0).blocking);
		QVERIFY(!issues.at(1).blocking);
		QCOMPARE(issues.at(0).otherKey, QStringLiteral("sodium.jar"));
		QCOMPARE(issues.at(0).otherName, QStringLiteral("sodium"));
		QCOMPARE(issues.at(1).otherKey, QStringLiteral("iris.jar"));
	}

	void clashesOnlyWhenTheVersionSurelyMatches()
	{
		const auto clashWith = [](const QString& version, const QString& range) {
			const QList<Entry> folder = {
				mod("a", "1.0", FabricOnly, { breaks("b", range) }),
				mod("b", version),
			};
			return summary(check(folder, fabricEnvironment()));
		};
		const QStringList clash = { QStringLiteral("Incompatible a.jar b") };

		QCOMPARE(clashWith(QStringLiteral("0.5"), QStringLiteral("<1.0")),
				 clash);
		QCOMPARE(clashWith(QStringLiteral("2.0"), QStringLiteral("<1.0")),
				 QStringList());
		// Free text: not known to be inside the range, so not reported.
		QCOMPARE(clashWith(QStringLiteral("${version}"), QStringLiteral("<1.0")),
				 QStringList());
		QCOMPARE(clashWith(QString(), QStringLiteral("<1.0")), QStringList());
		// Any version of it, whatever it says about itself.
		QCOMPARE(clashWith(QStringLiteral("${version}"), QStringLiteral("*")),
				 clash);
		QCOMPARE(clashWith(QStringLiteral("2.0"), QString()), clash);
	}

	void clashesIgnoreSwitchedOffFilesAndThemselves()
	{
		QCOMPARE(summary(check({ mod("a", "1.0", FabricOnly, { breaks("b") }),
								 switchedOff(mod("b", "1.0")) },
							   fabricEnvironment())),
				 QStringList());
		// A mod that lists itself, or an id it provides, among its enemies.
		QCOMPARE(summary(check({ mod("a", "1.0", FabricOnly,
									 { breaks("a"), conflicts("a-legacy") },
									 { provided("a-legacy", "1.0") }) },
							   fabricEnvironment())),
				 QStringList());
		// Someone else's provided id is another matter.
		const QList<Issue> issues = check(
			{ mod("a", "1.0", FabricOnly, { breaks("b-legacy") }),
			  mod("b", "1.0", FabricOnly, {}, { provided("b-legacy", "1.0") }) },
			fabricEnvironment());
		QCOMPARE(summary(issues),
				 QStringList{ QStringLiteral("Incompatible a.jar b-legacy") });
		QCOMPARE(issues.first().otherKey, QStringLiteral("b.jar"));
	}

	void clashesWithTheGameItself()
	{
		const QList<Entry> folder = {
			mod("old-mod", "1.0", FabricOnly,
				{ breaks("minecraft", "<1.21"), conflicts("minecraft", "1.20.x"),
				  breaks("minecraft", "<1.20") }),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		QCOMPARE(summary(issues),
				 (QStringList{
					 QStringLiteral("Incompatible old-mod.jar minecraft"),
					 QStringLiteral("Discouraged old-mod.jar minecraft") }));
		QCOMPARE(issues.at(0).otherKey, QString());
		QCOMPARE(issues.at(0).otherName, QStringLiteral("minecraft"));
	}

	void duplicates()
	{
		const QList<Entry> folder = {
			named(mod("sodium", "0.5.3"), "sodium-0.5.3.jar", "Sodium 0.5.3"),
			named(mod("Sodium", "0.5.2"), "sodium-0.5.2.jar", "Sodium 0.5.2"),
			mod("lithium", "0.11.2"),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		// By name, so the older copy comes first; each points at the other.
		QCOMPARE(summary(issues),
				 (QStringList{
					 QStringLiteral("Duplicate sodium-0.5.2.jar Sodium"),
					 QStringLiteral("Duplicate sodium-0.5.3.jar sodium") }));
		QVERIFY(issues.at(0).blocking && issues.at(1).blocking);
		QCOMPARE(issues.at(0).otherKey, QStringLiteral("sodium-0.5.3.jar"));
		QCOMPARE(issues.at(0).otherName, QStringLiteral("Sodium 0.5.3"));
		QCOMPARE(issues.at(1).otherKey, QStringLiteral("sodium-0.5.2.jar"));
		QCOMPARE(issues.at(1).otherName, QStringLiteral("Sodium 0.5.2"));
	}

	void threeCopiesAreAllReported()
	{
		const QList<Entry> folder = {
			named(mod("sodium", "1"), "a.jar", "a"),
			named(mod("sodium", "2"), "b.jar", "b"),
			named(mod("sodium", "3"), "c.jar", "c"),
		};
		const QList<Issue> issues = check(folder, fabricEnvironment());
		QCOMPARE(summary(issues),
				 (QStringList{ QStringLiteral("Duplicate a.jar sodium"),
							   QStringLiteral("Duplicate b.jar sodium"),
							   QStringLiteral("Duplicate c.jar sodium") }));
		for (const Issue& issue : issues)
			QVERIFY(!issue.otherKey.isEmpty() && issue.otherKey != issue.key);
	}

	void aSwitchedOffCopyIsNotADuplicate()
	{
		const QList<Entry> folder = {
			named(mod("sodium", "0.5.3"), "new.jar", "new"),
			named(switchedOff(mod("sodium", "0.5.2")), "old.jar.disabled", "old"),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())), QStringList());
	}

	void aFileForAnotherLoader()
	{
		Entry create = mod("create", "0.5.1", ForgeOnly,
						   { needMaven("forge", "[47,)"),
							 needMaven("flywheel", "[0.6,)") });
		const QList<Issue> issues = check({ create }, fabricEnvironment());
		QCOMPARE(summary(issues),
				 QStringList{ QStringLiteral("WrongLoader create.jar") });
		QVERIFY(!issues.first().blocking);
		QCOMPARE(issues.first().otherId, QString());
		QCOMPARE(issues.first().otherKey, QString());
	}

	void theOtherLoadersBuildOfAModIsNotADuplicate()
	{
		// The loader never opens the forge copy, so there is only one.
		const QList<Entry> folder = {
			named(mod("create", "0.5.1", FabricOnly), "create-fabric.jar",
				  "create fabric"),
			named(mod("create", "0.5.1", ForgeOnly), "create-forge.jar",
				  "create forge"),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 QStringList{ QStringLiteral("WrongLoader create-forge.jar") });
	}

	void aFileTheLoaderNeverOpensIsNoClash()
	{
		const QList<Entry> folder = {
			mod("steam-trains", "1.0", FabricOnly, { breaks("create") }),
			mod("create", "0.5.1", ForgeOnly),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 QStringList{ QStringLiteral("WrongLoader create.jar") });
	}

	void aFileForAnotherLoaderStillCountsAsPresent()
	{
		// The jar is there, even if the loader will not open it, so the mod
		// that needs it is not told it is "missing" on top of the loader
		// report: that would be one problem named twice.
		const QList<Entry> folder = {
			mod("steam-trains", "1.0", FabricOnly, { need("create") }),
			mod("create", "0.5.1", ForgeOnly),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 QStringList{ QStringLiteral("WrongLoader create.jar") });
	}

	void quiltReadsFabricJars()
	{
		const Environment quilt = loaderEnvironment("1.20.1", "quilt");
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly, { need("fabric-api") }),
			mod("qsl", "1.0", { "quilt" }),
			mod("only-forge", "1.0", ForgeOnly),
		};
		QCOMPARE(summary(check(folder, quilt)),
				 (QStringList{
					 QStringLiteral("Missing sodium.jar fabric-api"),
					 QStringLiteral("WrongLoader only-forge.jar") }));
		QCOMPARE(summary(check({ mod("q", "1.0", { "quilt" }) },
							   fabricEnvironment())),
				 QStringList{ QStringLiteral("WrongLoader q.jar") });
	}

	void quiltFileUsesItsOwnNeedsFirst()
	{
		const QList<ModDependency> both = {
			need("needed-by-fabric", QString(), "fabric"),
			need("needed-by-quilt", QString(), "quilt"),
		};
		const QList<Entry> folder = {
			mod("dual", "1.0", { "fabric", "quilt" }, both),
		};
		QCOMPARE(summary(check(folder, loaderEnvironment("1.20.1", "quilt"))),
				 QStringList{
					 QStringLiteral("Missing dual.jar needed-by-quilt") });
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 QStringList{
					 QStringLiteral("Missing dual.jar needed-by-fabric") });
	}

	void neoForgeReadsForgeJarsBeforeTheCut_data()
	{
		QTest::addColumn<QString>("minecraft");
		QTest::addColumn<bool>("readsForge");

		QTest::newRow("1.20.1") << QStringLiteral("1.20.1") << true;
		QTest::newRow("1.20.4") << QStringLiteral("1.20.4") << true;
		QTest::newRow("1.20.5") << QStringLiteral("1.20.5") << false;
		QTest::newRow("1.20.6") << QStringLiteral("1.20.6") << false;
		QTest::newRow("1.21") << QStringLiteral("1.21") << false;
		QTest::newRow("1.21.1") << QStringLiteral("1.21.1") << false;
		// Cannot be ordered: the jar gets the benefit of the doubt.
		QTest::newRow("snapshot") << QStringLiteral("24w14a") << true;
		QTest::newRow("unknown") << QString() << true;
	}

	void neoForgeReadsForgeJarsBeforeTheCut()
	{
		QFETCH(QString, minecraft);
		QFETCH(bool, readsForge);

		const Environment neoforge = loaderEnvironment(minecraft, "neoforge");
		const QList<Entry> folder = { mod("jei", "15.0", ForgeOnly) };
		QCOMPARE(summary(check(folder, neoforge)),
				 readsForge ? QStringList()
							: QStringList{ QStringLiteral("WrongLoader jei.jar") });
		// Its own metadata is always read.
		QCOMPARE(summary(check({ mod("jei", "15.0", { "neoforge" }) },
								neoforge)),
				 QStringList());
	}

	void connectorLetsForgeLoadersReadFabricJars()
	{
		const QList<Entry> sodium = { mod("sodium", "0.5.3", FabricOnly) };
		const Environment neoforge = loaderEnvironment("1.20.1", "neoforge");
		const QStringList wrong = { QStringLiteral("WrongLoader sodium.jar") };

		QCOMPARE(summary(check(sodium, neoforge)), wrong);

		QList<Entry> withConnector = sodium;
		withConnector.append(mod("connector", "1.0.0", { "neoforge" }));
		QCOMPARE(summary(check(withConnector, neoforge)), QStringList());

		QList<Entry> onForge = sodium;
		onForge.append(mod("connector", "1.0.0", ForgeOnly));
		QCOMPARE(summary(check(onForge, loaderEnvironment("1.20.1", "forge"))),
				 QStringList());

		// Switched off, it runs nothing.
		QList<Entry> switchedOffConnector = sodium;
		switchedOffConnector.append(
			switchedOff(mod("Connector", "1.0.0", { "neoforge" })));
		QCOMPARE(summary(check(switchedOffConnector, neoforge)), wrong);
	}

	void connectorChangesNothingOnFabric()
	{
		const QList<Entry> folder = {
			mod("create", "1.0", ForgeOnly),
			mod("connector", "1.0.0", { "neoforge" }),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 (QStringList{ QStringLiteral("WrongLoader connector.jar"),
							   QStringLiteral("WrongLoader create.jar") }));
	}

	void aJarForSeveralLoadersCountsTheNeedsOfTheOneThatReadsIt()
	{
		const QList<ModDependency> both = {
			need("needed-by-fabric", QString(), "fabric"),
			needMaven("needed-by-forge", QString(), "forge"),
		};
		const QList<Entry> folder = {
			mod("multi", "1.0", { "fabric", "forge" }, both),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 QStringList{
					 QStringLiteral("Missing multi.jar needed-by-fabric") });
		QCOMPARE(summary(check(folder, loaderEnvironment("1.20.1", "forge"))),
				 QStringList{
					 QStringLiteral("Missing multi.jar needed-by-forge") });
	}

	void theRuntimeProvidesItsOwnIds()
	{
		QList<ModDependency> runtime;
		for (const char* id :
			 { "java", "fml", "javafml", "lowcodefml", "mixinextras", "JAVA" }) {
			runtime.append(needMaven(QString::fromLatin1(id), "[1,)"));
		}
		QCOMPARE(summary(check({ mod("a", "1.0", ForgeOnly, runtime) },
							   loaderEnvironment("1.20.1", "forge"))),
				 QStringList());
	}

	void loaderIdsAreNotMissingMods()
	{
		const QList<Entry> folder = {
			mod("jei", "15.0", ForgeOnly,
				{ needMaven("forge", "[47.2,)"), needMaven("minecraft", "[1.20,1.21)") }),
		};
		// NeoForge 1.20.1 answers to "forge" too, whether or not it was listed.
		QCOMPARE(summary(check(folder, loaderEnvironment("1.20.1", "neoforge"))),
				 QStringList());

		Environment listed = loaderEnvironment("1.20.1", "forge");
		listed.builtIns.insert(QStringLiteral("forge"), QStringLiteral("47.2.0"));
		QCOMPARE(summary(check(folder, listed)), QStringList());

		// Listed with a version that is too old, in whatever case. (Not a
		// 46.x against [47,): Forge on 1.20.1 also answers to 47.1.79.)
		Environment tooOld = loaderEnvironment("1.20.1", "forge");
		tooOld.builtIns.insert(QStringLiteral("Forge"), QStringLiteral("46.0.1"));
		QCOMPARE(summary(check(folder, tooOld)),
				 QStringList{ QStringLiteral("WrongLoaderVersion jei.jar forge") });
	}

	void mavenRangesAgainstInstalledMods()
	{
		const Environment forge = loaderEnvironment("1.20.1", "forge");
		const QList<Entry> folder = {
			mod("create", "0.5.1", ForgeOnly,
				{ needMaven("flywheel", "[0.6,)"),
				  needMaven("registrate", "[1.3,2)"),
				  needMaven("ponder", "[1,2)") }),
			mod("flywheel", "0.5.0", ForgeOnly),
			mod("registrate", "1.20.1-1.3.3", ForgeOnly),
			mod("ponder", "${file.jarVersion}", ForgeOnly),
		};
		const QList<Issue> issues = check(folder, forge);
		QCOMPARE(summary(issues),
				 QStringList{ QStringLiteral("WrongVersion create.jar flywheel") });
		QCOMPARE(issues.first().foundVersion, QStringLiteral("0.5.0"));
		QVERIFY(issues.first().dependency.syntax == ModDependency::Syntax::Maven);
	}

	void wrongLoaderVersionFromMaven()
	{
		Environment forge = loaderEnvironment("1.20.1", "forge");
		forge.builtIns.insert(QStringLiteral("forge"), QStringLiteral("47.2.0"));
		const QList<Entry> folder = {
			mod("needs-new-forge", "1.0", ForgeOnly, { needMaven("forge", "[48,)") }),
			mod("fine", "1.0", ForgeOnly, { needMaven("forge", "[47,)") }),
		};
		QCOMPARE(summary(check(folder, forge)),
				 QStringList{ QStringLiteral(
					 "WrongLoaderVersion needs-new-forge.jar forge") });
	}

	void idsCompareWithoutCase()
	{
		const QList<Entry> folder = {
			mod("Sodium", "0.5.3", FabricOnly,
				{ need("FABRIC-API", ">=0.90"), need("Minecraft", "1.20.x") }),
			mod("fabric-api", "0.92.2", FabricOnly, {},
				{ provided("Fabric", "0.92.2") }),
			mod("lithium", "1.0", FabricOnly, { need("fabric") }),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())), QStringList());
	}

	void noLoaderInTheEnvironmentMeansNothingToCheck()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly, { need("fabric-api") }),
			mod("sodium", "0.5.2"),
			mod("create", "1.0", ForgeOnly),
		};
		QVERIFY(check(folder, environment(QStringLiteral("1.20.1"), {}, {}))
					.isEmpty());
	}

	void filesNotReadYetAndSwitchedOffTakePartInNothing()
	{
		const QList<Entry> folder = {
			unread("unread.jar"),
			unread("unread-too.jar"),
			switchedOff(mod("off", "1.0", FabricOnly,
							{ need("nothing-like-this"), breaks("sodium") })),
			switchedOff(mod("off-forge", "1.0", ForgeOnly)),
			mod("sodium", "0.5.3"),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())), QStringList());
	}

	void aFileWithoutLoaderMetadataIsOnlyAProvider()
	{
		// A plain library jar: no loaders, so nothing to read its needs
		// from - but it still has an id others can depend on.
		const QList<Entry> folder = {
			mod("library", "2.0", {}, { need("something-missing") }),
			mod("sodium", "0.5.3", FabricOnly, { need("library", ">=1.0") }),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())), QStringList());
	}

	void blockingFirstThenByNameThenFile()
	{
		const QList<Entry> folder = {
			named(mod("z", "1.0", FabricOnly, { conflicts("b") }), "z.jar", "Zeta"),
			named(mod("b", "1.0", FabricOnly, { need("gone") }), "b2.jar", "beta"),
			named(mod("a", "1.0", FabricOnly, { need("gone") }), "a.jar", "Alpha"),
			named(mod("c", "1.0", ForgeOnly), "c.jar", "alpha loader"),
			named(mod("d", "1.0", FabricOnly, { need("gone") }), "b1.jar", "beta"),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 (QStringList{ QStringLiteral("Missing a.jar gone"),
							   QStringLiteral("Missing b1.jar gone"),
							   QStringLiteral("Missing b2.jar gone"),
							   QStringLiteral("WrongLoader c.jar"),
							   QStringLiteral("Discouraged z.jar b") }));
	}

	void theSameThingIsReportedOnce()
	{
		const QList<Entry> folder = {
			mod("sodium", "0.5.3", FabricOnly,
				{ need("fabric-api", ">=0.90"), need("fabric-api", ">=0.91"),
				  breaks("lithium"), breaks("lithium") }),
			mod("indium", "1.0", FabricOnly, { need("fabric-api") }),
			mod("lithium", "1.0"),
		};
		QCOMPARE(summary(check(folder, fabricEnvironment())),
				 (QStringList{ QStringLiteral("Missing indium.jar fabric-api"),
							   QStringLiteral("Missing sodium.jar fabric-api"),
							   QStringLiteral("Incompatible sodium.jar lithium") }));
	}

	void neoForgeOneTwentyOneOneAlsoAnswersToOneTwentyOne()
	{
		// What the mods of ATM10 write: built for 1.21, asking for
		// [1.21,1.21.1) - and FML's version support matrix lets them load.
		const QStringList NeoForge = { QStringLiteral("neoforge") };
		const QList<Entry> folder = {
			mod("additional_lights", "2.1.10", NeoForge,
				{ needMaven("minecraft", "[1.21,1.21.1)", "neoforge") }),
			mod("spaced", "1.0", NeoForge,
				{ needMaven("minecraft", "[1.21, 1.21.1)", "neoforge") }),
			mod("exactly", "1.0", NeoForge,
				{ needMaven("minecraft", "[1.21]", "neoforge") }),
			mod("current", "1.0", NeoForge,
				{ needMaven("minecraft", "[1.21.1]", "neoforge") }),
			mod("older-neoforge", "1.0", NeoForge,
				{ needMaven("neoforge", "[21.0.100,21.1)", "neoforge") }),
		};
		QCOMPARE(summary(check(folder, neoForgeEnvironment("1.21.1"))),
				 QStringList());

		// A range neither 1.21.1 nor 1.21 is in is still wrong.
		const QList<Entry> tooOld = {
			mod("old", "1.0", NeoForge,
				{ needMaven("minecraft", "[1.20.5,1.21)", "neoforge"),
				  needMaven("neoforge", "[20.4,21.0)", "neoforge") }),
		};
		const QList<Issue> issues =
			check(tooOld, neoForgeEnvironment("1.21.1"));
		QCOMPARE(summary(issues),
				 (QStringList{
					 QStringLiteral("WrongMinecraft old.jar minecraft"),
					 QStringLiteral("WrongLoaderVersion old.jar neoforge") }));
		QCOMPARE(issues.at(0).foundVersion, QStringLiteral("1.21.1"));
	}

	void theSupportMatrixIsForTheGameVersionItNames()
	{
		const QStringList NeoForge = { QStringLiteral("neoforge") };
		const QList<Entry> folder = {
			mod("built-for-1.21", "1.0", NeoForge,
				{ needMaven("minecraft", "[1.21,1.21.1)", "neoforge") }),
		};
		const QStringList wrong = {
			QStringLiteral("WrongMinecraft built-for-1.21.jar minecraft") };

		// Another game version has no such entry...
		QCOMPARE(summary(check(folder, neoForgeEnvironment("1.21.4"))), wrong);
		QCOMPARE(summary(check(folder, neoForgeEnvironment("1.21.2"))), wrong);
		// ...and Forge does not take the NeoForge entry.
		QCOMPARE(summary(check({ mod("forge-built", "1.0", ForgeOnly,
									 { needMaven("minecraft", "[1.21,1.21.1)",
												 "forge") }) },
							   forgeEnvironment("1.21.1", "52.0.0"))),
				 QStringList{
					 QStringLiteral("WrongMinecraft forge-built.jar minecraft") });
	}

	void forgeOneTwentyAlsoAnswersToItsOldBuild()
	{
		const QList<Entry> folder = {
			mod("pinned", "1.0", ForgeOnly,
				{ needMaven("forge", "[47.1.0,47.1.80)") }),
		};
		QCOMPARE(summary(check(folder, forgeEnvironment("1.20.1", "47.4.8"))),
				 QStringList());

		// Not on another game version, where the loader says nothing of it.
		QCOMPARE(summary(check(folder, forgeEnvironment("1.20.4", "49.0.0"))),
				 QStringList{
					 QStringLiteral("WrongLoaderVersion pinned.jar forge") });
	}

	void theSupportMatrixIsNotUsedForClashes()
	{
		// A clash is only reported when the built-in is surely in range:
		// the extra versions do not add one.
		const QStringList NeoForge = { QStringLiteral("neoforge") };
		const ModDependency clash = declaration(
			ModDependency::Kind::Incompatible, ModDependency::Syntax::Maven,
			QStringLiteral("minecraft"), QStringLiteral("[1.21]"),
			QStringLiteral("neoforge"));
		QCOMPARE(summary(check({ mod("breaker", "1.0", NeoForge, { clash }) },
							   neoForgeEnvironment("1.21.1"))),
				 QStringList());
	}

	void connectorIsFoundThroughTheModsItProvides()
	{
		const QList<Entry> sodium = { mod("sodium", "0.5.3", FabricOnly) };
		const Environment forge = forgeEnvironment("1.20.1", "47.4.8");
		const QStringList wrong = { QStringLiteral("WrongLoader sodium.jar") };

		for (const QString& id :
			 { QStringLiteral("connectormod"), QStringLiteral("connector") }) {
			// The jar has no metadata of its own: the mod is nested.
			QList<Entry> viaProvides = sodium;
			viaProvides.append(wrapperJar("Connector.jar",
										  { provided(id, "1.0.0-beta.47") }));
			QCOMPARE(summary(check(viaProvides, forge)), QStringList());

			// Or it is the file's own id, in either spelling.
			QList<Entry> viaOwnId = sodium;
			viaOwnId.append(mod(id.toUpper(), "1.0.0", ForgeOnly));
			QCOMPARE(summary(check(viaOwnId, forge)), QStringList());

			QList<Entry> off = sodium;
			off.append(switchedOff(wrapperJar("Connector.jar",
											  { provided(id, "1.0.0") })));
			QCOMPARE(summary(check(off, forge)), wrong);
		}

		QCOMPARE(summary(check(sodium, forge)), wrong);
	}

	void connectorReadsFabricJarsWithDashesAsUnderscores()
	{
		const Environment forge = forgeEnvironment("1.20.1", "47.4.8");
		const Entry connector = wrapperJar(
			"Connector.jar", { provided("connectormod", "1.0.0-beta.47") });
		// The Forgified Fabric API answers to Forge's spelling.
		const QList<Entry> folder = {
			connector,
			mod("fabric_api", "0.92.6", ForgeOnly),
			mod("resistance_balancer", "1.0.0", FabricOnly,
				{ need("fabric-api"), need("fabricloader", ">=0.15.11"),
				  need("necronomicon", ">=1.4.2") }),
			mod("necronomicon", "1.6.0", ForgeOnly),
			mod("clasher", "1.0", FabricOnly, { breaks("some-mod") }),
			mod("some_mod", "2.0", ForgeOnly),
			switchedOff(mod("off_mod", "1.0", ForgeOnly)),
			mod("needs-off", "1.0", FabricOnly, { need("off-mod") }),
		};
		const QList<Issue> issues = check(folder, forge);
		QCOMPARE(summary(issues),
				 (QStringList{
					 QStringLiteral("Incompatible clasher.jar some-mod"),
					 QStringLiteral("Disabled needs-off.jar off-mod") }));
		QCOMPARE(issues.at(0).otherKey, QStringLiteral("some_mod.jar"));
		QCOMPARE(issues.at(1).otherKey, QStringLiteral("off_mod.jar"));
	}

	void dashesAndUnderscoresAreDifferentOutsideConnector()
	{
		// No Connector in the folder: a Forge file is read as Forge, and
		// nothing is folded.
		const Environment forge = forgeEnvironment("1.20.1", "47.4.8");
		const QList<Entry> forgeFile = {
			mod("fabric_api", "0.92.6", ForgeOnly),
			mod("asks", "1.0", ForgeOnly,
				{ needMaven("fabric-api", "[0,)") }),
		};
		QCOMPARE(summary(check(forgeFile, forge)),
				 QStringList{ QStringLiteral("Missing asks.jar fabric-api") });

		// Connector there, but this file is Forge's own, not Fabric's.
		QList<Entry> withConnector = forgeFile;
		withConnector.append(wrapperJar(
			"Connector.jar", { provided("connectormod", "1.0.0-beta.47") }));
		QCOMPARE(summary(check(withConnector, forge)),
				 QStringList{ QStringLiteral("Missing asks.jar fabric-api") });

		// On Fabric (no Connector, Fabric reads it itself) the ids differ.
		const QList<Entry> onFabric = {
			mod("fabric_api", "0.92.6", FabricOnly),
			mod("sodium", "0.5.3", FabricOnly, { need("fabric-api") }),
		};
		QCOMPARE(summary(check(onFabric, fabricEnvironment())),
				 QStringList{ QStringLiteral("Missing sodium.jar fabric-api") });

		// Nor does a Fabric instance with a Connector-named mod fold.
		QList<Entry> fabricConnector = onFabric;
		fabricConnector.append(mod("connector", "1.0.0", FabricOnly));
		QCOMPARE(summary(check(fabricConnector, fabricEnvironment())),
				 QStringList{ QStringLiteral("Missing sodium.jar fabric-api") });
	}

	void readableLoaders_data()
	{
		QTest::addColumn<QString>("minecraft");
		QTest::addColumn<QStringList>("loaders");
		QTest::addColumn<bool>("connector");
		QTest::addColumn<QString>("expected");

		const auto row = [](const char* name, const char* minecraft,
							const QStringList& loaders, bool connector,
							const char* expected) {
			QTest::newRow(name) << QString::fromUtf8(minecraft) << loaders
								<< connector << QString::fromUtf8(expected);
		};

		row("fabric", "1.20.1", { "fabric" }, false, "fabric");
		row("quilt also reads fabric", "1.20.1", { "quilt" }, false,
			"quilt,fabric");
		row("forge", "1.20.1", { "forge" }, false, "forge");
		row("neoforge before the cut", "1.20.4", { "neoforge" }, false,
			"neoforge,forge");
		row("neoforge at the cut", "1.20.5", { "neoforge" }, false, "neoforge");
		row("neoforge after the cut", "1.21", { "neoforge" }, false,
			"neoforge");
		row("neoforge on a snapshot", "24w14a", { "neoforge" }, false,
			"neoforge,forge");
		row("neoforge on nothing", "", { "neoforge" }, false, "neoforge,forge");
		row("forge with connector", "1.20.1", { "forge" }, true, "forge,fabric");
		row("neoforge with connector", "1.20.1", { "neoforge" }, true,
			"neoforge,forge,fabric");
		row("neoforge 1.21 with connector", "1.21", { "neoforge" }, true,
			"neoforge,fabric");
		row("fabric with connector", "1.20.1", { "fabric" }, true, "fabric");
		row("quilt with connector", "1.20.1", { "quilt" }, true,
			"quilt,fabric");
		row("own loaders first", "1.20.1", { "quilt", "forge" }, false,
			"quilt,forge,fabric");
		row("listed twice", "1.20.1", { "quilt", "fabric" }, false,
			"quilt,fabric");
		row("upper case", "1.20.1", { "Fabric" }, false, "fabric");
		row("no loader", "1.20.1", {}, false, "");
		row("no loader, connector", "1.20.1", {}, true, "");
	}

	void readableLoaders()
	{
		QFETCH(QString, minecraft);
		QFETCH(QStringList, loaders);
		QFETCH(bool, connector);
		QFETCH(QString, expected);

		QCOMPARE(readable(environment(minecraft, loaders, {}), connector),
				 expected);
	}
};

QTEST_GUILESS_MAIN(ModHealthTest)

#include "ModHealth_test.moc"
