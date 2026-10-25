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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/KiteModRules.h"

namespace
{
	const QString kHash = QString(64, QLatin1Char('a'));

	QJsonObject buildJson(const QString& minecraft = "26.3",
						  const QString& version = "0.1.0",
						  const QString& minLoader = "0.19.3")
	{
		return QJsonObject{
			{"minecraft", minecraft},
			{"version", version},
			{"minLoader", minLoader},
			{"file", "kite-mod-" + version + "+" + minecraft + ".jar"},
			{"url", "https://meta.kiteclient.com/kite/mod/kite-mod.jar"},
			{"sha256", kHash},
			{"size", 123}};
	}

	QByteArray manifest(const QJsonArray& builds, int formatVersion = 1)
	{
		return QJsonDocument(QJsonObject{{"formatVersion", formatVersion},
										 {"builds", builds}})
			.toJson();
	}

	KiteModBuild build(const QString& minecraft, const QString& version,
					   const QString& minLoader)
	{
		KiteModBuild made;
		made.minecraft = minecraft;
		made.version = version;
		made.minLoader = minLoader;
		made.file = "kite-mod-" + version + ".jar";
		made.size = 4;
		return made;
	}

	void write(const QString& path, const QByteArray& content)
	{
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(content);
	}

	/// Parses a manifest with one entry changed from the valid one.
	std::optional<QList<KiteModBuild>> parseWith(const QString& key,
												 const QJsonValue& value)
	{
		QJsonObject entry = buildJson();
		entry[key] = value;
		return KiteMod::parseManifest(manifest({entry}));
	}
} // namespace

class KiteModRulesTest : public QObject
{
	Q_OBJECT

  private slots:
	void parsesAValidBuild()
	{
		const auto builds = KiteMod::parseManifest(manifest({buildJson()}));
		QVERIFY(builds);
		QCOMPARE(builds->size(), 1);
		const KiteModBuild& first = builds->first();
		QCOMPARE(first.minecraft, QString("26.3"));
		QCOMPARE(first.version, QString("0.1.0"));
		QCOMPARE(first.minLoader, QString("0.19.3"));
		QCOMPARE(first.file, QString("kite-mod-0.1.0+26.3.jar"));
		QCOMPARE(first.sha256, kHash);
		QCOMPARE(first.size, qint64(123));
	}

	void lowercasesTheChecksum()
	{
		const auto builds = parseWith("sha256", kHash.toUpper());
		QVERIFY(builds);
		QCOMPARE(builds->first().sha256, kHash);
	}

	void rejectsWhatIsNotAManifest()
	{
		QVERIFY(!KiteMod::parseManifest("not json"));
		QVERIFY(!KiteMod::parseManifest("[]"));
		QVERIFY(!KiteMod::parseManifest(QByteArray()));
	}

	void rejectsAnUnknownFormatVersion()
	{
		QVERIFY(!KiteMod::parseManifest(manifest({buildJson()}, 2)));
		QVERIFY(!KiteMod::parseManifest(manifest({buildJson()}, 0)));
		QVERIFY(!KiteMod::parseManifest("{\"builds\":[]}"));
	}

	void anEmptyManifestIsStillAManifest()
	{
		const auto builds = KiteMod::parseManifest(manifest({}));
		QVERIFY(builds);
		QVERIFY(builds->isEmpty());
	}

	void skipsEntriesWithAMissingField()
	{
		for (const QString& key : {"minecraft", "version", "minLoader", "file",
								   "url", "sha256", "size"}) {
			QJsonObject entry = buildJson();
			entry.remove(key);
			const auto builds = KiteMod::parseManifest(manifest({entry}));
			QVERIFY2(builds && builds->isEmpty(), qPrintable(key));
		}
	}

	void skipsEntriesThatAreNotHttps()
	{
		for (const QString& url :
			 {"http://meta.kiteclient.com/a.jar", "ftp://x.test/a.jar",
			  "file:///tmp/a.jar", "//meta.kiteclient.com/a.jar", ""}) {
			const auto builds = parseWith("url", url);
			QVERIFY2(builds && builds->isEmpty(), qPrintable(url));
		}
	}

	void skipsEntriesWithABadChecksum()
	{
		for (const QString& hash :
			 {QString(63, QLatin1Char('a')), QString(65, QLatin1Char('a')),
			  QString(64, QLatin1Char('g')), QString()}) {
			const auto builds = parseWith("sha256", hash);
			QVERIFY2(builds && builds->isEmpty(), qPrintable(hash));
		}
	}

	void skipsFileNamesWithAPath()
	{
		for (const QString& file :
			 {"../kite-mod.jar", "a/kite-mod.jar", "a\\kite-mod.jar", "..",
			  "kite-mod.zip", ""}) {
			const auto builds = parseWith("file", file);
			QVERIFY2(builds && builds->isEmpty(), qPrintable(file));
		}
	}

	void skipsEntriesWithoutASize()
	{
		for (const QJsonValue& size :
			 {QJsonValue(0), QJsonValue(-5), QJsonValue("big")}) {
			const auto builds = parseWith("size", size);
			QVERIFY(builds && builds->isEmpty());
		}
	}

	void keepsTheValidEntriesAroundABadOne()
	{
		QJsonObject bad = buildJson("26.2");
		bad["url"] = "http://meta.kiteclient.com/a.jar";
		const auto builds = KiteMod::parseManifest(
			manifest({buildJson("26.1"), bad, "junk", buildJson("26.3")}));
		QVERIFY(builds);
		QCOMPARE(builds->size(), 2);
		QCOMPARE(builds->at(0).minecraft, QString("26.1"));
		QCOMPARE(builds->at(1).minecraft, QString("26.3"));
	}

	void picksTheExactMinecraftVersion()
	{
		const QList<KiteModBuild> builds{build("26.2", "0.1.0", "0.19.3"),
										 build("26.3", "0.1.0", "0.19.3")};
		const auto picked = KiteMod::pickBuild(builds, "26.3", "0.20.1");
		QVERIFY(picked);
		QCOMPARE(picked->minecraft, QString("26.3"));
		QVERIFY(!KiteMod::pickBuild(builds, "26.3.1", "0.20.1"));
		QVERIFY(!KiteMod::pickBuild(builds, "26.1", "0.20.1"));
		QVERIFY(!KiteMod::pickBuild({}, "26.3", "0.20.1"));
	}

	void picksNothingWhenTheLoaderIsTooOld()
	{
		const QList<KiteModBuild> builds{build("26.3", "0.1.0", "0.19.3")};
		QVERIFY(!KiteMod::pickBuild(builds, "26.3", "0.19.2"));
		QVERIFY(!KiteMod::pickBuild(builds, "26.3", "0.9.9"));
		QVERIFY(!KiteMod::pickBuild(builds, "26.3", QString()));
		QVERIFY(KiteMod::pickBuild(builds, "26.3", "0.19.3"));
		QVERIFY(KiteMod::pickBuild(builds, "26.3", "0.20.1"));
	}

	void picksTheHighestModVersion()
	{
		const QList<KiteModBuild> builds{build("26.3", "0.2.0", "0.19.3"),
										 build("26.3", "0.10.0", "0.19.3"),
										 build("26.3", "0.9.0", "0.19.3")};
		QCOMPARE(KiteMod::pickBuild(builds, "26.3", "0.20.1")->version,
				 QString("0.10.0"));
	}

	void skipsAHigherBuildTheLoaderCannotRun()
	{
		const QList<KiteModBuild> builds{build("26.3", "0.1.0", "0.19.3"),
										 build("26.3", "0.2.0", "0.20.1")};
		QCOMPARE(KiteMod::pickBuild(builds, "26.3", "0.19.5")->version,
				 QString("0.1.0"));
		QCOMPARE(KiteMod::pickBuild(builds, "26.3", "0.20.1")->version,
				 QString("0.2.0"));
	}

	void appliesOnlyToAnEnabledFabricInstance()
	{
		QVERIFY(KiteMod::appliesTo(true, "fabric", "0.20.1"));
		QVERIFY(!KiteMod::appliesTo(false, "fabric", "0.20.1"));
		QVERIFY(!KiteMod::appliesTo(true, "quilt", "0.20.1"));
		QVERIFY(!KiteMod::appliesTo(true, "forge", "0.20.1"));
		QVERIFY(!KiteMod::appliesTo(true, "neoforge", "0.20.1"));
		QVERIFY(!KiteMod::appliesTo(true, QString(), "0.20.1"));
		QVERIFY(!KiteMod::appliesTo(true, "fabric", QString()));
	}

	void addsTheJarWhenThePlayerSetNothing()
	{
		QCOMPARE(KiteMod::addModsArgument({}, "/a/kite.jar"),
				 QString("-Dfabric.addMods=/a/kite.jar"));
		QCOMPARE(KiteMod::addModsArgument({"-Xmx2G", "-Dother=1"}, "/a/kite.jar"),
				 QString("-Dfabric.addMods=/a/kite.jar"));
	}

	void keepsTheModsThePlayerAdded()
	{
		const QString sep = QDir::listSeparator();
		QCOMPARE(KiteMod::addModsArgument({"-Dfabric.addMods=/mine.jar"},
										  "/a/kite.jar"),
				 "-Dfabric.addMods=/mine.jar" + sep + "/a/kite.jar");
	}

	void theLastOfTheirsWins()
	{
		const QString sep = QDir::listSeparator();
		const QStringList theirs{"-Dfabric.addMods=/first.jar", "-Xmx1G",
								 "-Dfabric.addMods=/second.jar"};
		QCOMPARE(KiteMod::addModsArgument(theirs, "/a/kite.jar"),
				 "-Dfabric.addMods=/second.jar" + sep + "/a/kite.jar");
	}

	void anEmptyValueOfTheirsAddsNoSeparator()
	{
		QCOMPARE(KiteMod::addModsArgument({"-Dfabric.addMods="}, "/a/kite.jar"),
				 QString("-Dfabric.addMods=/a/kite.jar"));
	}

	void keepsAWindowsPathAsItIs()
	{
		const QString sep = QDir::listSeparator();
		QCOMPARE(KiteMod::addModsArgument(
					 {"-Dfabric.addMods=C:\\mods\\mine.jar"},
					 "C:\\Kite\\cache\\kite.jar"),
				 "-Dfabric.addMods=C:\\mods\\mine.jar" + sep +
					 "C:\\Kite\\cache\\kite.jar");
	}

	void keepsTheJarInsideTheCache()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());
		const KiteModBuild made = build("26.3", "0.1.0", "0.19.3");
		QCOMPARE(KiteMod::cacheFolder(root.path()),
				 QDir(root.path()).filePath("kite-mod"));
		QCOMPARE(KiteMod::jarPath(root.path(), made),
				 QDir(root.path()).filePath("kite-mod/kite-mod-0.1.0.jar"));
	}

	void aJarIsDownloadedOnlyWithTheRightSize()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());
		const KiteModBuild made = build("26.3", "0.1.0", "0.19.3");
		QVERIFY(!KiteMod::isDownloaded(root.path(), made));
		write(KiteMod::jarPath(root.path(), made), "abc");
		QVERIFY(!KiteMod::isDownloaded(root.path(), made));
		write(KiteMod::jarPath(root.path(), made), "abcd");
		QVERIFY(KiteMod::isDownloaded(root.path(), made));
	}

	void readsTheBuildFromTheCachedManifest()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());
		QVERIFY(!KiteMod::cachedBuild(root.path(), "26.3", "0.20.1"));
		write(QDir(root.path()).filePath(KiteMod::kManifestPath), "junk");
		QVERIFY(!KiteMod::cachedBuild(root.path(), "26.3", "0.20.1"));
		write(QDir(root.path()).filePath(KiteMod::kManifestPath),
			  manifest({buildJson("26.2"), buildJson("26.3")}));
		const auto picked = KiteMod::cachedBuild(root.path(), "26.3", "0.20.1");
		QVERIFY(picked);
		QCOMPARE(picked->minecraft, QString("26.3"));
		QVERIFY(!KiteMod::cachedBuild(root.path(), "26.4", "0.20.1"));
	}

	void listsOnlyOlderKiteModJarsToDelete()
	{
		const QStringList names{"kite-mod-0.1.0.jar", "kite-mod-0.2.0.jar",
								"kite-mod-0.0.9.jar", "other.jar",
								"kite-mod-0.0.1.txt"};
		QCOMPARE(KiteMod::outdatedJars(names, "kite-mod-0.2.0.jar"),
				 (QStringList{"kite-mod-0.1.0.jar", "kite-mod-0.0.9.jar"}));
		QVERIFY(KiteMod::outdatedJars({}, "kite-mod-0.2.0.jar").isEmpty());
	}
};

QTEST_GUILESS_MAIN(KiteModRulesTest)

#include "KiteModRules_test.moc"
