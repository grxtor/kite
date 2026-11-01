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

#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "InstanceList.h"
#include "NullInstance.h"
#include "TestSettings.h"

namespace
{
bool writeFile(const QString& path, const QByteArray& contents,
			   const QDateTime& modified)
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	if (file.write(contents) != contents.size()) {
		return false;
	}
	// Flushed before the explicit mtime is set: a write landing after
	// setFileTime() would bump the mtime back to now.
	if (!file.flush()) {
		return false;
	}
	const bool timeSet =
		file.setFileTime(modified, QFileDevice::FileModificationTime);
	file.close();
	return timeSet;
}

InstanceList::CoverDirs coverDirsIn(const QString& root)
{
	return {QDir(root).filePath("meta"),
			QDir(root).filePath("assets/indexes"),
			QDir(root).filePath("assets/objects")};
}

/* A version metadata file naming asset index @p indexId, the index naming a
 * hash for each panorama face in @p faces (face 0: @p hash itself, face n:
 * @p hash followed by n), and the objects. Returns the face 0 object path. */
QString writePanorama(const InstanceList::CoverDirs& dirs,
					  const QString& version, const QString& indexId,
					  const QString& hash, const QList<int>& faces = {0})
{
	const QDateTime now = QDateTime::currentDateTime();
	QDir().mkpath(QDir(dirs.meta).filePath("net.minecraft"));
	QDir().mkpath(dirs.assetIndexes);
	const QString objectDir = QDir(dirs.assetObjects).filePath(hash.left(2));
	QDir().mkpath(objectDir);

	QByteArray objects;
	for (const int face : faces) {
		const QString faceHash =
			face == 0 ? hash : hash + QString::number(face);
		if (!objects.isEmpty()) {
			objects += ", ";
		}
		objects += "\"minecraft/textures/gui/title/background/panorama_" +
				   QByteArray::number(face) + ".png\": {\"hash\": \"" +
				   faceHash.toUtf8() + "\"}";
		writeFile(QDir(objectDir).filePath(faceHash), "png", now);
	}
	writeFile(QDir(dirs.meta).filePath("net.minecraft/" + version + ".json"),
			  "{\"assetIndex\": {\"id\": \"" + indexId.toUtf8() + "\"}}", now);
	writeFile(QDir(dirs.assetIndexes).filePath(indexId + ".json"),
			  "{\"objects\": {" + objects + "}}", now);
	return QDir(objectDir).filePath(hash);
}
} // namespace

/* The cover tests build a data directory with a fake version metadata file,
 * asset index and asset object; the role tests use a minimal InstanceList +
 * NullInstance fixture and wait out the background lookup with QTRY_COMPARE.
 * A NullInstance has no Minecraft version, so the panorama step is checked on
 * panoramaPath()/pickCover() directly. */
class InstanceListTest : public QObject
{
	Q_OBJECT
  private slots:

	void panoramaPathFollowsMetaIndexAndObject()
	{
		QTemporaryDir data;
		QVERIFY(data.isValid());
		const InstanceList::CoverDirs dirs = coverDirsIn(data.path());
		const QString object = writePanorama(dirs, "1.21.1", "17", "ab12cd");

		QCOMPARE(InstanceList::panoramaPath(dirs, "1.21.1"), object);
		// Another version has no metadata file.
		QCOMPARE(InstanceList::panoramaPath(dirs, "1.20.1"), QString());
		QCOMPARE(InstanceList::panoramaPath(dirs, QString()), QString());
		QCOMPARE(InstanceList::panoramaPath({}, "1.21.1"), QString());
	}

	void panoramaPathPicksTheFaceAndFallsBackToTheFirst()
	{
		QTemporaryDir data;
		QVERIFY(data.isValid());
		const InstanceList::CoverDirs dirs = coverDirsIn(data.path());
		const QString first =
			writePanorama(dirs, "1.21.1", "17", "ab12cd", {0, 2});

		QCOMPARE(InstanceList::panoramaPath(dirs, "1.21.1", 0), first);
		QCOMPARE(InstanceList::panoramaPath(dirs, "1.21.1", 2),
				 first + QStringLiteral("2"));
		// Face 3 is not in the index: the first one stands in.
		QCOMPARE(InstanceList::panoramaPath(dirs, "1.21.1", 3), first);
	}

	void panoramaFaceIsStablePerInstanceAndSpread()
	{
		QSet<int> seen;
		for (const QString id : {"survival", "creative", "skyblock", "pvp",
								 "modded", "test", "new", "old"}) {
			const int face = InstanceList::panoramaFaceFor(id);
			QVERIFY(face >= 0 && face < InstanceList::kPanoramaFaces);
			QCOMPARE(InstanceList::panoramaFaceFor(id), face);
			seen.insert(face);
		}
		QVERIFY(seen.size() > 1);
	}

	void panoramaPathNeedsTheObjectOnDisk()
	{
		QTemporaryDir data;
		QVERIFY(data.isValid());
		const InstanceList::CoverDirs dirs = coverDirsIn(data.path());
		const QString object = writePanorama(dirs, "1.21.1", "17", "ab12cd");
		QVERIFY(QFile::remove(object));

		QCOMPARE(InstanceList::panoramaPath(dirs, "1.21.1"), QString());
	}

	void panoramaPathRefusesAVersionThatLeavesTheMetaFolder()
	{
		QTemporaryDir data;
		QVERIFY(data.isValid());
		const InstanceList::CoverDirs dirs = coverDirsIn(data.path());
		writePanorama(dirs, "1.21.1", "17", "ab12cd");

		QCOMPARE(InstanceList::panoramaPath(dirs, "../net.minecraft/1.21.1"),
				 QString());
	}

	void pickCoverPrefersCustomThenPanoramaThenNothing()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString custom = QDir(dir.path()).filePath("shot.png");
		const QString panorama = QDir(dir.path()).filePath("pano");
		QVERIFY(writeFile(custom, "x", QDateTime::currentDateTime()));
		QVERIFY(writeFile(panorama, "y", QDateTime::currentDateTime()));

		auto both = InstanceList::pickCover(custom, panorama);
		QCOMPARE(both.kind, QStringLiteral("custom"));
		QCOMPARE(both.url, QUrl::fromLocalFile(custom).toString());

		auto onlyPanorama = InstanceList::pickCover(QString(), panorama);
		QCOMPARE(onlyPanorama.kind, QStringLiteral("panorama"));
		QCOMPARE(onlyPanorama.url, QUrl::fromLocalFile(panorama).toString());

		// A picked screenshot that was deleted falls through.
		const QString gone = QDir(dir.path()).filePath("gone.png");
		QCOMPARE(InstanceList::pickCover(gone, panorama).kind,
				 QStringLiteral("panorama"));
		auto none = InstanceList::pickCover(gone, QString());
		QCOMPARE(none.kind, QString());
		QCOMPARE(none.url, QString());
	}

	void averageIconColorWeighsVisiblePixels()
	{
		QImage image(4, 4, QImage::Format_ARGB32);
		image.fill(QColor(0, 0, 255));
		QCOMPARE(InstanceList::averageIconColor(image), QColor(0, 0, 255));

		image.fill(Qt::transparent);
		QVERIFY(!InstanceList::averageIconColor(image).isValid());
		QVERIFY(!InstanceList::averageIconColor(QImage()).isValid());
	}

	void hasCrashedRoleReflectsInstanceState()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		const QString instRoot = QDir(instsDir.path()).filePath("testinst");
		QVERIFY(QDir().mkpath(instRoot));
		QVERIFY(writeFile(QDir(instRoot).filePath("instance.cfg"),
						   "InstanceType=NullTest\n",
						   QDateTime::currentDateTime()));

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);

		InstancePtr inst = list.getInstanceById("testinst");
		QVERIFY(inst);
		const QModelIndex idx = list.getInstanceIndexById("testinst");
		QVERIFY(idx.isValid());

		QCOMPARE(list.roleNames().value(InstanceList::HasCrashedRole),
				 QByteArray("hasCrashed"));
		QCOMPARE(list.data(idx, InstanceList::HasCrashedRole).toBool(),
				 false);

		QSignalSpy dataChangedSpy(&list, &InstanceList::dataChanged);
		inst->setCrashed(true);
		QVERIFY(!dataChangedSpy.isEmpty());
		QCOMPARE(list.data(idx, InstanceList::HasCrashedRole).toBool(), true);

		dataChangedSpy.clear();
		inst->setCrashed(false);
		QVERIFY(!dataChangedSpy.isEmpty());
		QCOMPARE(list.data(idx, InstanceList::HasCrashedRole).toBool(),
				 false);
	}

	void packArtRoleOnlyForPlatformPacks()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		const QString plainRoot = QDir(instsDir.path()).filePath("plain");
		const QString packRoot = QDir(instsDir.path()).filePath("pack");
		QVERIFY(QDir().mkpath(plainRoot));
		QVERIFY(QDir().mkpath(packRoot));
		QVERIFY(writeFile(QDir(plainRoot).filePath("instance.cfg"),
						   "InstanceType=NullTest\n",
						   QDateTime::currentDateTime()));
		QVERIFY(writeFile(QDir(packRoot).filePath("instance.cfg"),
						   "InstanceType=NullTest\n"
						   "PackProvider=modrinth\n"
						   "PackIconUrl=https://cdn.example.org/icon.png\n",
						   QDateTime::currentDateTime()));

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);

		QCOMPARE(list.roleNames().value(InstanceList::PackArtRole),
				 QByteArray("packArt"));
		const QModelIndex plain = list.getInstanceIndexById("plain");
		const QModelIndex pack = list.getInstanceIndexById("pack");
		QVERIFY(plain.isValid());
		QVERIFY(pack.isValid());
		QCOMPARE(list.data(plain, InstanceList::PackArtRole).toString(),
				 QString());
		QCOMPARE(list.data(pack, InstanceList::PackArtRole).toString(),
				 QStringLiteral("https://cdn.example.org/icon.png"));
	}

	void coverRolesFollowThePickedScreenshot()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		/* Canonicalized: InstanceList canonicalizes every instance root
		 * (resolving e.g. macOS's /var -> /private/var). */
		const QString instsRoot =
			QFileInfo(instsDir.path()).canonicalFilePath();
		const QString instRoot = QDir(instsRoot).filePath("testinst");
		QVERIFY(QDir().mkpath(instRoot));
		QVERIFY(writeFile(QDir(instRoot).filePath("instance.cfg"),
						   "InstanceType=NullTest\n",
						   QDateTime::currentDateTime()));
		QVERIFY(QDir().mkpath(QDir(instRoot).filePath("screenshots")));
		const QString shotPath =
			QDir(instRoot).filePath("screenshots/shot.png");
		QVERIFY(writeFile(shotPath, "x", QDateTime::currentDateTime()));
		const QString elsewhere = QDir(instRoot).filePath("elsewhere.png");
		QVERIFY(writeFile(elsewhere, "x", QDateTime::currentDateTime()));
		const QString notImage =
			QDir(instRoot).filePath("screenshots/notes.txt");
		QVERIFY(writeFile(notImage, "x", QDateTime::currentDateTime()));
		const QString expectedUrl = QUrl::fromLocalFile(shotPath).toString();

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);
		list.setCoverDirs({});
		const QModelIndex idx = list.getInstanceIndexById("testinst");
		QVERIFY(idx.isValid());
		QCOMPARE(list.roleNames().value(InstanceList::CoverImageRole),
				 QByteArray("coverImage"));
		QCOMPARE(list.roleNames().value(InstanceList::CoverKindRole),
				 QByteArray("coverKind"));

		// No screenshot is picked: no automatic cover, however many
		// screenshots the folder holds.
		QVERIFY(writeFile(QDir(instRoot).filePath("screenshots/newest.png"),
						   "x", QDateTime::currentDateTime()));
		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());
		QTest::qWait(100);
		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());
		QCOMPARE(list.data(idx, InstanceList::CoverKindRole).toString(),
				 QString());

		// Only an existing image can be picked, by path or by file URL.
		QVERIFY(!list.setCover("testinst", notImage));
		QVERIFY(!list.setCover("testinst",
							   QDir(instRoot).filePath("screenshots/no.png")));
		// Outside the screenshots folder a copy of the instance could not
		// find it again.
		QVERIFY(!list.setCover("testinst", elsewhere));
		QVERIFY(!list.setCover("nobody", shotPath));
		QSignalSpy changed(&list, &InstanceList::dataChanged);
		QVERIFY(list.setCover("testinst", QUrl::fromLocalFile(shotPath).toString()));
		QVERIFY(!changed.isEmpty());
		// Only the name is stored, not the home directory.
		QCOMPARE(list.getInstanceById("testinst")->coverImage(),
				 QStringLiteral("shot.png"));

		QTRY_COMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
					 expectedUrl);
		QCOMPARE(list.data(idx, InstanceList::CoverKindRole).toString(),
				 QStringLiteral("custom"));

		// Picking another screenshot replaces the cover.
		const QString otherPath =
			QDir(instRoot).filePath("screenshots/newest.png");
		QVERIFY(list.setCover("testinst", otherPath));
		QTRY_COMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
					 QUrl::fromLocalFile(otherPath).toString());

		list.clearCover("testinst");
		QVERIFY(list.getInstanceById("testinst")->coverImage().isEmpty());
		QTRY_COMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
					 QString());
		QCOMPARE(list.data(idx, InstanceList::CoverKindRole).toString(),
				 QString());
	}

	void coverFallsThroughWhenThePickedFileIsGone()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		const QString instsRoot =
			QFileInfo(instsDir.path()).canonicalFilePath();
		const QString instRoot = QDir(instsRoot).filePath("testinst");
		QVERIFY(QDir().mkpath(instRoot));
		const QString gone = QStringLiteral("deleted.png");
		QVERIFY(writeFile(QDir(instRoot).filePath("instance.cfg"),
						   ("InstanceType=NullTest\nCoverImage=" + gone +
							"\n").toUtf8(),
						   QDateTime::currentDateTime()));

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);
		list.setCoverDirs({});
		const QModelIndex idx = list.getInstanceIndexById("testinst");
		QVERIFY(idx.isValid());
		QCOMPARE(list.getInstanceById("testinst")->coverImage(), gone);

		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());
		QTest::qWait(100);
		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());
		QCOMPARE(list.data(idx, InstanceList::CoverKindRole).toString(),
				 QString());
	}
};

QTEST_GUILESS_MAIN(InstanceListTest)

#include "InstanceList_test.moc"
