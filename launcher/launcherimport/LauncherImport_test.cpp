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

#include <QCryptographicHash>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "launcherimport/ImportBuild.h"
#include "launcherimport/ImportCopy.h"
#include "launcherimport/LauncherScan.h"
#include "launcherimport/SafeFiles.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "settings/INIFile.h"

using namespace LauncherImport;

/* Every format's fixture (testdata/, synthetic) is copied into a temporary
 * folder first, so a test may add links or damage a file without touching the
 * tree, and so the copies the fixtures are compared against stay put. */
class LauncherImportTest : public QObject
{
	Q_OBJECT

	QTemporaryDir m_tmp;

	static QString fixture(const QString& name)
	{
		return QFileInfo(QFINDTESTDATA("testdata/" + name)).absoluteFilePath();
	}

	static void copyTree(const QString& from, const QString& to)
	{
		QDir().mkpath(to);
		QDirIterator it(from, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
						QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			const QString target = to + "/" + QDir(from).relativeFilePath(it.filePath());
			if (it.fileInfo().isDir()) {
				QDir().mkpath(target);
			} else {
				QDir().mkpath(QFileInfo(target).absolutePath());
				QVERIFY(QFile::copy(it.filePath(), target));
			}
		}
	}

	/* A fixture copied to <tmp>/<where>; its path. */
	QString place(const QString& name, const QString& where)
	{
		const QString target = m_tmp.filePath(where);
		copyTree(fixture(name), target);
		return target;
	}

	/* Everything under @p root as it is on disk, links not followed: what a
	 * source must still look like after an import. */
	static QMap<QString, QString> snapshot(const QString& root)
	{
		QMap<QString, QString> state;
		QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
						QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			const QFileInfo info = it.fileInfo();
			const QString rel = QDir(root).relativeFilePath(it.filePath());
			if (info.isSymLink()) {
				state.insert(rel, "link:" + info.symLinkTarget());
			} else if (info.isDir()) {
				state.insert(rel, "dir");
			} else {
				QFile file(it.filePath());
				if (!file.open(QIODevice::ReadOnly)) {
					state.insert(rel, "unreadable");
					continue;
				}
				state.insert(rel, QString("%1:%2:%3")
									  .arg(info.size())
									  .arg(info.lastModified().toMSecsSinceEpoch())
									  .arg(QString(QCryptographicHash::hash(file.readAll(),
																			QCryptographicHash::Sha1)
													   .toHex())));
			}
		}
		return state;
	}

	static const FoundInstance* named(const FoundLauncher& launcher, const QString& name)
	{
		for (const FoundInstance& instance : launcher.instances) {
			if (instance.name == name) {
				return &instance;
			}
		}
		return nullptr;
	}

	static QJsonObject readJson(const QString& path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return {};
		}
		return QJsonDocument::fromJson(file.readAll()).object();
	}

	static INIFile readIni(const QString& path)
	{
		INIFile ini;
		ini.loadFile(path);
		return ini;
	}

	BuildResult build(const FoundInstance& instance, Mode mode, const QString& staging)
	{
		BuildRequest request;
		request.instance = instance;
		request.mode = mode;
		request.name = instance.name;
		request.iconKey = QStringLiteral("default");
		std::atomic_bool cancel{false};
		return buildInstance(request, staging, cancel);
	}

  private slots:
	void initTestCase()
	{
		QVERIFY(m_tmp.isValid());
	}

	void standardFoldersPerSystem()
	{
		using System = SearchEnvironment::System;
		const auto mac = SearchEnvironment::underHome("/Users/u", System::MacOS);
		QCOMPARE(candidateDirs(Source::Prism, mac).value(0),
				 QString("/Users/u/Library/Application Support/PrismLauncher"));
		QCOMPARE(candidateDirs(Source::ModrinthApp, mac).value(0),
				 QString("/Users/u/Library/Application Support/ModrinthApp"));
		QCOMPARE(candidateDirs(Source::Official, mac).value(0),
				 QString("/Users/u/Library/Application Support/minecraft"));
		QVERIFY(candidateDirs(Source::CurseForge, mac)
					.contains("/Users/u/Documents/curseforge/minecraft/Instances"));

		const auto windows = SearchEnvironment::underHome("C:/Users/u", System::Windows);
		QCOMPARE(candidateDirs(Source::Prism, windows).value(0),
				 QString("C:/Users/u/AppData/Roaming/PrismLauncher"));
		QCOMPARE(candidateDirs(Source::Official, windows).value(0),
				 QString("C:/Users/u/AppData/Roaming/.minecraft"));
		QCOMPARE(candidateDirs(Source::FTBApp, windows).value(0),
				 QString("C:/Users/u/AppData/Local/.ftba"));
		QVERIFY(candidateDirs(Source::CurseForge, windows)
					.contains("C:/Users/u/curseforge/minecraft/Instances"));
		// MultiMC is portable on Windows: no standard folder.
		QVERIFY(candidateDirs(Source::MultiMC, windows).isEmpty());

		const auto linuxEnv = SearchEnvironment::underHome("/home/u", System::Linux);
		QCOMPARE(candidateDirs(Source::Prism, linuxEnv),
				 QStringList({"/home/u/.local/share/PrismLauncher",
							  "/home/u/.var/app/org.prismlauncher.PrismLauncher/data/PrismLauncher"}));
		QCOMPARE(candidateDirs(Source::Official, linuxEnv).value(0), QString("/home/u/.minecraft"));
	}

	void scanFindsEveryLauncherUnderAHome()
	{
		const QString home = m_tmp.filePath("home-mac");
		const QString support = home + "/Library/Application Support/";
		copyTree(fixture("prism"), support + "PrismLauncher");
		copyTree(fixture("modrinth"), support + "ModrinthApp");
		copyTree(fixture("curseforge/Instances"), home + "/Documents/curseforge/minecraft/Instances");
		copyTree(fixture("official"), support + "minecraft");
		QDir().mkpath(support + "ATLauncher/instances/Some Pack");

		const auto found = scanAll(SearchEnvironment::underHome(home, SearchEnvironment::System::MacOS));
		QStringList sources;
		for (const FoundLauncher& launcher : found) {
			sources << sourceKey(launcher.source);
		}
		QCOMPARE(sources, QStringList({"prism", "modrinth", "curseforge", "official", "atlauncher"}));
		QVERIFY(!sourceSupported(Source::ATLauncher));
		QVERIFY(found.last().instances.isEmpty());
	}

	void prismInstanceIsRead()
	{
		const QString dir = place("prism", "prism-read");
		const FoundLauncher launcher = scanMultiMc(Source::Prism, dir);
		QCOMPARE(launcher.instances.size(), 4); // not _LAUNCHER_TEMP
		const FoundInstance* pack = named(launcher, "Fabric Pack, Remix");
		QVERIFY(pack);
		QVERIFY(pack->importable());
		QCOMPARE(pack->minecraftVersion, QString("1.20.1"));
		QCOMPARE(pack->loader, QString("fabric"));
		QCOMPARE(pack->loaderVersion, QString("0.15.11"));
		QCOMPARE(pack->iconKey, QString("grass"));
		QCOMPARE(pack->lastPlayed.toMSecsSinceEpoch(), 1730000000000LL);
		QVERIFY(pack->gameDir.endsWith("/.minecraft"));
		QVERIFY(pack->linkable());
		QVERIFY(pack->leftBehind.isEmpty());
	}

	void prismBrokenAndOldInstancesAreListedNotOffered()
	{
		const FoundLauncher launcher = scanMultiMc(Source::Prism, place("prism", "prism-broken"));
		const FoundInstance* broken = named(launcher, "Broken");
		QVERIFY(broken);
		QVERIFY(!broken->importable());
		QCOMPARE(broken->problem, QString("Its details file is damaged."));
		const FoundInstance* old = named(launcher, "Old");
		QVERIFY(old);
		QVERIFY(!old->importable());
	}

	void prismCustomComponentsAreLeftBehind()
	{
		const FoundLauncher launcher = scanMultiMc(Source::Prism, place("prism", "prism-custom"));
		const FoundInstance* custom = named(launcher, "Custom");
		QVERIFY(custom);
		QVERIFY(custom->importable());
		QCOMPARE(custom->loader, QString("forge"));
		QCOMPARE(custom->loaderVersion, QString("14.23.5.2860"));
		QCOMPARE(custom->leftBehind, QStringList({"custom components", "jar mods"}));
		QVERIFY(custom->gameDir.endsWith("/minecraft"));
	}

	void prismConfiguredInstanceFolderIsFollowed()
	{
		const QString dir = place("prism", "prism-moved");
		QVERIFY(QDir(dir).rename("instances", "elsewhere"));
		QFile cfg(dir + "/prismlauncher.cfg");
		QVERIFY(cfg.open(QIODevice::WriteOnly | QIODevice::Truncate));
		cfg.write("InstanceDir=elsewhere\n");
		cfg.close();
		QCOMPARE(scanMultiMc(Source::Prism, dir).instances.size(), 4);
	}

	void modrinthDatabaseProfiles()
	{
		const FoundLauncher launcher = scanModrinthApp(place("modrinth", "modrinth-db"));
		QVERIFY2(launcher.problem.isEmpty(), qPrintable(launcher.problem));
		// "../escape" names no folder under profiles/ and is not listed.
		QCOMPARE(launcher.instances.size(), 3);
		const FoundInstance* fabric = named(launcher, "Fabric Profile");
		QVERIFY(fabric);
		QVERIFY(fabric->importable());
		QCOMPARE(fabric->minecraftVersion, QString("1.20.1"));
		QCOMPARE(fabric->loader, QString("fabric"));
		QCOMPARE(fabric->loaderVersion, QString("0.15.11"));
		QCOMPARE(fabric->lastPlayed.toSecsSinceEpoch(), 1730000000LL);
		QVERIFY(!fabric->modrinthDatabase.isEmpty());
		const FoundInstance* forge = named(launcher, "Forge Profile");
		QVERIFY(forge);
		QCOMPARE(forge->loaderVersion, QString("47.2.0"));
		const FoundInstance* installing = named(launcher, "Installing");
		QVERIFY(installing);
		QVERIFY(!installing->importable());
	}

	void modrinthUncheckpointedProfileIsSeen()
	{
		const FoundLauncher launcher = scanModrinthApp(place("modrinth-wal", "modrinth-wal"));
		QVERIFY(named(launcher, "WAL Profile"));
		QCOMPARE(named(launcher, "WAL Profile")->loader, QString());
	}

	void modrinthOldProfileJson()
	{
		const FoundLauncher launcher = scanModrinthApp(fixture("modrinth-legacy"));
		QCOMPARE(launcher.instances.size(), 1);
		const FoundInstance& profile = launcher.instances.first();
		QCOMPARE(profile.loader, QString("quilt"));
		QCOMPARE(profile.loaderVersion, QString("0.17.6"));
		QStringList paths;
		for (const ModOrigin& mod : profile.mods) {
			paths << mod.relativePath;
		}
		paths.sort();
		// The "../../escape.jar" key is dropped, the backslash one kept.
		QCOMPARE(paths, QStringList({"mods/lithium.jar", "mods/sodium.jar"}));
	}

	void curseForgeInstances()
	{
		const FoundLauncher launcher = scanCurseForge(place("curseforge/Instances", "cf-read"));
		QCOMPARE(launcher.instances.size(), 3); // not "Not An Instance"
		const FoundInstance* atm = named(launcher, "All the Mods (synthetic)");
		QVERIFY(atm);
		QCOMPARE(atm->loader, QString("forge"));
		QCOMPARE(atm->loaderVersion, QString("47.2.0"));
		QStringList paths;
		for (const ModOrigin& mod : atm->mods) {
			paths << mod.relativePath + "=" + mod.projectId;
		}
		// Traversing and absolute file names are dropped, a missing file too.
		QCOMPARE(paths, QStringList({"mods/jei-forge.jar=238222", "mods/disabled-mod.jar=999001",
									 "resourcepacks/pack.zip=999002"}));
		const FoundInstance* fabric = named(launcher, "Fabric Vanilla+");
		QVERIFY(fabric);
		QCOMPARE(fabric->loaderVersion, QString("0.16.5"));
		QVERIFY(!fabric->lastPlayed.isValid()); // year 1 is "never"
		const FoundInstance* broken = named(launcher, "Broken");
		QVERIFY(broken);
		QCOMPARE(broken->problem, QString("Its details file is damaged."));
	}

	void curseForgeLargeManifest()
	{
		const QString dir = m_tmp.filePath("cf-large/Big Pack");
		QDir().mkpath(dir);
		QJsonArray addons;
		for (int i = 0; i < 5000; ++i) {
			addons.append(QJsonObject{{"addonID", 100000 + i},
									  {"name", QString("Synthetic addon %1").arg(i)},
									  {"installedFile", QJsonObject{{"id", 200000 + i},
																	{"fileName", QString("addon-%1.jar").arg(i)}}}});
		}
		QJsonObject manifest{{"name", "Big Pack"}, {"gameVersion", "1.20.1"}, {"installedAddons", addons}};
		QFile file(dir + "/minecraftinstance.json");
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(QJsonDocument(manifest).toJson());
		file.close();
		const FoundLauncher fits = scanCurseForge(m_tmp.filePath("cf-large"));
		QCOMPARE(fits.instances.size(), 1);
		QVERIFY(fits.instances.first().importable());

		// One byte past the cap is refused, not read in part.
		QVERIFY(file.open(QIODevice::Append));
		file.write(QByteArray(kMaxManifestBytes, ' '));
		file.close();
		const FoundLauncher tooLarge = scanCurseForge(m_tmp.filePath("cf-large"));
		QCOMPARE(tooLarge.instances.first().problem, QString("Its details file is too large to read."));
	}

	void officialProfiles()
	{
		const QString dir = place("official", "official-read");
		const FoundLauncher launcher = scanOfficial(dir);
		QCOMPARE(launcher.instances.size(), 8);
		const FoundInstance* latest = named(launcher, "Latest release");
		QVERIFY(latest);
		QCOMPARE(latest->minecraftVersion, QString("1.21.1"));
		QCOMPARE(latest->gameDir, dir);
		const FoundInstance* fabric = named(launcher, "Fabric 1.21.1");
		QVERIFY(fabric);
		QCOMPARE(fabric->loader, QString("fabric"));
		QCOMPARE(fabric->loaderVersion, QString("0.16.5"));
		QCOMPARE(fabric->minecraftVersion, QString("1.21.1"));
		const FoundInstance* forge = named(launcher, "Forge");
		QCOMPARE(forge->minecraftVersion, QString("1.20.1"));
		QCOMPARE(forge->loaderVersion, QString("47.2.0"));
		const FoundInstance* neoforge = named(launcher, "NeoForge");
		QCOMPARE(neoforge->minecraftVersion, QString("1.21.1"));
		QCOMPARE(neoforge->loaderVersion, QString("21.1.77"));
		QVERIFY(!named(launcher, "Traversal")->importable());
		QVERIFY(!named(launcher, "Relative folder")->importable());
		QVERIFY(!named(launcher, "Gone folder")->importable());
	}

	void copyTakesOnlyThePlayersFiles()
	{
		const QString dir = place("official", "official-copy");
		const FoundInstance latest = *named(scanOfficial(dir), "Latest release");
		const auto before = snapshot(dir);
		const QString staging = m_tmp.filePath("staging-official");
		const BuildResult result = build(latest, Mode::Copy, staging);
		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(snapshot(dir), before);

		QStringList copied = snapshot(staging + "/minecraft").keys();
		QCOMPARE(copied, QStringList({"mods", "mods/fabric-api.jar", "options.txt", "saves", "saves/Survival",
									  "saves/Survival/level.dat", "servers.dat"}));
		// The launcher's sign-in, logs, libraries and assets stay where they are.
		QVERIFY(!QFile::exists(staging + "/minecraft/launcher_accounts.json"));
		QVERIFY(!QFile::exists(staging + "/minecraft/launcher_msa_credentials.bin"));

		const INIFile cfg = readIni(staging + "/instance.cfg");
		QCOMPARE(cfg.get("InstanceType", "").toString(), QString("OneSix"));
		QCOMPARE(cfg.get("name", "").toString(), QString("Latest release"));
		QCOMPARE(cfg.get(kImportedFromKey, "").toString(), latest.key);
		QVERIFY(!cfg.contains(kLinkedGameDirKey));
		const QJsonArray components = readJson(staging + "/mmc-pack.json").value("components").toArray();
		QCOMPARE(components.size(), 1);
		QCOMPARE(components.at(0).toObject().value("version").toString(), QString("1.21.1"));
	}

	void copyKeepsPrismsOwnModIndex()
	{
		const QString dir = place("prism", "prism-copy");
		const FoundInstance pack = *named(scanMultiMc(Source::Prism, dir), "Fabric Pack, Remix");
		const auto before = snapshot(dir);
		const QString staging = m_tmp.filePath("staging-prism");
		const BuildResult result = build(pack, Mode::Copy, staging);
		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(snapshot(dir), before);
		QVERIFY(QFile::exists(staging + "/minecraft/mods/.index/sodium.pw.toml"));
		QVERIFY(QFile::exists(staging + "/minecraft/saves/Island/region/r.0.0.mca"));
		QVERIFY(QFile::exists(staging + "/minecraft/resourcepacks/Faithful.zip"));
		QVERIFY(!QFile::exists(staging + "/minecraft/logs"));
		QVERIFY(!QFile::exists(staging + "/minecraft/crash-reports"));
		const QJsonArray components = readJson(staging + "/mmc-pack.json").value("components").toArray();
		QCOMPARE(components.size(), 2);
		QCOMPARE(components.at(1).toObject().value("uid").toString(), QString("net.fabricmc.fabric-loader"));
		QCOMPARE(components.at(1).toObject().value("version").toString(), QString("0.15.11"));
		ModMetadataIndex index(QDir(staging + "/minecraft/mods"));
		index.load();
		QCOMPARE(index.get("sodium.jar").projectId, QString("AANobbMI"));
	}

	void copyWritesCurseForgeIds()
	{
		const QString dir = place("curseforge/Instances", "cf-copy");
		const FoundInstance atm = *named(scanCurseForge(dir), "All the Mods (synthetic)");
		const auto before = snapshot(dir);
		const QString staging = m_tmp.filePath("staging-cf");
		const BuildResult result = build(atm, Mode::Copy, staging);
		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(snapshot(dir), before);
		QCOMPARE(result.mappedMods, 3);
		QVERIFY(QFile::exists(staging + "/minecraft/kubejs/server_scripts/recipes.js"));
		QVERIFY(!QFile::exists(staging + "/minecraft/minecraftinstance.json"));
		ModMetadataIndex mods(QDir(staging + "/minecraft/mods"));
		mods.load();
		QCOMPARE(mods.get("jei-forge.jar").platform, QString("curseforge"));
		QCOMPARE(mods.get("jei-forge.jar").projectId, QString("238222"));
		QCOMPARE(mods.get("jei-forge.jar").versionId, QString("4712866"));
		QCOMPARE(mods.get("disabled-mod.jar").projectId, QString("999001"));
	}

	void copyWritesModrinthIdsFromItsCache()
	{
		const QString dir = place("modrinth", "modrinth-copy");
		const FoundInstance fabric = *named(scanModrinthApp(dir), "Fabric Profile");
		const auto before = snapshot(dir);
		const QString staging = m_tmp.filePath("staging-modrinth");
		const BuildResult result = build(fabric, Mode::Copy, staging);
		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(snapshot(dir), before);
		QCOMPARE(result.mappedMods, 2);
		ModMetadataIndex mods(QDir(staging + "/minecraft/mods"));
		mods.load();
		QCOMPARE(mods.get("sodium.jar").projectId, QString("AANobbMI"));
		QCOMPARE(mods.get("sodium.jar").versionId, QString("OihdIimA"));
		QCOMPARE(mods.get("iris.jar").projectId, QString("YL57xq9U"));
		QVERIFY(!mods.get("unknown.jar").hasPlatformOrigin());
		QVERIFY(QFile::exists(staging + "/minecraft/shaderpacks/BSL.zip"));
	}

	void linkPointsAtTheSourceFolder()
	{
		const QString dir = place("prism", "prism-link");
		const FoundInstance pack = *named(scanMultiMc(Source::Prism, dir), "Fabric Pack, Remix");
		const auto before = snapshot(dir);
		const QString staging = m_tmp.filePath("staging-link");
		const BuildResult result = build(pack, Mode::Link, staging);
		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(snapshot(dir), before);
		QVERIFY(!QFile::exists(staging + "/minecraft"));
		const QString linked = readIni(staging + "/instance.cfg").get(kLinkedGameDirKey, "").toString();
		QCOMPARE(linked, QFileInfo(pack.gameDir).canonicalFilePath());
		QCOMPARE(MinecraftInstance::gameRootFor(staging, linked), linked);
		// A link whose folder is gone falls back to the instance's own.
		QCOMPARE(MinecraftInstance::gameRootFor(staging, staging + "/gone"), staging + "/.minecraft");
	}

	void linkIsRefusedWhereItIsNotOffered()
	{
		const FoundLauncher launcher = scanMultiMc(Source::Prism, place("prism", "prism-nolink"));
		FoundInstance custom = *named(launcher, "Custom");
		QDir(custom.gameDir).removeRecursively();
		custom.linkBlocker = QStringLiteral("Its game folder is missing.");
		const BuildResult result = build(custom, Mode::Link, m_tmp.filePath("staging-nolink"));
		QVERIFY(!result.ok);
		QCOMPARE(result.error, custom.linkBlocker);
	}

	void linksOutOfTheGameFolderAreNotFollowed()
	{
#ifdef Q_OS_WIN
		QSKIP("Symbolic links need privileges on Windows");
#endif
		const QString dir = place("prism", "prism-links");
		const QString game = dir + "/instances/Fabric Pack/.minecraft";
		const QString outside = m_tmp.filePath("outside");
		QDir().mkpath(outside);
		QFile secret(outside + "/secret.txt");
		QVERIFY(secret.open(QIODevice::WriteOnly));
		secret.write("not for copying");
		secret.close();
		QVERIFY(QFile::link(outside + "/secret.txt", game + "/mods/secret.jar"));
		QVERIFY(QFile::link(outside, game + "/saves/Elsewhere"));
		QVERIFY(QFile::link(game + "/config", game + "/saves/Configs")); // inside: followed
		QVERIFY(QFile::link(game + "/config", game + "/config/loop"));   // a loop: walked once
		const CopyPlan plan = planCopy(game);
		QCOMPARE(plan.skippedLinks, 2);
		QStringList files;
		for (const auto& file : plan.files) {
			files << file.relative;
		}
		QVERIFY(!files.join(",").contains("secret"));
		QVERIFY(files.contains("saves/Configs/sodium.json"));
		QCOMPARE(files.count("config/sodium.json"), 1);
	}

	void instanceFoldersLinkedOutsideAreNotListed()
	{
#ifdef Q_OS_WIN
		QSKIP("Symbolic links need privileges on Windows");
#endif
		const QString dir = place("prism", "prism-outlink");
		const QString elsewhere = place("prism", "prism-elsewhere");
		QVERIFY(QFile::link(elsewhere + "/instances/Fabric Pack", dir + "/instances/Borrowed"));
		const FoundLauncher launcher = scanMultiMc(Source::Prism, dir);
		QCOMPARE(launcher.instances.size(), 4);
	}

	void cancelledCopyLeavesNothingFinished()
	{
		const QString dir = place("prism", "prism-cancel");
		BuildRequest request;
		request.instance = *named(scanMultiMc(Source::Prism, dir), "Fabric Pack, Remix");
		request.name = request.instance.name;
		std::atomic_bool cancel{true};
		const QString staging = m_tmp.filePath("staging-cancel");
		const BuildResult result = buildInstance(request, staging, cancel);
		QVERIFY(!result.ok);
		QVERIFY(result.cancelled);
		QVERIFY(!QFile::exists(staging + "/instance.cfg"));
	}

	void sizesAreCountedOncePerFolder()
	{
		QList<FoundLauncher> launchers{scanOfficial(place("official", "official-size"))};
		countCopySizes(&launchers);
		qint64 size = -1;
		for (const FoundInstance& instance : launchers.first().instances) {
			if (instance.importable()) {
				QVERIFY(instance.copyBytes > 0);
				QVERIFY(size < 0 || size == instance.copyBytes);
				size = instance.copyBytes;
			}
		}
	}

	/* A look at this computer's real launchers, by hand only:
	 *   KITE_LAUNCHER_IMPORT_DRYRUN=<empty scratch folder> LauncherImport_test realLaunchersDryRun
	 * KITE_LAUNCHER_IMPORT_ALSO=<data folder> adds one more folder in Prism's format (Kite's own, say).
	 * Prints counts only. The smallest instance found is built into the scratch folder, and its
	 * source must be byte for byte the same afterwards. */
	void realLaunchersDryRun()
	{
		const QString scratch = qEnvironmentVariable("KITE_LAUNCHER_IMPORT_DRYRUN");
		if (scratch.isEmpty()) {
			QSKIP("Set KITE_LAUNCHER_IMPORT_DRYRUN to an empty scratch folder to run");
		}
		QList<FoundLauncher> found = scanAll(SearchEnvironment::current());
		const QString also = qEnvironmentVariable("KITE_LAUNCHER_IMPORT_ALSO");
		if (!also.isEmpty()) {
			found.append(scanMultiMc(Source::MeshMC, also));
		}
		countCopySizes(&found);
		const FoundInstance* smallest = nullptr;
		for (const FoundLauncher& launcher : found) {
			int importable = 0, linkable = 0, mods = 0, leftBehind = 0;
			qint64 bytes = 0;
			for (const FoundInstance& instance : launcher.instances) {
				importable += instance.importable() ? 1 : 0;
				linkable += instance.linkable() ? 1 : 0;
				mods += instance.mods.size();
				leftBehind += instance.leftBehind.isEmpty() ? 0 : 1;
				bytes += qMax<qint64>(0, instance.copyBytes);
				if (instance.importable() && (!smallest || instance.copyBytes < smallest->copyBytes)) {
					smallest = &instance;
				}
			}
			qInfo().noquote() << QString("%1: %2 instances, %3 importable, %4 linkable, %5 with parts left behind, "
										 "%6 mod origins, %7 MiB to copy%8")
									 .arg(sourceName(launcher.source))
									 .arg(launcher.instances.size())
									 .arg(importable)
									 .arg(linkable)
									 .arg(leftBehind)
									 .arg(mods)
									 .arg(bytes / (1024 * 1024))
									 .arg(launcher.problem.isEmpty() ? QString() : " (list unreadable)");
		}
		if (!smallest) {
			qInfo() << "Nothing importable found.";
			return;
		}
		const auto before = snapshot(smallest->gameDir);
		const BuildResult result = build(*smallest, Mode::Copy, scratch + "/staging");
		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(snapshot(smallest->gameDir), before);
		qInfo().noquote() << QString("Built the smallest one into the scratch folder: %1 KiB copied, %2 links "
									 "skipped, %3 mod origins written; its source is unchanged.")
								 .arg(result.copiedBytes / 1024)
								 .arg(result.skippedLinks)
								 .arg(result.mappedMods);
	}

	void cappedReadsRefuseLargerFiles()
	{
		const QString path = m_tmp.filePath("capped.json");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(QByteArray(100, 'x'));
		file.close();
		QByteArray bytes;
		bool tooLarge = false;
		QVERIFY(readCapped(path, 100, &bytes, &tooLarge));
		QVERIFY(!readCapped(path, 99, &bytes, &tooLarge));
		QVERIFY(tooLarge);
		QVERIFY(!isSafeName(".."));
		QVERIFY(!isSafeName("a/b"));
		QVERIFY(!isSafeName("C:x"));
		QVERIFY(joinSafe("/r", "mods/../../x").isEmpty());
		QCOMPARE(joinSafe("/r", "mods/a.jar"), QDir("/r").absolutePath() + "/mods/a.jar");
	}
};

QTEST_GUILESS_MAIN(LauncherImportTest)

#include "LauncherImport_test.moc"
