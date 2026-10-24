/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include "MMCZip.h"
#include "minecraft/mod/LocalModParseTask.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModIconCache.h"

namespace
{
	QByteArray png(int width, int height, QColor color = Qt::red)
	{
		QImage image(width, height, QImage::Format_ARGB32);
		image.fill(color);
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		return bytes;
	}

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

	LocalModParseTask::ResultPtr parse(const QString& path,
									   const QString& cacheDir)
	{
		const QFileInfo info(path);
		LocalModParseTask task(0, Mod(info).type(), info, cacheDir);
		task.run();
		return task.result();
	}

	QImage cached(const QString& cacheDir, const QString& key)
	{
		return QImage(ModIconCache::pathFor(cacheDir, key));
	}

	QByteArray fileBytes(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	const ModDependency* dependencyOn(const ModDetails& details,
									  const QString& id,
									  const QString& loader = QString())
	{
		for (const ModDependency& dependency : details.dependencies) {
			if (dependency.id == id &&
				(loader.isEmpty() || dependency.loader == loader)) {
				return &dependency;
			}
		}
		return nullptr;
	}

	QStringList providedIds(const ModDetails& details)
	{
		QStringList ids;
		for (const ProvidedMod& mod : details.provides) {
			ids << mod.id;
		}
		return ids;
	}

	QString providedVersion(const ModDetails& details, const QString& id)
	{
		for (const ProvidedMod& mod : details.provides) {
			if (mod.id == id) {
				return mod.version;
			}
		}
		return QStringLiteral("<not provided>");
	}

	QString fabricJson(const QString& id, const QString& version,
					   const QString& extra = QString())
	{
		return QStringLiteral(R"({"schemaVersion":1,"id":"%1","version":"%2"%3})")
			.arg(id, version,
				 extra.isEmpty() ? QString() : QStringLiteral(",") + extra);
	}
} // namespace

class LocalModParseTaskTest : public QObject
{
	Q_OBJECT

  private slots:
	void init()
	{
		QVERIFY(m_dir.isValid());
		m_cache = m_dir.filePath(QStringLiteral("cache-%1").arg(m_run++));
	}

	void fabricIconString()
	{
		const QString jar = writeZip(
			m_dir.filePath("fabric.jar"),
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"sodium","name":"Sodium",
				  "icon":"assets/sodium/icon.png"})"},
			 {"assets/sodium/icon.png", png(32, 32)}});
		QVERIFY(!jar.isEmpty());

		const auto result = parse(jar, m_cache);
		QVERIFY(result->details);
		QCOMPARE(result->details->name, QStringLiteral("Sodium"));
		QCOMPARE(result->details->icon,
				 QStringLiteral("assets/sodium/icon.png"));
		QCOMPARE(result->iconKey, ModIconCache::keyFor(QFileInfo(jar)));
		// Small logos are kept at their own size.
		QCOMPARE(cached(m_cache, result->iconKey).size(), QSize(32, 32));
	}

	void fabricIconMapPicksLargestWithinLimit()
	{
		const QString jar = writeZip(
			m_dir.filePath("map.jar"),
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"map",
				  "icon":{"16":"i16.png","64":"i64.png","512":"i512.png"}})"},
			 {"i16.png", png(16, 16)},
			 {"i64.png", png(64, 64)},
			 {"i512.png", png(512, 512)}});
		const auto result = parse(jar, m_cache);
		QCOMPARE(result->details->icon, QStringLiteral("i64.png"));
		QCOMPARE(cached(m_cache, result->iconKey).size(), QSize(64, 64));
	}

	void oversizedIconIsShrunk()
	{
		const QString jar = writeZip(
			m_dir.filePath("big.jar"),
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"big","icon":{"512":"i512.png"}})"},
			 {"i512.png", png(512, 256)}});
		const auto result = parse(jar, m_cache);
		QCOMPARE(result->details->icon, QStringLiteral("i512.png"));
		QCOMPARE(cached(m_cache, result->iconKey).size(),
				 QSize(ModIconCache::kMaxExtent, ModIconCache::kMaxExtent / 2));
	}

	void quiltMetadataIcon()
	{
		const QString jar = writeZip(
			m_dir.filePath("quilt.jar"),
			{{"quilt.mod.json",
			  R"({"schema_version":1,"quilt_loader":{"id":"qsl",
				  "version":"7.0.0","metadata":{"name":"Quilt Standard Libraries",
				  "icon":"assets/qsl/icon.png"}}})"},
			 {"assets/qsl/icon.png", png(48, 48)}});
		const auto result = parse(jar, m_cache);
		QVERIFY(result->details);
		QCOMPARE(result->details->name,
				 QStringLiteral("Quilt Standard Libraries"));
		QCOMPARE(result->details->version, QStringLiteral("7.0.0"));
		QVERIFY(!result->iconKey.isEmpty());
	}

	void modsTomlLogoFile()
	{
		const QString jar = writeZip(
			m_dir.filePath("forge.jar"),
			{{"META-INF/mods.toml", "modLoader=\"javafml\"\n"
									"[[mods]]\n"
									"modId=\"create\"\n"
									"version=\"0.5.1\"\n"
									"displayName=\"Create\"\n"
									"logoFile=\"logo.png\"\n"},
			 {"logo.png", png(40, 40)}});
		const auto result = parse(jar, m_cache);
		QCOMPARE(result->details->icon, QStringLiteral("logo.png"));
		QCOMPARE(cached(m_cache, result->iconKey).size(), QSize(40, 40));
	}

	void mcmodInfoLogoFileWithLeadingSlash()
	{
		const QString jar = writeZip(
			m_dir.filePath("legacy.jar"),
			{{"mcmod.info", R"([{"modid":"ic2","name":"IndustrialCraft 2",
							   "logoFile":"/assets/ic2/logo.png"}])"},
			 {"assets/ic2/logo.png", png(64, 32)}});
		const auto result = parse(jar, m_cache);
		QCOMPARE(result->details->icon, QStringLiteral("/assets/ic2/logo.png"));
		QCOMPARE(cached(m_cache, result->iconKey).size(), QSize(64, 32));
	}

	void packPngFallback()
	{
		const QString zip = writeZip(
			m_dir.filePath("Faithful.zip"),
			{{"pack.mcmeta", R"({"pack":{"pack_format":34}})"},
			 {"pack.png", png(128, 128)}});
		const auto result = parse(zip, m_cache);
		QVERIFY(!result->details);
		QVERIFY(!result->iconKey.isEmpty());
	}

	void folderPackPng()
	{
		QDir dir(m_dir.path());
		QVERIFY(dir.mkpath("folderpack"));
		QFile icon(dir.filePath("folderpack/pack.png"));
		QVERIFY(icon.open(QIODevice::WriteOnly));
		icon.write(png(16, 16));
		icon.close();

		const auto result = parse(dir.filePath("folderpack"), m_cache);
		QVERIFY(!result->iconKey.isEmpty());
	}

	void folderLogoCannotEscapeTheFolder()
	{
		QDir dir(m_dir.path());
		QVERIFY(dir.mkpath("escape/inner"));
		QFile outside(dir.filePath("escape/outside.png"));
		QVERIFY(outside.open(QIODevice::WriteOnly));
		outside.write(png(16, 16));
		outside.close();
		QFile info(dir.filePath("escape/inner/mcmod.info"));
		QVERIFY(info.open(QIODevice::WriteOnly));
		info.write(R"([{"modid":"x","logoFile":"../outside.png"}])");
		info.close();

		const auto result = parse(dir.filePath("escape/inner"), m_cache);
		QVERIFY(result->details);
		QVERIFY(result->iconKey.isEmpty());
	}

	void noIcon()
	{
		const QString jar = writeZip(
			m_dir.filePath("plain.jar"),
			{{"fabric.mod.json", R"({"schemaVersion":1,"id":"plain"})"}});
		const auto result = parse(jar, m_cache);
		QVERIFY(result->details);
		QVERIFY(result->iconKey.isEmpty());
		QVERIFY(!QDir(m_cache).exists() || QDir(m_cache).isEmpty());
	}

	void undecodableIconIsIgnored()
	{
		const QString jar = writeZip(
			m_dir.filePath("broken.jar"),
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"broken","icon":"icon.png"})"},
			 {"icon.png", "not a png"}});
		const auto result = parse(jar, m_cache);
		QVERIFY(result->iconKey.isEmpty());
	}

	void noCacheDirSkipsIcons()
	{
		const QString jar = writeZip(
			m_dir.filePath("nocache.jar"),
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"nocache","icon":"icon.png"})"},
			 {"icon.png", png(16, 16)}});
		const auto result = parse(jar, QString());
		QVERIFY(result->details);
		QVERIFY(result->iconKey.isEmpty());
	}

	void keyFollowsPathAndModificationTime()
	{
		const QString jar = writeZip(
			m_dir.filePath("keyed.jar"),
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"keyed","icon":"icon.png"})"},
			 {"icon.png", png(16, 16)}});
		const QString key = ModIconCache::keyFor(QFileInfo(jar));
		QVERIFY(ModIconCache::isValidKey(key));

		// Disabling a mod renames it; the logo is the same file's.
		QVERIFY(QFile::copy(jar, jar + ".disabled"));
		QFile disabled(jar + ".disabled");
		QVERIFY(disabled.open(QIODevice::ReadWrite));
		QVERIFY(disabled.setFileTime(QFileInfo(jar).lastModified(),
									 QFileDevice::FileModificationTime));
		disabled.close();
		QCOMPARE(ModIconCache::keyFor(QFileInfo(jar + ".disabled")), key);

		// A rebuilt jar is a different entry, never a stale logo.
		QFile file(jar);
		QVERIFY(file.open(QIODevice::ReadWrite));
		QVERIFY(file.setFileTime(QDateTime::currentDateTime().addDays(1),
								 QFileDevice::FileModificationTime));
		file.close();
		QVERIFY(ModIconCache::keyFor(QFileInfo(jar)) != key);

		// A second parse of an unchanged file is served from the cache.
		const auto first = parse(jar, m_cache);
		QVERIFY(ModIconCache::contains(m_cache, first->iconKey));
		const auto second = parse(jar, m_cache);
		QCOMPARE(second->iconKey, first->iconKey);
	}

	void invalidKeysAreRejected()
	{
		QVERIFY(!ModIconCache::isValidKey(QString()));
		QVERIFY(!ModIconCache::isValidKey(QStringLiteral("../../etc/passwd")));
		QVERIFY(!ModIconCache::isValidKey(QString(40, QLatin1Char('G'))));
		QVERIFY(!ModIconCache::store(m_cache, QStringLiteral("../x"),
									 png(16, 16)));
	}

	// The role QML binds to, end to end through the model's own async load
	// and parse. The cache directory is resolved from the current
	// directory, as it is in the launcher (its data directory).
	void modelExposesIconUrl()
	{
		QDir root(m_dir.filePath("datadir"));
		QVERIFY(root.mkpath("mods"));
		const QString previous = QDir::currentPath();
		QVERIFY(QDir::setCurrent(root.path()));

		const QString jar = writeZip(
			root.filePath("mods/withicon.jar"),
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"withicon","icon":"icon.png"})"},
			 {"icon.png", png(16, 16)}});
		writeZip(root.filePath("mods/without.jar"),
				 {{"fabric.mod.json", R"({"schemaVersion":1,"id":"without"})"}});

		{
			ModFolderModel model(root.filePath("mods"));
			model.update();
			QTRY_COMPARE(model.rowCount(QModelIndex()), 2);

			QString withIcon;
			QString without = QStringLiteral("unset");
			// Rows resolve in whatever order the worker threads finish.
			const auto scan = [&]() {
				for (int row = 0; row < model.rowCount(QModelIndex()); ++row) {
					const QModelIndex index = model.index(row, 0);
					const QString name =
						model.data(index, ModFolderModel::NameRole).toString();
					const QString url =
						model.data(index, ModFolderModel::IconUrlRole)
							.toString();
					if (name == QStringLiteral("withicon")) {
						withIcon = url;
					} else {
						without = url;
					}
				}
				return !withIcon.isEmpty();
			};
			QTRY_VERIFY(scan());
			QCOMPARE(withIcon, QStringLiteral("image://modicon/") +
								   ModIconCache::keyFor(QFileInfo(jar)));
			QVERIFY(without.isEmpty());
			QVERIFY(QFileInfo::exists(
				root.filePath("cache/LocalModIcons/" +
							  withIcon.section(QLatin1Char('/'), -1) +
							  ".png")));
			QVERIFY(model.roleNames().value(ModFolderModel::IconUrlRole) ==
					"iconUrl");
		}
		QVERIFY(QDir::setCurrent(previous));
	}

	// ---- loaders, dependencies and provided ids ---------------------------

	void fabricDependencies()
	{
		const auto result = parseJar(
			"deps.jar",
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"m","version":"1.0.0",
				  "depends":{"fabricloader":">=0.15.0",
							 "minecraft":["1.20.1","~1.20.2"],
							 "fabric-api":"*",
							 "java":["*"]},
				  "breaks":{"badmod":"<2.0.0"},
				  "conflicts":{"sodium":"0.5.x"},
				  "recommends":{"modmenu":"*"},
				  "suggests":{"extra":"*"}})"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(d.loaders, QStringList{"fabric"});
		QCOMPARE(d.dependencies.size(), 6);

		const ModDependency* loader = dependencyOn(d, "fabricloader");
		QVERIFY(loader);
		QCOMPARE(loader->kind, ModDependency::Kind::Required);
		QCOMPARE(loader->syntax, ModDependency::Syntax::Fabric);
		QCOMPARE(loader->versions, QStringList{">=0.15.0"});
		QCOMPARE(loader->loader, QStringLiteral("fabric"));

		QCOMPARE(dependencyOn(d, "minecraft")->versions,
				 (QStringList{"1.20.1", "~1.20.2"}));
		// "*" - alone or as the only alternative - is any version.
		QVERIFY(dependencyOn(d, "fabric-api")->versions.isEmpty());
		QVERIFY(dependencyOn(d, "java")->versions.isEmpty());

		const ModDependency* breaks = dependencyOn(d, "badmod");
		QCOMPARE(breaks->kind, ModDependency::Kind::Incompatible);
		QCOMPARE(breaks->versions, QStringList{"<2.0.0"});
		const ModDependency* conflicts = dependencyOn(d, "sodium");
		QCOMPARE(conflicts->kind, ModDependency::Kind::Discouraged);
		QCOMPARE(conflicts->versions, QStringList{"0.5.x"});

		// Hints for the user, not something the loader acts on.
		QVERIFY(!dependencyOn(d, "modmenu"));
		QVERIFY(!dependencyOn(d, "extra"));
	}

	void fabricProvides()
	{
		const auto result = parseJar(
			"provides.jar",
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"main","version":"3.1.4",
				  "provides":["alias","legacy-id","alias"]})"}});
		QVERIFY(result->details);
		QCOMPARE(providedIds(*result->details),
				 (QStringList{"alias", "legacy-id"}));
		QCOMPARE(providedVersion(*result->details, "alias"),
				 QStringLiteral("3.1.4"));
	}

	void quiltDependencies()
	{
		const auto result = parseJar(
			"quiltdeps.jar",
			{{"quilt.mod.json",
			  R"({"schema_version":1,"quilt_loader":{"id":"q","version":"2.0.0",
				  "depends":[
					"plain",
					{"id":"org.quiltmc:qsl","versions":">=7.0.0"},
					{"id":"alternatives","versions":["1.0.0","2.0.0"]},
					{"id":"anyof","versions":{"any":[">=1 <2",">=3"]}},
					{"id":"allof","versions":{"all":[">=1","<2"]}},
					{"id":"star","versions":"*"},
					{"id":"optional-mod","optional":true},
					{"id":"unlessful","versions":">=1","unless":"other"},
					[{"id":"group-a"},{"id":"group-b"}]
				  ],
				  "breaks":[{"id":"badmod","versions":"<1.0.0"},"always-bad"]}})"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(d.loaders, QStringList{"quilt"});

		const ModDependency* plain = dependencyOn(d, "plain");
		QVERIFY(plain);
		QVERIFY(plain->versions.isEmpty());
		QCOMPARE(plain->kind, ModDependency::Kind::Required);
		QCOMPARE(plain->syntax, ModDependency::Syntax::Fabric);
		QCOMPARE(plain->loader, QStringLiteral("quilt"));

		// Only what follows the last ':' is the mod's id.
		QVERIFY(!dependencyOn(d, "org.quiltmc:qsl"));
		QCOMPARE(dependencyOn(d, "qsl")->versions, QStringList{">=7.0.0"});
		QCOMPARE(dependencyOn(d, "alternatives")->versions,
				 (QStringList{"1.0.0", "2.0.0"}));
		QCOMPARE(dependencyOn(d, "anyof")->versions,
				 (QStringList{">=1 <2", ">=3"}));
		// "all" is one alternative: space separated predicates are ANDed.
		QCOMPARE(dependencyOn(d, "allof")->versions, QStringList{">=1 <2"});
		QVERIFY(dependencyOn(d, "star")->versions.isEmpty());
		QCOMPARE(dependencyOn(d, "unlessful")->versions, QStringList{">=1"});

		QVERIFY(!dependencyOn(d, "optional-mod"));
		QVERIFY(!dependencyOn(d, "group-a"));
		QVERIFY(!dependencyOn(d, "group-b"));

		const ModDependency* breaks = dependencyOn(d, "badmod");
		QCOMPARE(breaks->kind, ModDependency::Kind::Incompatible);
		QCOMPARE(breaks->versions, QStringList{"<1.0.0"});
		QCOMPARE(dependencyOn(d, "always-bad")->kind,
				 ModDependency::Kind::Incompatible);
		QCOMPARE(d.dependencies.size(), 9);
	}

	void quiltProvides()
	{
		const auto result = parseJar(
			"quiltprovides.jar",
			{{"quilt.mod.json",
			  R"({"schema_version":1,"quilt_loader":{"id":"q","version":"2.0.0",
				  "provides":["plain",
							  {"id":"versioned","version":"9.9"},
							  {"id":"inherits"},
							  "org.example:grouped"]}})"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(providedIds(d),
				 (QStringList{"plain", "versioned", "inherits", "grouped"}));
		QCOMPARE(providedVersion(d, "plain"), QStringLiteral("2.0.0"));
		QCOMPARE(providedVersion(d, "versioned"), QStringLiteral("9.9"));
		QCOMPARE(providedVersion(d, "inherits"), QStringLiteral("2.0.0"));
	}

	void forgeDependenciesMandatoryFlag()
	{
		const auto result = parseJar(
			"forgedeps.jar",
			{{"META-INF/mods.toml",
			  "modLoader=\"javafml\"\n"
			  "[[mods]]\n"
			  "modId=\"examplemod\"\n"
			  "version=\"1.0.0\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"forge\"\n"
			  "mandatory=true\n"
			  "versionRange=\"[47,)\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"jei\"\n"
			  "mandatory=false\n"
			  "versionRange=\"[15,)\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"minecraft\"\n"
			  "mandatory=true\n"
			  "versionRange=\"${minecraft_version_range}\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"anyversion\"\n"
			  "mandatory=true\n"
			  "[[dependencies.stranger]]\n"
			  "modId=\"ignored\"\n"
			  "mandatory=true\n"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(d.loaders, QStringList{"forge"});

		const ModDependency* forge = dependencyOn(d, "forge");
		QVERIFY(forge);
		QCOMPARE(forge->kind, ModDependency::Kind::Required);
		QCOMPARE(forge->syntax, ModDependency::Syntax::Maven);
		QCOMPARE(forge->versions, QStringList{"[47,)"});
		QCOMPARE(forge->loader, QStringLiteral("forge"));

		QVERIFY(!dependencyOn(d, "jei"));
		// An unexpanded build variable: kept, but with no range to check.
		const ModDependency* minecraft = dependencyOn(d, "minecraft");
		QVERIFY(minecraft);
		QVERIFY(minecraft->versions.isEmpty());
		QVERIFY(dependencyOn(d, "anyversion")->versions.isEmpty());
		// Only the dependencies of mods the file declares count.
		QVERIFY(!dependencyOn(d, "ignored"));
		QCOMPARE(d.dependencies.size(), 3);
	}

	void neoForgeDependencyTypes()
	{
		const auto result = parseJar(
			"neotypes.jar",
			{{"META-INF/neoforge.mods.toml",
			  "modLoader=\"javafml\"\n"
			  "[[mods]]\n"
			  "modId=\"examplemod\"\n"
			  "version=\"1.0.0\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"neoforge\"\n"
			  "type=\"required\"\n"
			  "versionRange=\"[21.1,)\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"mandatorymod\"\n"
			  "type=\"REQUIRED\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"optionalmod\"\n"
			  "type=\"optional\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"clashing\"\n"
			  "type=\"incompatible\"\n"
			  "versionRange=\"[1,2)\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"unwise\"\n"
			  "type=\"Discouraged\"\n"
			  // type decides over the older flag
			  "[[dependencies.examplemod]]\n"
			  "modId=\"typed\"\n"
			  "type=\"optional\"\n"
			  "mandatory=true\n"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(d.loaders, QStringList{"neoforge"});

		QCOMPARE(dependencyOn(d, "neoforge")->kind,
				 ModDependency::Kind::Required);
		QCOMPARE(dependencyOn(d, "neoforge")->versions, QStringList{"[21.1,)"});
		QCOMPARE(dependencyOn(d, "mandatorymod")->kind,
				 ModDependency::Kind::Required);
		QVERIFY(!dependencyOn(d, "optionalmod"));
		QCOMPARE(dependencyOn(d, "clashing")->kind,
				 ModDependency::Kind::Incompatible);
		QCOMPARE(dependencyOn(d, "clashing")->versions, QStringList{"[1,2)"});
		QCOMPARE(dependencyOn(d, "unwise")->kind,
				 ModDependency::Kind::Discouraged);
		QVERIFY(!dependencyOn(d, "typed"));
		QCOMPARE(d.dependencies.size(), 4);
		for (const ModDependency& dependency : d.dependencies) {
			QCOMPARE(dependency.loader, QStringLiteral("neoforge"));
			QCOMPARE(dependency.syntax, ModDependency::Syntax::Maven);
		}
	}

	// NeoForge's file needs no flag at all: a dependency is required unless it
	// says otherwise. Forge's file does need one.
	void dependencyWithoutFlagIsRequiredOnlyInNeoForgesFile()
	{
		const QByteArray body =
			"[[mods]]\n"
			"modId=\"examplemod\"\n"
			"version=\"1.0.0\"\n"
			"[[dependencies.examplemod]]\n"
			"modId=\"bare\"\n"
			"versionRange=\"[1,)\"\n"
			"[[dependencies.examplemod]]\n"
			"modId=\"unreadflag\"\n"
			"required=true\n"
			"[[dependencies.examplemod]]\n"
			"modId=\"switchedoff\"\n"
			"mandatory=false\n";
		const auto neo =
			parseJar("neobare.jar", {{"META-INF/neoforge.mods.toml", body}});
		QVERIFY(neo->details);
		QCOMPARE(dependencyOn(*neo->details, "bare")->kind,
				 ModDependency::Kind::Required);
		QVERIFY(dependencyOn(*neo->details, "unreadflag"));
		QVERIFY(!dependencyOn(*neo->details, "switchedoff"));

		const auto forge =
			parseJar("forgebare.jar", {{"META-INF/mods.toml", body}});
		QVERIFY(forge->details);
		QVERIFY(forge->details->dependencies.isEmpty());
	}

	// Jars that carry mods.toml and neoforge.mods.toml with the same
	// dependencies are common; they still make one declaration each.
	void sameDependencyInBothTomlFilesIsKeptOnce()
	{
		const QByteArray body =
			"[[mods]]\n"
			"modId=\"examplemod\"\n"
			"version=\"1.0.0\"\n"
			"[[dependencies.examplemod]]\n"
			"modId=\"neoforge\"\n"
			"mandatory=true\n"
			"versionRange=\"[20,)\"\n"
			"[[dependencies.examplemod]]\n"
			"modId=\"minecraft\"\n"
			"mandatory=true\n"
			"versionRange=\"[1.20.2,)\"\n";
		const auto result = parseJar("twotomls.jar",
									 {{"META-INF/mods.toml", body},
									  {"META-INF/neoforge.mods.toml", body}});
		QVERIFY(result->details);
		QCOMPARE(result->details->loaders, QStringList{"neoforge"});
		QCOMPARE(result->details->dependencies.size(), 2);
	}

	void neoForgeModsTomlIsShownLikeModsToml()
	{
		const auto result = parseJar(
			"neoonly.jar",
			{{"META-INF/neoforge.mods.toml",
			  "[[mods]]\n"
			  "modId=\"neomod\"\n"
			  "version=\"2.5.0\"\n"
			  "displayName=\"Neo Mod\"\n"
			  "authors=\"Someone\"\n"}});
		QVERIFY(result->details);
		QCOMPARE(result->details->mod_id, QStringLiteral("neomod"));
		QCOMPARE(result->details->version, QStringLiteral("2.5.0"));
		QCOMPARE(result->details->name, QStringLiteral("Neo Mod"));
		QCOMPARE(result->details->authors, QStringList{"Someone"});
		QCOMPARE(result->details->loaders, QStringList{"neoforge"});
	}

	void modsTomlDependingOnNeoForgeIsNeoForge()
	{
		const auto result = parseJar(
			"neoinmods.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\n"
			  "modId=\"examplemod\"\n"
			  "version=\"1.0.0\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"neoforge\"\n"
			  "type=\"required\"\n"
			  "versionRange=\"[20.4,)\"\n"
			  "[[dependencies.examplemod]]\n"
			  "modId=\"othermod\"\n"
			  "mandatory=true\n"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(d.loaders, QStringList{"neoforge"});
		QCOMPARE(dependencyOn(d, "neoforge")->loader,
				 QStringLiteral("neoforge"));
		QCOMPARE(dependencyOn(d, "othermod")->loader,
				 QStringLiteral("neoforge"));
	}

	void multiLoaderJarKeepsEachLoadersDependencies()
	{
		const auto result = parseJar(
			"both.jar",
			{{"fabric.mod.json",
			  R"({"schemaVersion":1,"id":"both","version":"1.0.0",
				  "name":"From Fabric",
				  "depends":{"fabric-api":"*"}})"},
			 {"META-INF/mods.toml",
			  "[[mods]]\n"
			  "modId=\"both\"\n"
			  "version=\"1.0.0\"\n"
			  "displayName=\"From Forge\"\n"
			  "[[dependencies.both]]\n"
			  "modId=\"forge\"\n"
			  "mandatory=true\n"},
			 {"META-INF/neoforge.mods.toml",
			  "[[mods]]\n"
			  "modId=\"both\"\n"
			  "version=\"1.0.0\"\n"
			  "[[dependencies.both]]\n"
			  "modId=\"neoforge\"\n"
			  "type=\"required\"\n"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		// mods.toml still decides what the mod is shown as.
		QCOMPARE(d.name, QStringLiteral("From Forge"));
		QCOMPARE(d.loaders, (QStringList{"forge", "neoforge", "fabric"}));
		QCOMPARE(d.dependencies.size(), 3);
		QCOMPARE(dependencyOn(d, "fabric-api")->loader,
				 QStringLiteral("fabric"));
		QCOMPARE(dependencyOn(d, "forge")->loader, QStringLiteral("forge"));
		QCOMPARE(dependencyOn(d, "neoforge")->loader,
				 QStringLiteral("neoforge"));
	}

	void metadataPriorityAndFallThrough()
	{
		const auto fabricOnly = parseJar(
			"brokenforge.jar",
			{{"META-INF/mods.toml", "this is = = not toml"},
			 {"fabric.mod.json", fabricJson("fallback", "1.0.0").toUtf8()}});
		QVERIFY(fabricOnly->details);
		QCOMPARE(fabricOnly->details->mod_id, QStringLiteral("fallback"));
		QCOMPARE(fabricOnly->details->loaders, QStringList{"fabric"});

		const auto legacy = parseJar(
			"legacyplusfabric.jar",
			{{"mcmod.info", R"([{"modid":"legacyid","name":"Legacy"}])"},
			 {"fabric.mod.json", fabricJson("fabricid", "1.0.0").toUtf8()}});
		QVERIFY(legacy->details);
		QCOMPARE(legacy->details->mod_id, QStringLiteral("legacyid"));
		QCOMPARE(legacy->details->loaders, (QStringList{"forge", "fabric"}));
	}

	void modsTomlDeclaringSeveralModsProvidesTheRest()
	{
		const auto result = parseJar(
			"several.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\n"
			  "modId=\"first\"\n"
			  "version=\"1.0.0\"\n"
			  "[[mods]]\n"
			  "modId=\"second\"\n"
			  "version=\"2.0.0\"\n"
			  "[[mods]]\n"
			  "modId=\"third\"\n"
			  "version=\"${file.jarVersion}\"\n"
			  "[[dependencies.first]]\n"
			  "modId=\"second\"\n"
			  "mandatory=true\n"
			  "[[dependencies.second]]\n"
			  "modId=\"elsewhere\"\n"
			  "mandatory=true\n"},
			 {"META-INF/MANIFEST.MF",
			  "Manifest-Version: 1.0\r\nImplementation-Version: 7.7.7\r\n"}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(d.mod_id, QStringLiteral("first"));
		QCOMPARE(providedIds(d), (QStringList{"second", "third"}));
		QCOMPARE(providedVersion(d, "second"), QStringLiteral("2.0.0"));
		QCOMPARE(providedVersion(d, "third"), QStringLiteral("7.7.7"));
		// Needing a mod of the same file is not a dependency to satisfy.
		QVERIFY(!dependencyOn(d, "second"));
		QVERIFY(dependencyOn(d, "elsewhere"));
	}

	void jarVersionPlaceholderIsStillSubstituted()
	{
		const QByteArray toml =
			"[[mods]]\nmodId=\"m\"\nversion=\"${file.jarVersion}\"\n";
		for (const QString& entry :
			 {QStringLiteral("META-INF/mods.toml"),
			  QStringLiteral("META-INF/neoforge.mods.toml")}) {
			const auto withVersion = parseJar(
				"jarversion.jar",
				{{entry, toml},
				 {"META-INF/MANIFEST.MF",
				  "Manifest-Version: 1.0\nImplementation-Version: 1.2.3\n"}});
			QVERIFY(withVersion->details);
			QCOMPARE(withVersion->details->version, QStringLiteral("1.2.3"));
			QFile::remove(m_dir.filePath("jarversion.jar"));

			// A manifest that does not say, as Forge does it.
			const auto none = parseJar(
				"jarversion.jar",
				{{entry, toml}, {"META-INF/MANIFEST.MF", "Manifest-Version: 1.0\n"}});
			QCOMPARE(none->details->version, QStringLiteral("NONE"));
			QFile::remove(m_dir.filePath("jarversion.jar"));

			// No manifest at all leaves the placeholder as written.
			const auto missing = parseJar("jarversion.jar", {{entry, toml}});
			QCOMPARE(missing->details->version,
					 QStringLiteral("${file.jarVersion}"));
			QFile::remove(m_dir.filePath("jarversion.jar"));
		}
	}

	void fabricNestedJarsAreProvided()
	{
		// inner-two sits inside inner-one, which sits inside the outer jar.
		const QByteArray innerTwo = jarBytes(
			"inner-two.jar",
			{{"fabric.mod.json", fabricJson("inner-two", "0.0.2").toUtf8()}});
		const QByteArray innerOne = jarBytes(
			"inner-one.jar",
			{{"fabric.mod.json",
			  fabricJson("inner-one", "0.0.1",
						 R"("provides":["inner-one-alias"],
							"jars":[{"file":"META-INF/jars/inner-two.jar"}])")
				  .toUtf8()},
			 {"META-INF/jars/inner-two.jar", innerTwo}});
		const auto result = parseJar(
			"outer.jar",
			{{"fabric.mod.json",
			  fabricJson("outer", "5.0.0",
						 R"("name":"Outer","depends":{"fabric-api":"*"},
							"jars":[{"file":"META-INF/jars/inner-one.jar"},
									{"file":"META-INF/jars/missing.jar"}])")
				  .toUtf8()},
			 {"META-INF/jars/inner-one.jar", innerOne}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(providedIds(d), (QStringList{"inner-one", "inner-one-alias",
											  "inner-two"}));
		QCOMPARE(providedVersion(d, "inner-one"), QStringLiteral("0.0.1"));
		QCOMPARE(providedVersion(d, "inner-one-alias"),
				 QStringLiteral("0.0.1"));
		QCOMPARE(providedVersion(d, "inner-two"), QStringLiteral("0.0.2"));

		// What the bundled mods are says nothing about the outer one.
		QCOMPARE(d.mod_id, QStringLiteral("outer"));
		QCOMPARE(d.name, QStringLiteral("Outer"));
		QCOMPARE(d.loaders, QStringList{"fabric"});
		QCOMPARE(d.dependencies.size(), 1);
	}

	void nestedJarsDoNotChangeTheOuterLoadersOrDependencies()
	{
		const QByteArray forgeInner = jarBytes(
			"forge-inner.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\nmodId=\"forgeinner\"\nversion=\"1.0\"\n"
			  "[[dependencies.forgeinner]]\nmodId=\"forge\"\nmandatory=true\n"}});
		const auto result = parseJar(
			"fabricwithforge.jar",
			{{"fabric.mod.json",
			  fabricJson("outer", "1.0.0",
						 R"("jars":[{"file":"META-INF/jars/forge-inner.jar"}])")
				  .toUtf8()},
			 {"META-INF/jars/forge-inner.jar", forgeInner}});
		QVERIFY(result->details);
		QCOMPARE(providedIds(*result->details), QStringList{"forgeinner"});
		QCOMPARE(result->details->loaders, QStringList{"fabric"});
		QVERIFY(result->details->dependencies.isEmpty());
	}

	void quiltNestedJarsAreProvided()
	{
		const QByteArray inner = jarBytes(
			"quilt-inner.jar",
			{{"quilt.mod.json",
			  R"({"schema_version":1,"quilt_loader":{"id":"qinner",
				  "version":"1.1.1","provides":["qinner-alias"]}})"}});
		const auto result = parseJar(
			"quiltouter.jar",
			{{"quilt.mod.json",
			  R"({"schema_version":1,"quilt_loader":{"id":"qouter",
				  "version":"1.0.0","jars":["META-INF/jars/quilt-inner.jar"]}})"},
			 {"META-INF/jars/quilt-inner.jar", inner}});
		QVERIFY(result->details);
		QCOMPARE(providedIds(*result->details),
				 (QStringList{"qinner", "qinner-alias"}));
		QCOMPARE(providedVersion(*result->details, "qinner"),
				 QStringLiteral("1.1.1"));
	}

	void jarJarNestedForgeJarsAreProvided()
	{
		const QByteArray inner = jarBytes(
			"jarjar-inner.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\nmodId=\"bundled\"\nversion=\"4.2.0\"\n"
			  "[[mods]]\nmodId=\"bundled_api\"\nversion=\"4.2.1\"\n"}});
		const auto result = parseJar(
			"jarjar-outer.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\nmodId=\"host\"\nversion=\"1.0.0\"\n"},
			 {"META-INF/jarjar/metadata.json",
			  // Not a raw string: the version range's closing parenthesis
			  // and quote would end it early.
			  "{\"jars\":[{\"identifier\":{\"group\":\"g\","
			  "\"artifact\":\"bundled\"},"
			  "\"version\":{\"range\":\"[4,)\",\"artifactVersion\":\"4.2.0\"},"
			  "\"path\":\"META-INF/jarjar/bundled-4.2.0.jar\","
			  "\"isObfuscated\":false}]}"},
			 {"META-INF/jarjar/bundled-4.2.0.jar", inner}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		QCOMPARE(d.mod_id, QStringLiteral("host"));
		QCOMPARE(providedIds(d), (QStringList{"bundled", "bundled_api"}));
		QCOMPARE(providedVersion(d, "bundled"), QStringLiteral("4.2.0"));
		QCOMPARE(providedVersion(d, "bundled_api"), QStringLiteral("4.2.1"));
		QCOMPARE(d.loaders, QStringList{"forge"});
	}

	void nestedJarsStopAtTheDepthCap()
	{
		// level4 is bundled in level3, in level2, in level1, in the outer
		// jar: one deeper than the cap allows.
		QByteArray inner =
			jarBytes("level4.jar",
					 {{"fabric.mod.json", fabricJson("level4", "4").toUtf8()}});
		for (int level = 3; level >= 1; --level) {
			const QString id = QStringLiteral("level%1").arg(level);
			inner = jarBytes(
				id + ".jar",
				{{"fabric.mod.json",
				  fabricJson(id, QString::number(level),
							 R"("jars":[{"file":"nested.jar"}])")
					  .toUtf8()},
				 {"nested.jar", inner}});
		}
		const auto result = parseJar(
			"deepouter.jar",
			{{"fabric.mod.json",
			  fabricJson("outer", "1", R"("jars":[{"file":"nested.jar"}])")
				  .toUtf8()},
			 {"nested.jar", inner}});
		QVERIFY(result->details);
		QCOMPARE(providedIds(*result->details),
				 (QStringList{"level1", "level2", "level3"}));
	}

	void nestedJarsListedTwiceAreProvidedOnce()
	{
		const QByteArray inner = jarBytes(
			"dup-inner.jar",
			{{"fabric.mod.json", fabricJson("dup", "9.9.9").toUtf8()}});
		const auto result = parseJar(
			"dup-outer.jar",
			{{"fabric.mod.json",
			  fabricJson("outer", "1.0.0",
						 R"("provides":["dup"],
							"jars":[{"file":"a.jar"},{"file":"/a.jar"},
									{"file":"b.jar"}])")
				  .toUtf8()},
			 {"a.jar", inner},
			 {"b.jar", inner}});
		QVERIFY(result->details);
		QCOMPARE(providedIds(*result->details), QStringList{"dup"});
		// The first declaration wins: the outer file's own "provides".
		QCOMPARE(providedVersion(*result->details, "dup"),
				 QStringLiteral("1.0.0"));
	}

	void liteModLoader()
	{
		const QString path = writeZip(
			m_dir.filePath("old.litemod"),
			{{"litemod.json", R"({"name":"oldmod","version":"1.2",
								 "mcversion":"1.12.2"})"}});
		QVERIFY(!path.isEmpty());
		const auto result = parse(path, QString());
		QVERIFY(result->details);
		QCOMPARE(result->details->mod_id, QStringLiteral("oldmod"));
		QCOMPARE(result->details->loaders, QStringList{"liteloader"});
		QVERIFY(result->details->dependencies.isEmpty());
	}

	void legacyForgeLoaders()
	{
		const auto zip = parseJar(
			"legacyforge.jar",
			{{"mcmod.info", R"([{"modid":"ic2","name":"IndustrialCraft 2"}])"}});
		QVERIFY(zip->details);
		QCOMPARE(zip->details->loaders, QStringList{"forge"});
		QVERIFY(zip->details->dependencies.isEmpty());

		QDir dir(m_dir.path());
		QVERIFY(dir.mkpath("legacyfolder"));
		QFile info(dir.filePath("legacyfolder/mcmod.info"));
		QVERIFY(info.open(QIODevice::WriteOnly));
		info.write(R"([{"modid":"folder","name":"Folder Mod"}])");
		info.close();
		const auto folder = parse(dir.filePath("legacyfolder"), QString());
		QVERIFY(folder->details);
		QCOMPARE(folder->details->loaders, QStringList{"forge"});
	}

	void optionalNeoForgeDependencyStaysForge()
	{
		// PacketFixer's mods.toml: Forge's file, listing both Forge and
		// NeoForge as optional. Forge 1.20.1 loads it, so it is a Forge file.
		const auto optional = parseJar(
			"optionalneo.jar",
			{{"META-INF/mods.toml",
			  "modLoader=\"javafml\"\n"
			  "[[mods]]\n"
			  "modId=\"packetfixer\"\n"
			  "version=\"3.3.1\"\n"
			  "[[dependencies.packetfixer]]\n"
			  "modId=\"forge\"\n"
			  "mandatory=false\n"
			  "versionRange=\"[0,)\"\n"
			  "[[dependencies.packetfixer]]\n"
			  "modId=\"neoforge\"\n"
			  "mandatory=false\n"
			  "versionRange=\"[0,)\"\n"
			  "[[dependencies.packetfixer]]\n"
			  "modId=\"minecraft\"\n"
			  "mandatory=true\n"
			  "versionRange=\"[1.18,1.20.4]\"\n"}});
		QVERIFY(optional->details);
		QCOMPARE(optional->details->loaders, QStringList{"forge"});
		QCOMPARE(dependencyOn(*optional->details, "minecraft")->loader,
				 QStringLiteral("forge"));
		// Optional ones change nothing about whether the game starts.
		QVERIFY(!dependencyOn(*optional->details, "neoforge"));

		// The same file with the dependency that has to be met is NeoForge's.
		const auto required = parseJar(
			"requiredneo.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\n"
			  "modId=\"neomod\"\n"
			  "version=\"1.0.0\"\n"
			  "[[dependencies.neomod]]\n"
			  "modId=\"neoforge\"\n"
			  "mandatory=true\n"
			  "versionRange=\"[20.4,)\"\n"}});
		QVERIFY(required->details);
		QCOMPARE(required->details->loaders, QStringList{"neoforge"});
	}

	void jarWithOnlyNestedModsProvidesThem()
	{
		// kotlinforforge-all.jar: a manifest, a jarjar list that names no
		// mod ids, and the jars - one of them the mod, the rest libraries.
		const QByteArray mod = jarBytes(
			"kffmod.jar",
			{{"META-INF/neoforge.mods.toml",
			  "[[mods]]\nmodId=\"kotlinforforge\"\nversion=\"5.12.0\"\n"}});
		const QByteArray library =
			jarBytes("kotlin-stdlib.jar", {{"kotlin/Unit.class", "x"}});
		const auto result = parseJar(
			"kotlinforforge-5.12.0-all.jar",
			{{"META-INF/MANIFEST.MF", "Manifest-Version: 1.0\r\n\r\n"},
			 {"META-INF/jarjar/metadata.json",
			  "{\"jars\":[{\"identifier\":{\"group\":\"org.jetbrains.kotlin\","
			  "\"artifact\":\"kotlin-stdlib\"},"
			  "\"path\":\"META-INF/jarjar/kotlin-stdlib.jar\"},"
			  "{\"identifier\":{\"group\":\"thedarkcolour\","
			  "\"artifact\":\"kffmod\"},"
			  "\"path\":\"META-INF/jarjar/kffmod.jar\"}]}"},
			 {"META-INF/jarjar/kotlin-stdlib.jar", library},
			 {"META-INF/jarjar/kffmod.jar", mod}});
		QVERIFY(result->details);
		const ModDetails& d = *result->details;
		// Nothing about the outer file itself, only what it carries.
		QVERIFY(d.mod_id.isEmpty());
		QVERIFY(d.name.isEmpty());
		QVERIFY(d.loaders.isEmpty());
		QVERIFY(d.dependencies.isEmpty());
		QCOMPARE(providedIds(d), QStringList{"kotlinforforge"});
		QCOMPARE(providedVersion(d, "kotlinforforge"), QStringLiteral("5.12.0"));
	}

	void jarWithOnlyNestedLibrariesHasNoDetails()
	{
		const QByteArray library =
			jarBytes("only-lib.jar", {{"some/Class.class", "x"}});
		const auto result = parseJar(
			"fatlibrary.jar",
			{{"META-INF/jarjar/metadata.json",
			  "{\"jars\":[{\"identifier\":{\"group\":\"g\","
			  "\"artifact\":\"a\"},\"path\":\"META-INF/jarjar/a.jar\"}]}"},
			 {"META-INF/jarjar/a.jar", library}});
		QVERIFY(!result->details);
	}

	void manifestNamesTheEmbeddedModJar()
	{
		// Sinytra Connector: no mods.toml, no jarjar list - the manifest
		// names the mod jar, and wraps the long path onto a second line.
		const QByteArray mod = jarBytes(
			"connector-mod.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\nmodId=\"connectormod\"\n"
			  "version=\"${file.jarVersion}\"\n"},
			 {"META-INF/MANIFEST.MF",
			  "Manifest-Version: 1.0\r\n"
			  "Implementation-Version: 1.0.0-beta.47+1.20.1\r\n\r\n"}});
		const auto folded = parseJar(
			"Connector-1.0.0-beta.47+1.20.1.jar",
			{{"META-INF/MANIFEST.MF",
			  "Manifest-Version: 1.0\r\n"
			  "Embedded-Dependencies-Mod: META-INF/jarjar/Connector-1.0.0-beta.47+1.20.\r\n"
			  " 1-mod.jar\r\n"
			  "Specification-Title: Connector\r\n\r\n"},
			 {"META-INF/jarjar/Connector-1.0.0-beta.47+1.20.1-mod.jar", mod}});
		QVERIFY(folded->details);
		QCOMPARE(providedIds(*folded->details), QStringList{"connectormod"});
		QCOMPARE(providedVersion(*folded->details, "connectormod"),
				 QStringLiteral("1.0.0-beta.47+1.20.1"));

		// Unwrapped, with bare newlines and the name in other letters.
		const auto plain = parseJar(
			"plainconnector.jar",
			{{"META-INF/MANIFEST.MF",
			  "Manifest-Version: 1.0\nembedded-dependencies-mod: nested/mod.jar\n"},
			 {"nested/mod.jar", mod}});
		QVERIFY(plain->details);
		QCOMPARE(providedIds(*plain->details), QStringList{"connectormod"});

		// A path that is not in the jar is no mod.
		const auto missing = parseJar(
			"missingconnector.jar",
			{{"META-INF/MANIFEST.MF",
			  "Manifest-Version: 1.0\r\n"
			  "Embedded-Dependencies-Mod: nowhere.jar\r\n"}});
		QVERIFY(!missing->details);
	}

	void manifestVersionFollowsContinuationLines()
	{
		const auto result = parseJar(
			"foldedversion.jar",
			{{"META-INF/mods.toml",
			  "[[mods]]\nmodId=\"m\"\nversion=\"${file.jarVersion}\"\n"},
			 {"META-INF/MANIFEST.MF",
			  "Manifest-Version: 1.0\r\n"
			  "Implementation-Version: 1.2.3-a-version-long-enough-to-be-wr\r\n"
			  " apped\r\n"
			  "Other: x\r\n"}});
		QVERIFY(result->details);
		QCOMPARE(result->details->version,
				 QStringLiteral("1.2.3-a-version-long-enough-to-be-wrapped"));
	}

	void plainLibraryJarHasNoLoaders()
	{
		const auto result = parseJar("lib.jar", {{"some/Class.class", "x"}});
		QVERIFY(!result->details);
	}

  private:
	// A jar written into the temp directory, and parsed without an icon cache.
	LocalModParseTask::ResultPtr parseJar(const QString& name,
										  const Entries& entries)
	{
		const QString path = writeZip(m_dir.filePath(name), entries);
		Q_ASSERT(!path.isEmpty());
		return parse(path, QString());
	}

	// A jar's bytes, to bundle inside another one.
	QByteArray jarBytes(const QString& name, const Entries& entries)
	{
		return fileBytes(writeZip(m_dir.filePath(name), entries));
	}

	QTemporaryDir m_dir;
	QString m_cache;
	int m_run = 0;
};

QTEST_GUILESS_MAIN(LocalModParseTaskTest)

#include "LocalModParseTask_test.moc"
