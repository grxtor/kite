/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include <QSet>
#include <QTest>

#include "icons/AutoIcons.h"

class AutoIconsTest : public QObject
{
	Q_OBJECT

	static QList<AutoIcons::Instance> generic(const QStringList& ids)
	{
		QList<AutoIcons::Instance> list;
		for (const QString& id : ids)
			list.append({id, QStringLiteral("default")});
		return list;
	}

  private slots:
	/// Every generic instance gets an icon from the pool, none twice.
	void distinct()
	{
		const QStringList ids = {"26.1.2", "26.3", "26.3 Fabric", "26.3 NeoForge",
								 "Kite Performans 26.3", "MC-26.2-Meteor",
								 "MC-26.3-Animations", "Minecraft 26.3"};
		const auto map = AutoIcons::assign(generic(ids), {}, AutoIcons::pool());
		QCOMPARE(map.size(), ids.size());
		QSet<QString> seen;
		for (const QString& key : map) {
			QVERIFY(AutoIcons::pool().contains(key));
			QVERIFY2(!seen.contains(key), qPrintable(key));
			seen.insert(key);
		}
	}

	/// A picked icon (or a pack's own logo) is left alone; "", the grass
	/// block and a bare platform logo count as generic.
	void pickedIconsAreSkipped()
	{
		const QList<AutoIcons::Instance> list = {{"a", "creeper"}, {"b", ""},		   {"c", "default"},
												 {"d", "flame"},   {"e", "modrinth"}, {"f", "modrinth_fresh-smooth"}};
		const auto map = AutoIcons::assign(list, {}, AutoIcons::pool());
		QVERIFY(!map.contains("a"));
		QVERIFY(map.contains("b"));
		QVERIFY(map.contains("c"));
		QVERIFY(map.contains("d"));
		QVERIFY(map.contains("e"));
		QVERIFY(!map.contains("f"));
	}

	/// What was given stays given when another instance arrives, even one
	/// whose hashed slot is the same icon.
	void earlierIconsStay()
	{
		const auto before = AutoIcons::assign(generic({"x", "y"}), {}, AutoIcons::pool());
		// "a" sorts first, so without the kept map it would claim slots first.
		const auto after = AutoIcons::assign(generic({"a", "x", "y"}), before, AutoIcons::pool());
		QCOMPARE(after.value("x"), before.value("x"));
		QCOMPARE(after.value("y"), before.value("y"));
		QVERIFY(after.value("a") != after.value("x"));
		QVERIFY(after.value("a") != after.value("y"));
	}

	/// Removed instances, instances that got an icon, and icons no longer in
	/// the pool drop out of the map.
	void staleEntriesDrop()
	{
		const QHash<QString, QString> previous = {{"gone", "bee"}, {"picked", "fox"}, {"old", "nosuchicon"}};
		const QList<AutoIcons::Instance> list = {{"picked", "creeper"}, {"old", "default"}};
		const auto map = AutoIcons::assign(list, previous, AutoIcons::pool());
		QVERIFY(!map.contains("gone"));
		QVERIFY(!map.contains("picked"));
		QVERIFY(AutoIcons::pool().contains(map.value("old")));
	}

	/// The same library gets the same icons on every run.
	void deterministic()
	{
		const QStringList ids = {"one", "two", "three"};
		QCOMPARE(AutoIcons::assign(generic(ids), {}, AutoIcons::pool()),
				 AutoIcons::assign(generic(ids), {}, AutoIcons::pool()));
	}

	/// More generic instances than icons: everyone still gets one.
	void overflowShares()
	{
		const QStringList pool = {"bee", "fox"};
		const auto map = AutoIcons::assign(generic({"a", "b", "c"}), {}, pool);
		QCOMPARE(map.size(), 3);
		QCOMPARE(QSet<QString>(map.begin(), map.end()).size(), 2);
	}

	void jsonRoundTrip()
	{
		const QHash<QString, QString> map = {{"Kite Performans 26.3", "creeper"}, {"26.3", "bee"}};
		QCOMPARE(AutoIcons::fromJson(AutoIcons::toJson(map)), map);
		QVERIFY(AutoIcons::fromJson("not json").isEmpty());
	}
};

QTEST_GUILESS_MAIN(AutoIconsTest)

#include "AutoIcons_test.moc"
