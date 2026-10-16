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
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sstream>

#include <io/stream_reader.h>
#include <io/stream_writer.h>
#include <tag_compound.h>
#include <tag_list.h>
#include <tag_primitive.h>
#include <tag_string.h>

#include "models/ServersListModel.h"

namespace
{
QByteArray pngBytes()
{
	QImage image(64, 64, QImage::Format_ARGB32);
	image.fill(Qt::darkGreen);
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "PNG");
	return bytes;
}

// One servers.dat entry the way the game writes it, plus a tag this model
// knows nothing about.
void writeServersDat(const QString& path, const std::string& icon)
{
	nbt::tag_compound entry;
	entry.insert("name", std::string("Hypixel"));
	entry.insert("ip", std::string("mc.hypixel.net"));
	entry.insert("icon", icon);
	entry.insert("acceptTextures", nbt::tag_byte(1));
	entry.insert("hidden", nbt::tag_byte(0));
	nbt::tag_list list;
	list.push_back(std::move(entry));
	nbt::tag_compound root;
	root.insert("servers", nbt::value(std::move(list)));
	std::ostringstream out;
	nbt::io::write_tag("", root, out);
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.write(out.str().data(), static_cast<qint64>(out.str().size()));
}

std::unique_ptr<nbt::tag_compound> readFirstServer(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return nullptr;
	}
	const QByteArray bytes = file.readAll();
	std::istringstream in(std::string(bytes.constData(), bytes.size()));
	auto root = nbt::io::read_compound(in).second;
	auto& list = root->at("servers").as<nbt::tag_list>();
	return std::make_unique<nbt::tag_compound>(
		list.at(0).as<nbt::tag_compound>());
}
} // namespace

/* Needs neither a BaseInstance nor a LauncherContext - only a directory to
 * read/write servers.dat under, same reason OtherLogsModel_test.cpp drives
 * that model directly against a temp directory instead of a real
 * instance. */
class ServersListModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void addEditMoveRemove();
	void persistsAcrossReload();
	void lockedRefusesEdits();
	void watchesDirectoryOnlyWhileLocked();
	void addressOfOutOfRange();
	void exposesStoredFavicon();
	void acceptsPrefixedFavicon();
	void ignoresFaviconThatIsNotPng();
	void editsKeepTagsItDoesNotEdit();
};

void ServersListModelTest::addEditMoveRemove()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	QCOMPARE(model.rowCount(), 0);

	const int first = model.addServer();
	QCOMPARE(first, 0);
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Minecraft Server"));

	model.setName(0, QStringLiteral("Home"));
	model.setAddress(0, QStringLiteral("home.example.com:25565"));
	model.setAcceptTextures(0, 1);

	const int second = model.addServer();
	QCOMPARE(second, 1);
	model.setName(1, QStringLiteral("Away"));
	model.setAddress(1, QStringLiteral("away.example.com"));

	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Home"));
	QCOMPARE(
		model.data(model.index(0), ServersListModel::AddressRole).toString(),
		QStringLiteral("home.example.com:25565"));
	QCOMPARE(
		model.data(model.index(0), ServersListModel::AcceptTexturesRole).toInt(),
		1);

	// Move "Away" (row 1) up in front of "Home".
	QVERIFY(model.moveUp(1));
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Away"));
	QCOMPARE(model.data(model.index(1), ServersListModel::NameRole).toString(),
			QStringLiteral("Home"));

	QCOMPARE(model.addressOf(1), QStringLiteral("home.example.com:25565"));

	QVERIFY(model.removeServer(0));
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Home"));
}

void ServersListModelTest::persistsAcrossReload()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	{
		ServersListModel model(dir.path());
		model.startWatching();
		model.addServer();
		model.setName(0, QStringLiteral("Persisted"));
		model.setAddress(0, QStringLiteral("persisted.example.com:1234"));
		model.setAcceptTextures(0, 2);
		model.stopWatching();
		// stopWatching() flushes the pending save synchronously - the
		// destructor below would too, but this checks that specifically.
	}

	QVERIFY(QFile::exists(dir.filePath(QStringLiteral("servers.dat"))));

	ServersListModel reloaded(dir.path());
	reloaded.startWatching();
	QCOMPARE(reloaded.rowCount(), 1);
	QCOMPARE(
		reloaded.data(reloaded.index(0), ServersListModel::NameRole).toString(),
		QStringLiteral("Persisted"));
	QCOMPARE(reloaded.data(reloaded.index(0), ServersListModel::AddressRole)
				.toString(),
			QStringLiteral("persisted.example.com:1234"));
	QCOMPARE(reloaded
				.data(reloaded.index(0),
					 ServersListModel::AcceptTexturesRole)
				.toInt(),
			2);
}

void ServersListModelTest::lockedRefusesEdits()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	model.addServer();
	model.setName(0, QStringLiteral("Original"));

	QSignalSpy lockedSpy(&model, &ServersListModel::lockedChanged);
	model.setLocked(true);
	QCOMPARE(lockedSpy.count(), 1);
	QVERIFY(model.locked());

	// Every mutation is a no-op while locked.
	QCOMPARE(model.addServer(), -1);
	model.setName(0, QStringLiteral("Changed"));
	model.setAddress(0, QStringLiteral("changed.example.com"));
	QVERIFY(!model.removeServer(0));
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Original"));

	model.setLocked(false);
	QVERIFY(!model.locked());
	model.setName(0, QStringLiteral("Changed"));
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Changed"));
}

void ServersListModelTest::watchesDirectoryOnlyWhileLocked()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	model.addServer();
	model.setName(0, QStringLiteral("Unsaved edit"));

	QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

	// Unlocked and observed: the user may be mid-edit, so a change in the
	// directory (here a stray file, standing in for the game or another
	// launcher touching it) must not reload the list underneath them. The
	// widget's ServersModel watched only while locked for this reason.
	{
		QFile stray(dir.filePath(QStringLiteral("stray-1.txt")));
		QVERIFY(stray.open(QIODevice::WriteOnly));
		stray.write("x");
	}
	QTest::qWait(500);
	QCOMPARE(resetSpy.count(), 0);
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Unsaved edit"));

	// Locking (the game started) flushes the edit to disk and starts the
	// watch: from here an external change is picked up, as a positive
	// control that the watch really works in this environment.
	model.setLocked(true);
	{
		QFile stray(dir.filePath(QStringLiteral("stray-2.txt")));
		QVERIFY(stray.open(QIODevice::WriteOnly));
		stray.write("x");
	}
	QTRY_VERIFY_WITH_TIMEOUT(resetSpy.count() >= 1, 5000);
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Unsaved edit"));

	// Unlocking (the game closed) drops the watch again.
	model.setLocked(false);
	// Let anything the engine had already queued from the locked phase
	// arrive before taking the baseline.
	QTest::qWait(300);
	const int resetsBefore = resetSpy.count();
	{
		QFile stray(dir.filePath(QStringLiteral("stray-3.txt")));
		QVERIFY(stray.open(QIODevice::WriteOnly));
		stray.write("x");
	}
	QTest::qWait(500);
	QCOMPARE(resetSpy.count(), resetsBefore);
}

void ServersListModelTest::addressOfOutOfRange()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	QCOMPARE(model.addressOf(-1), QString());
	QCOMPARE(model.addressOf(0), QString());
}

void ServersListModelTest::exposesStoredFavicon()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	writeServersDat(dir.filePath("servers.dat"),
					pngBytes().toBase64().toStdString());

	ServersListModel model(dir.path());
	model.startWatching();
	QCOMPARE(model.rowCount(), 1);
	const QString url =
		model.data(model.index(0), ServersListModel::IconUrlRole).toString();
	QVERIFY(url.startsWith(QStringLiteral("data:image/png;base64,")));
	const QImage decoded = QImage::fromData(
		QByteArray::fromBase64(url.section(QLatin1Char(','), 1).toLatin1()));
	QCOMPARE(decoded.size(), QSize(64, 64));
	QCOMPARE(model.roleNames().value(ServersListModel::IconUrlRole),
			 QByteArray("iconUrl"));

	// A server added here has never been pinged.
	model.addServer();
	QVERIFY(model.data(model.index(1), ServersListModel::IconUrlRole)
				.toString()
				.isEmpty());
}

void ServersListModelTest::acceptsPrefixedFavicon()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	writeServersDat(dir.filePath("servers.dat"),
					"data:image/png;base64," +
						pngBytes().toBase64().toStdString());

	ServersListModel model(dir.path());
	model.startWatching();
	const QString url =
		model.data(model.index(0), ServersListModel::IconUrlRole).toString();
	QCOMPARE(url, QStringLiteral("data:image/png;base64,") +
					  QString::fromLatin1(pngBytes().toBase64()));
}

void ServersListModelTest::ignoresFaviconThatIsNotPng()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	writeServersDat(dir.filePath("servers.dat"),
					QByteArray("not an image").toBase64().toStdString());

	ServersListModel model(dir.path());
	model.startWatching();
	QCOMPARE(model.rowCount(), 1);
	QVERIFY(model.data(model.index(0), ServersListModel::IconUrlRole)
				.toString()
				.isEmpty());
}

void ServersListModelTest::editsKeepTagsItDoesNotEdit()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath("servers.dat");
	const std::string icon = pngBytes().toBase64().toStdString();
	writeServersDat(path, icon);

	{
		ServersListModel model(dir.path());
		model.startWatching();
		model.setName(0, QStringLiteral("Renamed"));
		model.setAcceptTextures(0, 0);
		model.stopWatching();
	}

	const auto saved = readFirstServer(path);
	QVERIFY(saved);
	QCOMPARE(QString::fromStdString(
				 saved->at("name").as<nbt::tag_string>().get()),
			 QStringLiteral("Renamed"));
	QCOMPARE(QString::fromStdString(
				 saved->at("icon").as<nbt::tag_string>().get()),
			 QString::fromStdString(icon));
	QVERIFY(saved->has_key("hidden", nbt::tag_type::Byte));
	// "Ask" is the absence of the tag, as the game writes it.
	QVERIFY(!saved->has_key("acceptTextures"));
}

QTEST_GUILESS_MAIN(ServersListModelTest)
#include "ServersListModel_test.moc"
