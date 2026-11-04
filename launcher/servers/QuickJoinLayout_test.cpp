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
#include <QTemporaryDir>
#include <QTest>

#include "servers/FeaturedServers.h"
#include "servers/QuickJoinLayout.h"
#include "settings/INISettingsObject.h"

namespace
{
	QByteArray listOf(const QStringList& ids)
	{
		QStringList servers;
		for (const QString& id : ids) {
			servers.append(QStringLiteral(R"({ "id": "%1", "name": "%1 Network", "address": "play.%1.example" })")
							   .arg(id));
		}
		return QStringLiteral(R"({ "formatVersion": 1, "servers": [%1] })")
			.arg(servers.join(", "))
			.toUtf8();
	}

	QVariantList featuredOf(const QStringList& ids)
	{
		FeaturedServers featured;
		featured.loadFromJson(listOf(ids));
		return featured.servers();
	}

	QStringList idsOf(const QVariantList& entries)
	{
		QStringList ids;
		for (const QVariant& entry : entries) {
			ids.append(entry.toMap().value("id").toString());
		}
		return ids;
	}

	QStringList shownIdsOf(const QVariantList& entries)
	{
		QStringList ids;
		for (const QVariant& entry : entries) {
			if (!entry.toMap().value("hidden").toBool()) {
				ids.append(entry.toMap().value("id").toString());
			}
		}
		return ids;
	}

	SettingsObjectPtr settingsIn(const QTemporaryDir& dir)
	{
		auto settings = std::make_shared<INISettingsObject>(dir.filePath("kite.cfg"));
		settings->registerSetting(QuickJoinLayout::kLayoutSetting, QString());
		settings->registerSetting(QuickJoinLayout::kHintSetting, false);
		return settings;
	}

	const QString kOwnAddress = QStringLiteral("mc.friends.example");
	const QString kOwnId = QStringLiteral("own:mc.friends.example:25565");
} // namespace

class QuickJoinLayoutTest : public QObject
{
	Q_OBJECT

  private slots:
	void anEmptyLayoutIsTheBigServersAsTheyCome()
	{
		const QVariantList entries = QuickJoinLayout::merge(featuredOf({"a", "b", "c"}), {});
		QCOMPARE(idsOf(entries), QStringList({"a", "b", "c"}));
		const QVariantMap first = entries.first().toMap();
		QVERIFY(first.value("featured").toBool());
		QVERIFY(!first.value("own").toBool());
		QVERIFY(!first.value("hidden").toBool());
		QCOMPARE(first.value("address").toString(), QString("play.a.example"));
	}

	void aNewBigServerComesAtTheEnd()
	{
		QuickJoinLayout::Layout layout;
		layout.order = {"c", "a", "b"};
		const QVariantList entries = QuickJoinLayout::merge(featuredOf({"new", "a", "b", "c"}), layout);
		QCOMPARE(idsOf(entries), QStringList({"c", "a", "b", "new"}));
	}

	void aHiddenBigServerStaysHidden()
	{
		QuickJoinLayout::Layout layout;
		layout.order = {"a", "b", "c"};
		layout.hidden = {"b"};
		// The list changed around it: still hidden, still in its place.
		const QVariantList entries = QuickJoinLayout::merge(featuredOf({"c", "b", "a", "d"}), layout);
		QCOMPARE(idsOf(entries), QStringList({"a", "b", "c", "d"}));
		QCOMPARE(shownIdsOf(entries), QStringList({"a", "c", "d"}));
	}

	void aBigServerGoneFromTheListGoes()
	{
		QuickJoinLayout::Layout layout;
		layout.order = {"a", "gone", "b"};
		layout.hidden = {"gone"};
		const QVariantList entries = QuickJoinLayout::merge(featuredOf({"a", "b"}), layout);
		QCOMPARE(idsOf(entries), QStringList({"a", "b"}));
	}

	void theirOwnServersStayWhereTheyPutThem()
	{
		QuickJoinLayout::Layout layout;
		layout.order = {"a", kOwnId, "b"};
		layout.own = {{kOwnId, "Friends", kOwnAddress}};
		const QVariantList entries = QuickJoinLayout::merge(featuredOf({"a", "b"}), layout);
		QCOMPARE(idsOf(entries), QStringList({"a", kOwnId, "b"}));
		const QVariantMap own = entries.at(1).toMap();
		QVERIFY(own.value("own").toBool());
		QVERIFY(!own.value("featured").toBool());
		QCOMPARE(own.value("name").toString(), QString("Friends"));
		QCOMPARE(own.value("address").toString(), kOwnAddress);
		// It takes any Minecraft.
		QCOMPARE(own.value("minVersion").toString(), QString());
		QCOMPARE(own.value("versions").toStringList(), QStringList());
	}

	void aBigServerPinnedAsTheirOwnOutlivesTheList()
	{
		const QString pinned = QuickJoinLayout::ownId("play.b.example");
		QuickJoinLayout::Layout layout;
		layout.order = {"a", "b", pinned, "c"};
		layout.own = {{pinned, "b Network", "play.b.example"}};
		// While the list has it, it is the big server, once, marked as theirs.
		QVariantList entries = QuickJoinLayout::merge(featuredOf({"a", "b", "c"}), layout);
		QCOMPARE(idsOf(entries), QStringList({"a", "b", "c"}));
		QVERIFY(entries.at(1).toMap().value("own").toBool());
		QVERIFY(entries.at(1).toMap().value("featured").toBool());
		// Once it is gone, it stays as theirs, in the same place.
		entries = QuickJoinLayout::merge(featuredOf({"a", "c"}), layout);
		QCOMPARE(idsOf(entries), QStringList({"a", pinned, "c"}));
		QCOMPARE(entries.at(1).toMap().value("name").toString(), QString("b Network"));
	}

	void theSettingReadsBackAsItWasWritten()
	{
		QuickJoinLayout::Layout layout;
		layout.order = {"b", kOwnId, "a"};
		layout.hidden = {"a"};
		layout.own = {{kOwnId, "Friends", kOwnAddress}};
		const QuickJoinLayout::Layout back =
			QuickJoinLayout::parse(QuickJoinLayout::serialize(layout));
		QCOMPARE(back.order, layout.order);
		QCOMPARE(back.hidden, layout.hidden);
		QCOMPARE(back.own.size(), 1);
		QCOMPARE(back.own.first().id, kOwnId);
		QCOMPARE(back.own.first().name, QString("Friends"));
		QCOMPARE(back.own.first().address, kOwnAddress);
		// Unreadable is the default.
		QVERIFY(QuickJoinLayout::parse("not json").order.isEmpty());
		QVERIFY(QuickJoinLayout::parse(QString()).own.isEmpty());
	}

	void moveSkipsTheHidden()
	{
		QuickJoinLayout::Layout layout;
		layout.order = {"a", "b", "c", "d"};
		layout.hidden = {"b"};
		const QVariantList entries = QuickJoinLayout::merge(featuredOf({"a", "b", "c", "d"}), layout);
		QCOMPARE(idsOf(QuickJoinLayout::moveShown(entries, "d", 0)), QStringList({"d", "a", "b", "c"}));
		// Shown are c, d once a is taken out; past the last is the end.
		QCOMPARE(idsOf(QuickJoinLayout::moveShown(entries, "a", 2)), QStringList({"b", "c", "d", "a"}));
		QCOMPARE(idsOf(QuickJoinLayout::moveShown(entries, "a", 1)), QStringList({"b", "c", "a", "d"}));
		QCOMPARE(idsOf(QuickJoinLayout::moveShown(entries, "unknown", 0)), idsOf(entries));
	}

	void aDraggedOrderIsKeptAcrossStarts()
	{
		QTemporaryDir dir;
		FeaturedServers featured;
		QVERIFY(featured.loadFromJson(listOf({"a", "b", "c"})));
		{
			QuickJoinLayout bar(&featured, settingsIn(dir));
			QSignalSpy changed(&bar, &QuickJoinLayout::changed);
			bar.move("c", 0);
			QCOMPARE(changed.count(), 1);
			QCOMPARE(idsOf(bar.shown()), QStringList({"c", "a", "b"}));
		}
		// The next start reads the same order from the file.
		QuickJoinLayout again(&featured, settingsIn(dir));
		QCOMPARE(idsOf(again.shown()), QStringList({"c", "a", "b"}));
		// A new big server joins at the end of it.
		QVERIFY(featured.loadFromJson(listOf({"new", "a", "b", "c"})));
		QCOMPARE(idsOf(again.shown()), QStringList({"c", "a", "b", "new"}));
	}

	void removeHidesABigServerAndDropsAnOwnOne()
	{
		QTemporaryDir dir;
		FeaturedServers featured;
		QVERIFY(featured.loadFromJson(listOf({"a", "b"})));
		QuickJoinLayout bar(&featured, settingsIn(dir));
		QVERIFY(bar.pin("Friends", kOwnAddress));
		QCOMPARE(idsOf(bar.shown()), QStringList({"a", "b", kOwnId}));
		QVERIFY(bar.isShown(kOwnAddress));

		bar.remove("a");
		QCOMPARE(idsOf(bar.shown()), QStringList({"b", kOwnId}));
		QCOMPARE(idsOf(bar.entries()), QStringList({"a", "b", kOwnId}));

		bar.remove(kOwnId);
		QCOMPARE(idsOf(bar.entries()), QStringList({"a", "b"}));
		QVERIFY(!bar.isShown(kOwnAddress));
	}

	void pinningAHiddenBigServerShowsItAsTheirs()
	{
		QTemporaryDir dir;
		FeaturedServers featured;
		QVERIFY(featured.loadFromJson(listOf({"a", "b"})));
		QuickJoinLayout bar(&featured, settingsIn(dir));
		bar.remove("b");
		QVERIFY(!bar.isShown("PLAY.B.example:25565"));
		// Another spelling of the same address is the same server.
		QVERIFY(bar.pin(QString(), "play.b.example."));
		QCOMPARE(idsOf(bar.shown()), QStringList({"a", "b"}));
		QVERIFY(bar.entries().at(1).toMap().value("own").toBool());
		// Gone from the list, it stays.
		QVERIFY(featured.loadFromJson(listOf({"a"})));
		QCOMPARE(idsOf(bar.shown()), QStringList({"a", QuickJoinLayout::ownId("play.b.example")}));
		QVERIFY(!bar.pin("Nothing", "   "));
	}

	void anOwnServerWithoutANameShowsItsAddress()
	{
		QTemporaryDir dir;
		FeaturedServers featured;
		QVERIFY(featured.loadFromJson(listOf({"a"})));
		QuickJoinLayout bar(&featured, settingsIn(dir));
		QVERIFY(bar.pin("  ", " mc.friends.example "));
		const QVariantMap own = bar.shown().last().toMap();
		QCOMPARE(own.value("name").toString(), kOwnAddress);
		QCOMPARE(own.value("address").toString(), kOwnAddress);
	}

	void theEditDialogSavesAndResets()
	{
		QTemporaryDir dir;
		FeaturedServers featured;
		QVERIFY(featured.loadFromJson(listOf({"a", "b", "c"})));
		QuickJoinLayout bar(&featured, settingsIn(dir));
		QVERIFY(bar.pin("Friends", kOwnAddress));

		// The dialog's copy: reordered, b hidden, their own server hidden too.
		QVariantList edited;
		for (const QString& id : {kOwnId, QString("c"), QString("b"), QString("a")}) {
			for (const QVariant& entry : bar.entries()) {
				QVariantMap map = entry.toMap();
				if (map.value("id").toString() == id) {
					map.insert("hidden", id == "b" || id == kOwnId);
					edited.append(map);
				}
			}
		}
		bar.apply(edited);
		QCOMPARE(idsOf(bar.entries()), QStringList({kOwnId, "c", "b", "a"}));
		QCOMPARE(idsOf(bar.shown()), QStringList({"c", "a"}));

		// "Reset to default" is the big servers as they come; Save keeps it.
		QCOMPARE(idsOf(bar.defaults()), QStringList({"a", "b", "c"}));
		bar.apply(bar.defaults());
		QCOMPARE(idsOf(bar.shown()), QStringList({"a", "b", "c"}));
	}

	void theHintIsSeenOnce()
	{
		QTemporaryDir dir;
		FeaturedServers featured;
		auto settings = settingsIn(dir);
		QuickJoinLayout bar(&featured, settings);
		QVERIFY(!bar.hintSeen());
		bar.dismissHint();
		QVERIFY(bar.hintSeen());
		QuickJoinLayout again(&featured, settingsIn(dir));
		QVERIFY(again.hintSeen());
	}
};

QTEST_GUILESS_MAIN(QuickJoinLayoutTest)

#include "QuickJoinLayout_test.moc"
