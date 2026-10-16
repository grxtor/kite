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

  private:
	QTemporaryDir m_dir;
	QString m_cache;
	int m_run = 0;
};

QTEST_GUILESS_MAIN(LocalModParseTaskTest)

#include "LocalModParseTask_test.moc"
