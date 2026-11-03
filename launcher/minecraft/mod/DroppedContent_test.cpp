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

#include "GZip.h"
#include "MMCZip.h"
#include "minecraft/mod/DroppedContent.h"

using namespace DroppedContent;

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

	bool writeFile(const QString& path, const QByteArray& bytes)
	{
		QDir().mkpath(QFileInfo(path).path());
		QFile file(path);
		return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
	}

	QByteArray readFile(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	// A gzipped level.dat holding {"": {Data: {LevelName: "Test World"}}}.
	QByteArray levelDat()
	{
		QByteArray nbt;
		auto header = [&nbt](char type, const QByteArray& name) {
			nbt.append(type);
			nbt.append(char(name.size() >> 8));
			nbt.append(char(name.size() & 0xFF));
			nbt.append(name);
		};
		header(10, "");
		header(10, "Data");
		header(8, "LevelName");
		const QByteArray world = "Test World";
		nbt.append(char(world.size() >> 8));
		nbt.append(char(world.size() & 0xFF));
		nbt.append(world);
		nbt.append(char(0)); // end of Data
		nbt.append(char(0)); // end of root
		QByteArray packed;
		GZip::zip(nbt, packed);
		return packed;
	}

	const QByteArray kPackMcmeta = R"({"pack":{"pack_format":15,"description":"x"}})";

	QByteArray fabricMod(const QString& id, const QString& version,
						 const QString& minecraft = QString())
	{
		QString json = QStringLiteral(
						   R"({"schemaVersion":1,"id":"%1","name":"%1 mod","version":"%2")")
						   .arg(id, version);
		if (!minecraft.isEmpty()) {
			json += QStringLiteral(R"(,"depends":{"minecraft":"%1"})").arg(minecraft);
		}
		return (json + QStringLiteral("}")).toUtf8();
	}

	const QByteArray kForgeMods = R"toml(modLoader="javafml"
loaderVersion="[47,)"
license="MIT"
[[mods]]
modId="forgemod"
version="2.0.0"
displayName="Forge Mod"
)toml";
} // namespace

class DroppedContentTest : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	QString path(const QString& name) const
	{
		return m_dir.filePath(name);
	}

	Kind kindOf(const QString& name) const
	{
		return classify(path(name)).kind;
	}

  private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());

		QVERIFY(!writeZip(path("fabric.jar"),
						  {{"fabric.mod.json", fabricMod("fabricmod", "1.2.3")},
						   {"assets/fabricmod/icon.png", "x"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("forge.jar"),
						  {{"META-INF/mods.toml", kForgeMods},
						   {"pack.mcmeta", kPackMcmeta},
						   {"assets/forgemod/a.txt", "x"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("old.jar"), {{"com/example/Mod.class", "x"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("zipmod.zip"),
						  {{"fabric.mod.json", fabricMod("zipmod", "1.0")}})
					 .isEmpty());

		QVERIFY(!writeZip(path("resources.zip"),
						  {{"pack.mcmeta", kPackMcmeta},
						   {"assets/minecraft/textures/a.png", "x"}})
					 .isEmpty());
		// A resource pack with its own core shaders is still a resource pack.
		QVERIFY(!writeZip(path("resources-shaders.zip"),
						  {{"pack.mcmeta", kPackMcmeta},
						   {"assets/minecraft/shaders/core/a.fsh", "x"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("data.zip"),
						  {{"pack.mcmeta", kPackMcmeta},
						   {"data/test/functions/a.mcfunction", "say hi"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("shaders.zip"),
						  {{"shaders/final.fsh", "x"}, {"shaders/final.vsh", "x"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("shaders-wrapped.zip"),
						  {{"BSL/shaders/final.fsh", "x"}})
					 .isEmpty());

		QVERIFY(!writeZip(path("world.zip"),
						  {{"level.dat", levelDat()},
						   {"region/r.0.0.mca", "x"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("world-wrapped.zip"),
						  {{"My World/level.dat", levelDat()},
						   {"My World/datapacks/a.txt", "x"}})
					 .isEmpty());

		QVERIFY(!writeZip(path("pack.mrpack"),
						  {{"modrinth.index.json", "{}"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("curse.zip"),
						  {{"manifest.json",
							R"({"manifestType":"minecraftModpack"})"},
						   {"overrides/config/a.toml", "x"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("prism.zip"),
						  {{"instance.cfg", "name=x"}, {"mmc-pack.json", "{}"}})
					 .isEmpty());
		QVERIFY(!writeZip(path("stray-manifest.zip"),
						  {{"manifest.json", R"({"name":"not a pack"})"}})
					 .isEmpty());

		QVERIFY(!writeZip(path("random.zip"), {{"notes.txt", "hello"}})
					 .isEmpty());
		QVERIFY(writeFile(path("notes.txt"), "hello"));
		QVERIFY(writeFile(path("broken.zip"), "this is not a zip"));

		QVERIFY(writeFile(path("worlddir/level.dat"), levelDat()));
		QVERIFY(writeFile(path("worlddir/region/r.0.0.mca"), "x"));
		QVERIFY(writeFile(path("wrapperdir/Saved/level.dat"), levelDat()));
		QVERIFY(writeFile(path("packdir/pack.mcmeta"), kPackMcmeta));
		QVERIFY(writeFile(path("packdir/assets/minecraft/a.png"), "x"));
		QVERIFY(writeFile(path("moddir/fabric.mod.json"),
						  fabricMod("folder", "1.0")));
		QVERIFY(QDir().mkpath(path("emptydir")));
	}

	void modsAreRecognisedByTheirMetadata()
	{
		const Item fabric = classify(path("fabric.jar"));
		QCOMPARE(fabric.kind, Kind::Mod);
		QCOMPARE(fabric.modId, QStringLiteral("fabricmod"));
		QCOMPARE(fabric.name, QStringLiteral("fabricmod mod"));
		QCOMPARE(fabric.version, QStringLiteral("1.2.3"));
		QVERIFY(fabric.details.loaders.contains(QStringLiteral("fabric")));

		const Item forge = classify(path("forge.jar"));
		QCOMPARE(forge.kind, Kind::Mod); // not a resource pack, though it has one
		QCOMPARE(forge.modId, QStringLiteral("forgemod"));
		QVERIFY(forge.details.loaders.contains(QStringLiteral("forge")));

		QCOMPARE(kindOf("zipmod.zip"), Kind::Mod);
		QCOMPARE(kindOf("moddir"), Kind::Mod);
	}

	void aJarWithoutMetadataIsStillAMod()
	{
		const Item old = classify(path("old.jar"));
		QCOMPARE(old.kind, Kind::Mod);
		QVERIFY(!old.hasMetadata);
		QCOMPARE(old.name, QStringLiteral("old"));
	}

	void packsAreToldApartByTheirLayout()
	{
		QCOMPARE(kindOf("resources.zip"), Kind::ResourcePack);
		QCOMPARE(kindOf("resources-shaders.zip"), Kind::ResourcePack);
		QCOMPARE(kindOf("data.zip"), Kind::DataPack);
		QCOMPARE(kindOf("shaders.zip"), Kind::ShaderPack);
		QCOMPARE(kindOf("shaders-wrapped.zip"), Kind::ShaderPack);
		QCOMPARE(kindOf("packdir"), Kind::ResourcePack);
	}

	void worldsAreFoundInZipsAndFolders()
	{
		QCOMPARE(kindOf("world.zip"), Kind::World);
		QCOMPARE(kindOf("world-wrapped.zip"), Kind::World);
		QCOMPARE(kindOf("worlddir"), Kind::World);

		const Item wrapped = classify(path("wrapperdir"));
		QCOMPARE(wrapped.kind, Kind::World);
		QCOMPARE(QFileInfo(wrapped.path).fileName(), QStringLiteral("Saved"));
	}

	void modpacksGoToImport()
	{
		QCOMPARE(kindOf("pack.mrpack"), Kind::Modpack);
		QCOMPARE(kindOf("curse.zip"), Kind::Modpack);
		QCOMPARE(kindOf("prism.zip"), Kind::Modpack);
		QCOMPARE(kindOf("stray-manifest.zip"), Kind::Unknown);
	}

	void everythingElseIsUnknown()
	{
		QCOMPARE(kindOf("random.zip"), Kind::Unknown);
		QCOMPARE(kindOf("notes.txt"), Kind::Unknown);
		QCOMPARE(kindOf("broken.zip"), Kind::Unknown);
		QCOMPARE(kindOf("emptydir"), Kind::Unknown);
		QCOMPARE(kindOf("missing.jar"), Kind::Unknown);
	}

	void fileUrlsAreAccepted()
	{
		const Item item = classify(QUrl::fromLocalFile(path("fabric.jar")).toString());
		QCOMPARE(item.kind, Kind::Mod);
		QCOMPARE(item.fileName, QStringLiteral("fabric.jar"));
	}

	void kindsRouteToTheirFolders()
	{
		QCOMPARE(contentKind(Kind::Mod), QStringLiteral("mods"));
		QCOMPARE(contentKind(Kind::ResourcePack), QStringLiteral("resourcepacks"));
		QCOMPARE(contentKind(Kind::ShaderPack), QStringLiteral("shaderpacks"));
		QVERIFY(contentKind(Kind::DataPack).isEmpty()); // needs a world
		QVERIFY(contentKind(Kind::World).isEmpty());
		QVERIFY(contentKind(Kind::Modpack).isEmpty());
		QVERIFY(contentKind(Kind::Unknown).isEmpty());
	}

	void summariesCountEachKind()
	{
		Counts none;
		QVERIFY(describe(none).isEmpty());

		Counts counts;
		counts.add(Kind::Mod);
		QCOMPARE(describe(counts), QStringLiteral("1 mod"));
		counts.add(Kind::Mod);
		counts.add(Kind::Mod);
		counts.add(Kind::ShaderPack);
		counts.add(Kind::Unknown);
		QCOMPARE(counts.total(), 4);
		QCOMPARE(describe(counts), QStringLiteral("3 mods and 1 shader pack"));
		counts.add(Kind::ResourcePack);
		counts.add(Kind::ResourcePack);
		QCOMPARE(describe(counts),
				 QStringLiteral("3 mods, 2 resource packs and 1 shader pack"));
	}

	void anIdenticalFileIsFoundByContent()
	{
		QVERIFY(writeFile(path("target/renamed.jar"), readFile(path("fabric.jar"))));
		QVERIFY(writeFile(path("target/other.jar"), "different"));
		QCOMPARE(identicalFileIn(path("target"), classify(path("fabric.jar"))),
				 QStringLiteral("renamed.jar"));
		QVERIFY(identicalFileIn(path("target"), classify(path("old.jar"))).isEmpty());
		// A folder is never a duplicate.
		QVERIFY(identicalFileIn(path("target"), classify(path("moddir"))).isEmpty());
	}

	void theSameModIdIsFoundInAnotherVersion()
	{
		QVERIFY(QDir().mkpath(path("mods")));
		QVERIFY(!writeZip(path("mods/fabricmod-1.0.jar"),
						  {{"fabric.mod.json", fabricMod("FabricMod", "1.0")}})
					 .isEmpty());
		QVERIFY(!writeZip(path("mods/other-3.jar"),
						  {{"fabric.mod.json", fabricMod("other", "3.0")}})
					 .isEmpty());
		QVERIFY(!writeZip(path("mods/off.jar.disabled"),
						  {{"fabric.mod.json", fabricMod("hidden", "1")}})
					 .isEmpty());
		QVERIFY(writeFile(path("mods/readme.txt"), "x"));

		QCOMPARE(sameModIn(path("mods"), classify(path("fabric.jar"))),
				 QStringLiteral("fabricmod-1.0.jar"));
		QVERIFY(sameModIn(path("mods"), classify(path("zipmod.zip"))).isEmpty());
		QVERIFY(sameModIn(path("mods"), classify(path("old.jar"))).isEmpty());

		const QString hidden = path("hidden.jar");
		QVERIFY(!writeZip(hidden, {{"fabric.mod.json", fabricMod("hidden", "2")}})
					 .isEmpty());
		QCOMPARE(sameModIn(path("mods"), classify(hidden)),
				 QStringLiteral("off.jar.disabled"));
	}

	void aModForAnotherLoaderOrVersionIsFlagged()
	{
		ModHealth::Environment fabric;
		fabric.minecraft = QStringLiteral("1.20.1");
		fabric.loaders = {QStringLiteral("fabric")};
		fabric.builtIns.insert(QStringLiteral("minecraft"), fabric.minecraft);

		QVERIFY(compatibilityProblem(classify(path("fabric.jar")), fabric).isEmpty());
		QVERIFY(compatibilityProblem(classify(path("old.jar")), fabric).isEmpty());

		const QString forge = compatibilityProblem(classify(path("forge.jar")), fabric);
		QVERIFY2(forge.contains(QStringLiteral("Forge")), qPrintable(forge));
		QVERIFY2(forge.contains(QStringLiteral("Fabric")), qPrintable(forge));

		QVERIFY(!writeZip(path("future.jar"),
						  {{"fabric.mod.json", fabricMod("future", "1", ">=1.21")}})
					 .isEmpty());
		const QString future = compatibilityProblem(classify(path("future.jar")), fabric);
		QVERIFY2(future.contains(QStringLiteral("1.20.1")), qPrintable(future));

		// No loader on the instance: nothing to compare with.
		ModHealth::Environment vanilla;
		vanilla.minecraft = fabric.minecraft;
		QVERIFY(compatibilityProblem(classify(path("forge.jar")), vanilla).isEmpty());
	}

	void aMixedDropIsSortedIntoTheRightFolders()
	{
		Target target;
		target.modsDir = path("inst/mods");
		target.resourcePacksDir = path("inst/resourcepacks");
		target.shaderPacksDir = path("inst/shaderpacks");

		const Plan result = plan(
			{QUrl::fromLocalFile(path("fabric.jar")).toString(),
			 path("resources.zip"), path("shaders.zip"), path("data.zip"),
			 path("world.zip"), path("pack.mrpack"), path("notes.txt"),
			 QStringLiteral("https://modrinth.com/mod/sodium"),
			 QStringLiteral("https://example.com/thing.zip")},
			target);

		QCOMPARE(result.entries.size(), 5);
		QCOMPARE(result.entries.at(0).item.kind, Kind::Mod);
		QCOMPARE(result.entries.at(0).dir, target.modsDir);
		QCOMPARE(result.entries.at(1).item.kind, Kind::ResourcePack);
		QCOMPARE(result.entries.at(1).dir, target.resourcePacksDir);
		QCOMPARE(result.entries.at(2).item.kind, Kind::ShaderPack);
		QCOMPARE(result.entries.at(2).dir, target.shaderPacksDir);
		// A data pack waits for a world, a world has its own folder.
		QCOMPARE(result.entries.at(3).item.kind, Kind::DataPack);
		QVERIFY(result.entries.at(3).dir.isEmpty());
		QCOMPARE(result.entries.at(4).item.kind, Kind::World);
		QVERIFY(result.entries.at(4).dir.isEmpty());

		QCOMPARE(result.modpacks, QStringList{path("pack.mrpack")});
		QCOMPARE(result.unknown, QStringList{QStringLiteral("notes.txt")});
		QCOMPARE(result.projectLinks,
				 QStringList{QStringLiteral("https://modrinth.com/mod/sodium")});
		QCOMPARE(result.otherLinks,
				 QStringList{QStringLiteral("https://example.com/thing.zip")});
		QCOMPARE(describe(result.counts),
				 QStringLiteral("1 mod, 1 resource pack, 1 data pack, 1 shader "
								"pack, 1 world and 1 modpack"));
	}

	void aDropMeetsWhatIsAlreadyInTheInstance()
	{
		Target target;
		target.modsDir = path("meet/mods");
		target.shaderPacksDir = path("meet/shaderpacks");
		target.environment.minecraft = QStringLiteral("1.20.1");
		target.environment.loaders = {QStringLiteral("fabric")};
		target.environment.builtIns.insert(QStringLiteral("minecraft"),
										   target.environment.minecraft);

		// The shader pack is already there under another name; an older fabricmod
		// is installed; forge.jar is for another loader.
		QVERIFY(QDir().mkpath(target.modsDir));
		QVERIFY(QDir().mkpath(target.shaderPacksDir));
		QVERIFY(writeFile(path("meet/shaderpacks/Old Name.zip"),
						  readFile(path("shaders.zip"))));
		QVERIFY(!writeZip(path("meet/mods/fabricmod-0.9.jar"),
						  {{"fabric.mod.json", fabricMod("fabricmod", "0.9")}})
					 .isEmpty());

		const Plan result = plan({path("fabric.jar"), path("shaders.zip"),
								  path("forge.jar")},
								 target);
		QCOMPARE(result.entries.size(), 3);

		const Entry& mod = result.entries.at(0);
		QCOMPARE(mod.sameMod, QStringLiteral("fabricmod-0.9.jar"));
		QVERIFY(mod.identical.isEmpty());
		QVERIFY(mod.warning.isEmpty());

		const Entry& shader = result.entries.at(1);
		QCOMPARE(shader.identical, QStringLiteral("Old Name.zip"));

		const Entry& forge = result.entries.at(2);
		QVERIFY(forge.sameMod.isEmpty());
		QVERIFY2(forge.warning.contains(QStringLiteral("Forge")),
				 qPrintable(forge.warning));
	}

	void projectLinksAreParsed_data()
	{
		QTest::addColumn<QString>("url");
		QTest::addColumn<QString>("provider");
		QTest::addColumn<QString>("type");
		QTest::addColumn<QString>("slug");

		QTest::newRow("modrinth mod")
			<< "https://modrinth.com/mod/sodium" << "modrinth" << "mods" << "sodium";
		QTest::newRow("modrinth version page")
			<< "https://www.modrinth.com/mod/sodium/version/mc1.20.1-0.5.3"
			<< "modrinth" << "mods" << "sodium";
		QTest::newRow("modrinth resource pack")
			<< "https://modrinth.com/resourcepack/faithful-32x" << "modrinth"
			<< "resourcepacks" << "faithful-32x";
		QTest::newRow("modrinth shader")
			<< "https://modrinth.com/shader/complementary-reimagined"
			<< "modrinth" << "shaderpacks" << "complementary-reimagined";
		QTest::newRow("modrinth data pack")
			<< "https://modrinth.com/datapack/terralith" << "modrinth"
			<< "datapacks" << "terralith";
		QTest::newRow("curseforge mod")
			<< "https://www.curseforge.com/minecraft/mc-mods/jei" << "curseforge"
			<< "mods" << "jei";
		QTest::newRow("curseforge file page")
			<< "https://www.curseforge.com/minecraft/mc-mods/jei/files/123"
			<< "curseforge" << "mods" << "jei";
		QTest::newRow("curseforge texture pack")
			<< "https://www.curseforge.com/minecraft/texture-packs/faithful"
			<< "curseforge" << "resourcepacks" << "faithful";
		QTest::newRow("curseforge shader")
			<< "https://www.curseforge.com/minecraft/shaders/bsl-shaders"
			<< "curseforge" << "shaderpacks" << "bsl-shaders";
		QTest::newRow("curseforge data pack")
			<< "https://www.curseforge.com/minecraft/data-packs/terralith"
			<< "curseforge" << "datapacks" << "terralith";
	}
	void projectLinksAreParsed()
	{
		QFETCH(QString, url);
		QFETCH(QString, provider);
		QFETCH(QString, type);
		QFETCH(QString, slug);
		const ProjectLink link = parseProjectLink(QUrl(url));
		QVERIFY(link.valid());
		QCOMPARE(link.provider, provider);
		QCOMPARE(link.contentType, type);
		QCOMPARE(link.slug, slug);
	}

	void otherLinksAreNotProjects_data()
	{
		QTest::addColumn<QString>("url");
		QTest::newRow("modpack") << "https://modrinth.com/modpack/fabulously-optimized";
		QTest::newRow("curseforge modpack")
			<< "https://www.curseforge.com/minecraft/modpacks/atm10";
		QTest::newRow("modrinth home") << "https://modrinth.com/";
		QTest::newRow("user page") << "https://modrinth.com/user/jellysquid3";
		QTest::newRow("other site") << "https://example.com/mod/sodium";
		QTest::newRow("curseforge other game")
			<< "https://www.curseforge.com/wow/addons/deadly-boss-mods";
		QTest::newRow("file") << "file:///tmp/mod/sodium";
		QTest::newRow("lookalike host") << "https://notmodrinth.com/mod/sodium";
	}
	void otherLinksAreNotProjects()
	{
		QFETCH(QString, url);
		QVERIFY(!parseProjectLink(QUrl(url)).valid());
	}

	void theSentenceSaysWhatHappened()
	{
		QString tone;
		Outcome outcome;
		outcome.added.add(Kind::Mod);
		outcome.added.add(Kind::Mod);
		outcome.added.add(Kind::Mod);
		outcome.added.add(Kind::ShaderPack);
		QCOMPARE(summarize(outcome, &tone),
				 QStringLiteral("Added 3 mods and 1 shader pack."));
		QCOMPARE(tone, QStringLiteral("success"));

		outcome.warnings << QStringLiteral("“A” is built for Forge, not Fabric.");
		QVERIFY(summarize(outcome, &tone).contains(QStringLiteral("built for Forge")));
		QCOMPARE(tone, QStringLiteral("warning"));

		outcome.warnings.clear();
		outcome.replaced = 1;
		outcome.unknown << QStringLiteral("notes.txt");
		const QString text = summarize(outcome, &tone);
		QVERIFY(text.contains(QStringLiteral("Replaced 1 older version.")));
		QVERIFY(text.contains(QStringLiteral("Skipped “notes.txt”")));
		QCOMPARE(tone, QStringLiteral("warning"));
	}

	void aDropThatDidNothingStillAnswers()
	{
		QString tone;
		Outcome identical;
		identical.identical = 2;
		QVERIFY(summarize(identical, &tone).contains(QStringLiteral("already here")));
		QCOMPARE(tone, QStringLiteral("neutral"));

		Outcome skipped;
		skipped.unknown << QStringLiteral("a.txt") << QStringLiteral("b.png");
		QCOMPARE(summarize(skipped, &tone),
				 QStringLiteral("Skipped 2 files that are not mods, packs or worlds."));
		QCOMPARE(tone, QStringLiteral("danger"));

		Outcome locked;
		locked.locked << QStringLiteral("a.jar");
		QVERIFY(summarize(locked, &tone).contains(QStringLiteral("game is running")));
		QCOMPARE(tone, QStringLiteral("danger"));

		QVERIFY(!summarize(Outcome(), &tone).isEmpty());
	}
};

QTEST_GUILESS_MAIN(DroppedContentTest)

#include "DroppedContent_test.moc"
