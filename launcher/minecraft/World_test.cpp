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

#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <optional>

#include "GZip.h"
#include "minecraft/World.h"

// Minimal big-endian NBT writer, so the tests can build level.dat files
// without depending on how the launcher serializes them.
namespace
{
	const quint8 TAG_END = 0;
	const quint8 TAG_INT = 3;
	const quint8 TAG_LONG = 4;
	const quint8 TAG_STRING = 8;
	const quint8 TAG_COMPOUND = 10;

	void putU8(QByteArray& out, quint8 value)
	{
		out.append(static_cast<char>(value));
	}

	void putU16(QByteArray& out, quint16 value)
	{
		out.append(static_cast<char>((value >> 8) & 0xFF));
		out.append(static_cast<char>(value & 0xFF));
	}

	void putI32(QByteArray& out, qint32 value)
	{
		for (int shift = 24; shift >= 0; shift -= 8) {
			out.append(static_cast<char>((value >> shift) & 0xFF));
		}
	}

	void putI64(QByteArray& out, qint64 value)
	{
		for (int shift = 56; shift >= 0; shift -= 8) {
			out.append(static_cast<char>((value >> shift) & 0xFF));
		}
	}

	void putString(QByteArray& out, const QByteArray& value)
	{
		putU16(out, static_cast<quint16>(value.size()));
		out.append(value);
	}

	void putTagHeader(QByteArray& out, quint8 type, const QByteArray& name)
	{
		putU8(out, type);
		putString(out, name);
	}

	void putStringTag(QByteArray& out, const QByteArray& name,
					  const QByteArray& value)
	{
		putTagHeader(out, TAG_STRING, name);
		putString(out, value);
	}

	void putIntTag(QByteArray& out, const QByteArray& name, qint32 value)
	{
		putTagHeader(out, TAG_INT, name);
		putI32(out, value);
	}

	void putLongTag(QByteArray& out, const QByteArray& name, qint64 value)
	{
		putTagHeader(out, TAG_LONG, name);
		putI64(out, value);
	}

	struct LevelDatSpec {
		bool worldGenSettingsSeed = false;
		qint64 worldGenSettingsSeedValue = 0;
		bool randomSeed = false;
		qint64 randomSeedValue = 0;
		bool time = false;
		qint64 timeValue = 0;
		// Some tags have changed width between versions, so both are read
		bool timeAsInt = false;
		bool dayTime = false;
		qint64 dayTimeValue = 0;
		// Added in 1.9, so its absence marks a genuinely old world
		bool dataVersion = false;
		qint32 dataVersionValue = 0;
	};

	QByteArray makeLevelDat(const LevelDatSpec& spec)
	{
		QByteArray dataPayload;
		putStringTag(dataPayload, "LevelName", "Test World");
		putLongTag(dataPayload, "LastPlayed", Q_INT64_C(1600000000000));
		putIntTag(dataPayload, "GameType", 1);
		if (spec.time) {
			if (spec.timeAsInt) {
				putIntTag(dataPayload, "Time",
						  static_cast<qint32>(spec.timeValue));
			} else {
				putLongTag(dataPayload, "Time", spec.timeValue);
			}
		}
		if (spec.dayTime) {
			putLongTag(dataPayload, "DayTime", spec.dayTimeValue);
		}
		if (spec.dataVersion) {
			putIntTag(dataPayload, "DataVersion", spec.dataVersionValue);
		}
		if (spec.worldGenSettingsSeed) {
			putTagHeader(dataPayload, TAG_COMPOUND, "WorldGenSettings");
			putLongTag(dataPayload, "seed", spec.worldGenSettingsSeedValue);
			putU8(dataPayload, TAG_END);
		}
		if (spec.randomSeed) {
			putLongTag(dataPayload, "RandomSeed", spec.randomSeedValue);
		}
		putU8(dataPayload, TAG_END);

		QByteArray root;
		putTagHeader(root, TAG_COMPOUND, ""); // unnamed root compound
		putTagHeader(root, TAG_COMPOUND, "Data");
		root.append(dataPayload);
		putU8(root, TAG_END);
		return root;
	}

	// Newer versions keep the seed in
	// <world>/data/minecraft/world_gen_settings.dat, in a "data" compound.
	QByteArray makeWorldGenSettings(qint64 seed)
	{
		QByteArray dataPayload;
		putLongTag(dataPayload, "seed", seed);
		putU8(dataPayload, TAG_END);

		QByteArray root;
		putTagHeader(root, TAG_COMPOUND, ""); // unnamed root compound
		putTagHeader(root, TAG_COMPOUND, "data");
		root.append(dataPayload);
		putIntTag(root, "DataVersion", 4771);
		putU8(root, TAG_END);
		return root;
	}

	// 26.1+ keeps the daylight clock in
	// <world>/data/minecraft/world_clocks.dat: one compound per dimension id
	// with its total ticks. Shape from a real 26.2 world.
	QByteArray makeWorldClocks(std::optional<qint64> overworldTicks,
							   qint64 endTicks = 4728484)
	{
		QByteArray dataPayload;
		if (overworldTicks) {
			putTagHeader(dataPayload, TAG_COMPOUND, "minecraft:overworld");
			putLongTag(dataPayload, "total_ticks", *overworldTicks);
			putU8(dataPayload, TAG_END);
		}
		putTagHeader(dataPayload, TAG_COMPOUND, "minecraft:the_end");
		putLongTag(dataPayload, "total_ticks", endTicks);
		putU8(dataPayload, TAG_END);
		putU8(dataPayload, TAG_END);

		QByteArray root;
		putTagHeader(root, TAG_COMPOUND, ""); // unnamed root compound
		putTagHeader(root, TAG_COMPOUND, "data");
		root.append(dataPayload);
		putIntTag(root, "DataVersion", 4903);
		putU8(root, TAG_END);
		return root;
	}

	bool writeDat(const QString& worldPath, const QString& relativePath,
				  const QByteArray& nbt)
	{
		QDir worldDir(worldPath);
		if (!worldDir.mkpath(QFileInfo(relativePath).path())) {
			return false;
		}
		QByteArray compressed;
		if (!GZip::zip(nbt, compressed)) {
			return false;
		}
		QFile file(worldDir.absoluteFilePath(relativePath));
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(compressed) == compressed.size();
	}

	bool writeWorld(const QString& worldPath, const QByteArray& levelDat)
	{
		if (!QDir().mkpath(worldPath)) {
			return false;
		}
		return writeDat(worldPath, "level.dat", levelDat);
	}

	bool writeWorldGenSettings(const QString& worldPath, const QByteArray& nbt)
	{
		return writeDat(worldPath, "data/minecraft/world_gen_settings.dat",
						nbt);
	}

	bool writeWorldClocks(const QString& worldPath, const QByteArray& nbt)
	{
		return writeDat(worldPath, "data/minecraft/world_clocks.dat", nbt);
	}
} // namespace

class WorldTest : public QObject
{
	Q_OBJECT
  private slots:

	// Minecraft 1.16 and newer: Data -> WorldGenSettings -> seed
	void test_ReadSeedFromWorldGenSettings()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("modern");

		LevelDatSpec spec;
		spec.worldGenSettingsSeed = true;
		spec.worldGenSettingsSeedValue = Q_INT64_C(-4172144997902289642);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.name(), QString("Test World"));
		QCOMPARE(world.seed(), Q_INT64_C(-4172144997902289642));
	}

	// Legacy worlds: Data -> RandomSeed
	void test_ReadSeedFromRandomSeed()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("legacy");

		LevelDatSpec spec;
		spec.randomSeed = true;
		spec.randomSeedValue = Q_INT64_C(1234567890123456789);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.seed(), Q_INT64_C(1234567890123456789));
	}

	// If both are present, the modern location wins
	void test_WorldGenSettingsTakesPrecedence()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("both");

		LevelDatSpec spec;
		spec.worldGenSettingsSeed = true;
		spec.worldGenSettingsSeedValue = Q_INT64_C(42);
		spec.randomSeed = true;
		spec.randomSeedValue = Q_INT64_C(1337);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.seed(), Q_INT64_C(42));
	}

	// Newer Minecraft: the seed lives in world_gen_settings.dat
	void test_ReadSeedFromWorldGenSettingsFile()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("split");

		QVERIFY(writeWorld(worldPath, makeLevelDat(LevelDatSpec())));
		QVERIFY(writeWorldGenSettings(
			worldPath, makeWorldGenSettings(Q_INT64_C(-8974235917123456))));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.name(), QString("Test World"));
		QCOMPARE(world.seed(), Q_INT64_C(-8974235917123456));
	}

	// A seed inside level.dat is authoritative, the extra file is not read
	void test_LevelDatSeedTakesPrecedenceOverWorldGenSettingsFile()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("mixed");

		LevelDatSpec spec;
		spec.worldGenSettingsSeed = true;
		spec.worldGenSettingsSeedValue = Q_INT64_C(99);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));
		QVERIFY(writeWorldGenSettings(worldPath,
									  makeWorldGenSettings(Q_INT64_C(1337))));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.seed(), Q_INT64_C(99));
	}

	// A corrupt world_gen_settings.dat must not break loading the world
	void test_CorruptWorldGenSettingsFileIsIgnored()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("corrupt");

		QVERIFY(writeWorld(worldPath, makeLevelDat(LevelDatSpec())));
		QVERIFY(QDir(worldPath).mkpath("data/minecraft"));
		QFile garbage(QDir(worldPath).absoluteFilePath(
			"data/minecraft/world_gen_settings.dat"));
		QVERIFY(garbage.open(QIODevice::WriteOnly));
		QVERIFY(garbage.write("not a gzipped nbt file") > 0);
		garbage.close();

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.seed(), Q_INT64_C(0));
	}

	// No seed anywhere: the world still loads, the seed is just unknown
	void test_MissingSeedIsZero()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("seedless");

		QVERIFY(writeWorld(worldPath, makeLevelDat(LevelDatSpec())));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.seed(), Q_INT64_C(0));
	}

	// Day count: the daylight cycle (Data -> DayTime) at 24000 ticks per day.
	// Values from a real 1.13.2 world, on day 5 after three days of ticks.
	void test_ReadDayCountFromDayTime()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("days");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 1631; // 1.13.2
		spec.time = true;
		spec.timeValue = Q_INT64_C(91449);
		spec.dayTime = true;
		spec.dayTimeValue = Q_INT64_C(124712);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(5));
	}

	// Sleeping jumps DayTime to morning while Time only counts ticks really
	// spent; the player sees DayTime.
	void test_DayCountPrefersDayTimeOverTime()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("slept");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 3955; // 1.21.1
		spec.time = true;
		spec.timeValue = Q_INT64_C(7632100); // 318 days of ticks
		spec.dayTime = true;
		spec.dayTimeValue = Q_INT64_C(12078000); // day 503
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(503));
	}

	// A world that has not seen a full day yet is on day zero
	void test_DayCountBelowOneDay()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("fresh");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 3955;
		spec.dayTime = true;
		spec.dayTimeValue = Q_INT64_C(23999);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(0));
	}

	// Worlds older than 1.3 have no DayTime: Time is the day count. Value from
	// a real McRegion level.dat (version 19132, no DataVersion).
	void test_LegacyWorldWithoutDayTimeUsesTime()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("mcregion");

		LevelDatSpec spec;
		spec.time = true;
		spec.timeValue = Q_INT64_C(24049); // day 1
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(1));
	}

	// 26.1+: the day comes from the Overworld clock in world_clocks.dat, not
	// level.dat. Values from a real 26.2 world, whose Time would claim day 251
	// instead of 399.
	void test_ReadDayCountFromWorldClocks()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("clocks");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 4903; // 26.2
		spec.time = true;
		spec.timeValue = Q_INT64_C(6034843);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));
		QVERIFY(
			writeWorldClocks(worldPath, makeWorldClocks(Q_INT64_C(9580803))));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(399));
	}

	// A DayTime tag in level.dat is authoritative, the clock file is not
	void test_DayTimeTakesPrecedenceOverWorldClocks()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("bothclocks");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 3955;
		spec.dayTime = true;
		spec.dayTimeValue = Q_INT64_C(168001); // day 7
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));
		QVERIFY(
			writeWorldClocks(worldPath, makeWorldClocks(Q_INT64_C(9580803))));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(7));
	}

	// A 26.1+ world whose clock file is missing, corrupt or has no Overworld
	// clock has no day count: Time there means gametime, so showing it would be
	// wrong. DataVersion separates these from pre-1.3 worlds, where Time is the
	// day count.
	void test_ModernWorldWithoutAnyClockHasNoDayCount()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("noclocks");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 4903;
		spec.time = true;
		spec.timeValue = Q_INT64_C(6034843);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QVERIFY(!world.dayCount().has_value());
	}

	void test_CorruptWorldClocksFileIsIgnored()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("badclocks");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 4903;
		spec.time = true;
		spec.timeValue = Q_INT64_C(6034843);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));
		QVERIFY(QDir(worldPath).mkpath("data/minecraft"));
		QFile garbage(QDir(worldPath).absoluteFilePath(
			"data/minecraft/world_clocks.dat"));
		QVERIFY(garbage.open(QIODevice::WriteOnly));
		QVERIFY(garbage.write("not a gzipped nbt file") > 0);
		garbage.close();

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QVERIFY(!world.dayCount().has_value());
	}

	// Other dimensions have clocks of their own, but only the Overworld
	// one is the day players talk about
	void test_WorldClocksWithoutOverworldHasNoDayCount()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("endonly");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 4903;
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));
		QVERIFY(writeWorldClocks(worldPath, makeWorldClocks(std::nullopt)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QVERIFY(!world.dayCount().has_value());
	}

	// An int-typed clock is accepted as well as a long one
	void test_DayCountAcceptsIntTime()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("inttime");

		LevelDatSpec spec;
		spec.time = true;
		spec.timeAsInt = true;
		spec.timeValue = Q_INT64_C(48000); // 2 days
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(2));
	}

	// A negative tick count is nonsense, but must not produce negative days
	void test_NegativeDayTimeIsZeroDays()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("negative");

		LevelDatSpec spec;
		spec.dataVersion = true;
		spec.dataVersionValue = 3955;
		spec.dayTime = true;
		spec.dayTimeValue = Q_INT64_C(-48000);
		QVERIFY(writeWorld(worldPath, makeLevelDat(spec)));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QCOMPARE(world.dayCount().value_or(-1), Q_INT64_C(0));
	}

	// No world time anywhere: the world loads, the day count is unknown
	void test_MissingDayCountIsEmpty()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		auto worldPath = QDir(tempDir.path()).absoluteFilePath("timeless");

		QVERIFY(writeWorld(worldPath, makeLevelDat(LevelDatSpec())));

		World world{QFileInfo(worldPath)};
		QVERIFY(world.isValid());
		QVERIFY(!world.dayCount().has_value());
	}
};

QTEST_GUILESS_MAIN(WorldTest)

#include "World_test.moc"
