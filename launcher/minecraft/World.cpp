/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2015-2021 MultiMC Contributors
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

#include <QDir>
#include <QString>
#include <QDebug>
#include <QSaveFile>
#include "World.h"

#include "GZip.h"
#include <MMCZip.h>
#include <FileSystem.h>
#include <sstream>
#include <io/stream_reader.h>
#include <tag_string.h>
#include <tag_primitive.h>

#include <QCoreApplication>

GameType::GameType(std::optional<int> original) : original(original)
{
	if (!original) {
		return;
	}
	switch (*original) {
		case 0:
			type = GameType::Survival;
			break;
		case 1:
			type = GameType::Creative;
			break;
		case 2:
			type = GameType::Adventure;
			break;
		case 3:
			type = GameType::Spectator;
			break;
		default:
			break;
	}
}

QString GameType::toTranslatedString() const
{
	switch (type) {
		case GameType::Survival:
			return QCoreApplication::translate("GameType", "Survival");
		case GameType::Creative:
			return QCoreApplication::translate("GameType", "Creative");
		case GameType::Adventure:
			return QCoreApplication::translate("GameType", "Adventure");
		case GameType::Spectator:
			return QCoreApplication::translate("GameType", "Spectator");
		default:
			break;
	}
	if (original) {
		return QCoreApplication::translate("GameType", "Unknown (%1)")
			.arg(*original);
	}
	return QCoreApplication::translate("GameType", "Undefined");
}

QString GameType::toLogString() const
{
	switch (type) {
		case GameType::Survival:
			return "Survival";
		case GameType::Creative:
			return "Creative";
		case GameType::Adventure:
			return "Adventure";
		case GameType::Spectator:
			return "Spectator";
		default:
			break;
	}
	if (original) {
		return QString("Unknown (%1)").arg(*original);
	}
	return "Undefined";
}

// level.dat is a few KB; the cap stops a gzip bomb from filling memory.
static constexpr qsizetype kMaxLevelDatBytes = 64 * 1024 * 1024;

std::unique_ptr<nbt::tag_compound> parseLevelDat(QByteArray data)
{
	QByteArray output;
	if (!GZip::unzip(data, output, kMaxLevelDatBytes)) {
		return nullptr;
	}
	std::istringstream foo(std::string(output.constData(), output.size()));
	try {
		auto pair = nbt::io::read_compound(foo);

		if (pair.first != "")
			return nullptr;

		if (pair.second == nullptr)
			return nullptr;

		return std::move(pair.second);
	} catch (const nbt::io::input_error& e) {
		qWarning() << "Unable to parse level.dat:" << e.what();
		return nullptr;
	}
}

QByteArray serializeLevelDat(nbt::tag_compound* levelInfo)
{
	std::ostringstream s;
	nbt::io::write_tag("", *levelInfo, s);
	QByteArray val(s.str().data(), (int)s.str().size());
	return val;
}

namespace
{
	// Newer versions keep the world generation settings, and the seed, in a
	// saved data file instead of level.dat.
	const QLatin1String worldGenSettingsPath(
		"data/minecraft/world_gen_settings.dat");

	// 26.1 moved the daylight clock out of level.dat the same way: one clock
	// per dimension, and the Overworld's is the day number the player sees.
	const QLatin1String worldClocksPath("data/minecraft/world_clocks.dat");
	const char* const overworldClockKey = "minecraft:overworld";
	const char* const clockTicksKey = "total_ticks";
} // namespace

QString getDatFileFromFS(const QFileInfo& file, const QString& relativePath)
{
	QDir worldDir(file.filePath());
	if (!file.isDir() || !worldDir.exists(relativePath)) {
		return QString();
	}
	return worldDir.absoluteFilePath(relativePath);
}

QByteArray getDatDataFromFS(const QFileInfo& file, const QString& relativePath)
{
	auto fullFilePath = getDatFileFromFS(file, relativePath);
	if (fullFilePath.isNull()) {
		return QByteArray();
	}
	QFile f(fullFilePath);
	if (!f.open(QIODevice::ReadOnly)) {
		return QByteArray();
	}
	return f.readAll();
}

QString getLevelDatFromFS(const QFileInfo& file)
{
	return getDatFileFromFS(file, QStringLiteral("level.dat"));
}

QByteArray getLevelDatDataFromFS(const QFileInfo& file)
{
	return getDatDataFromFS(file, QStringLiteral("level.dat"));
}

bool putLevelDatDataToFS(const QFileInfo& file, QByteArray& data)
{
	auto fullFilePath = getLevelDatFromFS(file);
	if (fullFilePath.isNull()) {
		return false;
	}
	QSaveFile f(fullFilePath);
	if (!f.open(QIODevice::WriteOnly)) {
		return false;
	}
	QByteArray compressed;
	if (!GZip::zip(data, compressed)) {
		return false;
	}
	if (f.write(compressed) != compressed.size()) {
		f.cancelWriting();
		return false;
	}
	return f.commit();
}

World::World(const QFileInfo& file)
{
	repath(file);
}

void World::repath(const QFileInfo& file)
{
	m_containerFile = file;
	m_folderName = file.fileName();
	if (file.isFile() && file.suffix() == "zip") {
		m_iconFile = QString();
		readFromZip(file);
	} else if (file.isDir()) {
		QFileInfo assumedIconPath(file.absoluteFilePath() + "/icon.png");
		if (assumedIconPath.exists()) {
			m_iconFile = assumedIconPath.absoluteFilePath();
		}
		readFromFS(file);
	}
}

bool World::resetIcon()
{
	if (m_iconFile.isNull()) {
		return false;
	}
	if (QFile(m_iconFile).remove()) {
		m_iconFile = QString();
		return true;
	}
	return false;
}

// Defined further down, with the other NBT reading helpers.
static std::optional<int64_t> readWorldGenSettingsSeed(const QByteArray& data);
static std::optional<int64_t> readWorldClocksDayCount(const QByteArray& data);

void World::readFromFS(const QFileInfo& file)
{
	auto bytes = getLevelDatDataFromFS(file);
	if (bytes.isEmpty()) {
		is_valid = false;
		return;
	}
	loadFromLevelDat(bytes);
	levelDatTime = file.lastModified();

	if (is_valid && m_randomSeed == 0) {
		auto worldGenBytes = getDatDataFromFS(file, worldGenSettingsPath);
		if (!worldGenBytes.isEmpty()) {
			m_randomSeed = readWorldGenSettingsSeed(worldGenBytes).value_or(0);
		}
	}

	if (is_valid && !m_dayCount) {
		auto clockBytes = getDatDataFromFS(file, worldClocksPath);
		if (!clockBytes.isEmpty()) {
			m_dayCount = readWorldClocksDayCount(clockBytes);
		}
	}
}

void World::readFromZip(const QFileInfo& file)
{
	QString zipPath = file.absoluteFilePath();
	auto location = MMCZip::findFolderOfFileInZip(zipPath, "level.dat");
	// Empty is level.dat at the archive's root; null is not found.
	is_valid = !location.isNull();
	if (!is_valid) {
		return;
	}
	m_containerOffsetPath = location;
	QByteArray levelDatData =
		MMCZip::readFileFromZip(zipPath, location + "level.dat");
	is_valid = !levelDatData.isEmpty();
	if (!is_valid) {
		return;
	}
	levelDatTime = MMCZip::getEntryModTime(zipPath, location + "level.dat");
	loadFromLevelDat(levelDatData);

	if (is_valid && m_randomSeed == 0) {
		auto worldGenBytes =
			MMCZip::readFileFromZip(zipPath, location + worldGenSettingsPath);
		if (!worldGenBytes.isEmpty()) {
			m_randomSeed = readWorldGenSettingsSeed(worldGenBytes).value_or(0);
		}
	}

	if (is_valid && !m_dayCount) {
		auto clockBytes =
			MMCZip::readFileFromZip(zipPath, location + worldClocksPath);
		if (!clockBytes.isEmpty()) {
			m_dayCount = readWorldClocksDayCount(clockBytes);
		}
	}
}

bool World::install(const QString& to, const QString& name)
{
	auto finalPath =
		FS::PathCombine(to, FS::DirNameFromString(m_actualName, to));
	if (!FS::ensureFolderPathExists(finalPath)) {
		return false;
	}
	bool ok = false;
	if (m_containerFile.isFile()) {
		auto result = MMCZip::extractSubDir(m_containerFile.absoluteFilePath(),
											m_containerOffsetPath, finalPath);
		ok = result.has_value();
	} else if (m_containerFile.isDir()) {
		QString from = m_containerFile.filePath();
		ok = FS::copy(from, finalPath)();
	}

	if (ok && !name.isEmpty() && m_actualName != name) {
		World newWorld{QFileInfo(finalPath)};
		if (newWorld.isValid()) {
			newWorld.rename(name);
		}
	}
	return ok;
}

bool World::rename(const QString& newName)
{
	if (m_containerFile.isFile()) {
		return false;
	}

	auto data = getLevelDatDataFromFS(m_containerFile);
	if (data.isEmpty()) {
		return false;
	}

	auto worldData = parseLevelDat(data);
	if (!worldData) {
		return false;
	}
	auto& val = worldData->at("Data");
	if (val.get_type() != nbt::tag_type::Compound) {
		return false;
	}
	auto& dataCompound = val.as<nbt::tag_compound>();
	dataCompound.put("LevelName",
					 nbt::value_initializer(newName.toUtf8().data()));
	data = serializeLevelDat(worldData.get());

	putLevelDatDataToFS(m_containerFile, data);

	m_actualName = newName;

	QDir parentDir(m_containerFile.absoluteFilePath());
	parentDir.cdUp();
	QFile container(m_containerFile.absoluteFilePath());
	auto dirName =
		FS::DirNameFromString(m_actualName, parentDir.absolutePath());
	container.rename(parentDir.absoluteFilePath(dirName));

	return true;
}

namespace
{

	std::optional<QString> read_string(nbt::value& parent, const char* name)
	{
		try {
			auto& namedValue = parent.at(name);
			if (namedValue.get_type() != nbt::tag_type::String) {
				return std::nullopt;
			}
			auto& tag_str = namedValue.as<nbt::tag_string>();
			return QString::fromStdString(tag_str.get());
		} catch (const std::out_of_range& e) {
			// fallback for old world formats
			qWarning() << "String NBT tag" << name << "could not be found.";
			return std::nullopt;
		} catch (const std::bad_cast& e) {
			// type mismatch
			qWarning() << "NBT tag" << name
					   << "could not be converted to string.";
			return std::nullopt;
		}
	}

	std::optional<int64_t> read_long(nbt::value& parent, const char* name)
	{
		try {
			auto& namedValue = parent.at(name);
			if (namedValue.get_type() != nbt::tag_type::Long) {
				return std::nullopt;
			}
			auto& tag_str = namedValue.as<nbt::tag_long>();
			return tag_str.get();
		} catch (const std::out_of_range& e) {
			// fallback for old world formats
			qWarning() << "Long NBT tag" << name << "could not be found.";
			return std::nullopt;
		} catch (const std::bad_cast& e) {
			// type mismatch
			qWarning() << "NBT tag" << name
					   << "could not be converted to long.";
			return std::nullopt;
		}
	}

	std::optional<int> read_int(nbt::value& parent, const char* name)
	{
		try {
			auto& namedValue = parent.at(name);
			if (namedValue.get_type() != nbt::tag_type::Int) {
				return std::nullopt;
			}
			auto& tag_str = namedValue.as<nbt::tag_int>();
			return tag_str.get();
		} catch (const std::out_of_range& e) {
			// fallback for old world formats
			qWarning() << "Int NBT tag" << name << "could not be found.";
			return std::nullopt;
		} catch (const std::bad_cast& e) {
			// type mismatch
			qWarning() << "NBT tag" << name << "could not be converted to int.";
			return std::nullopt;
		}
	}

	// Tags have changed width between versions and a missing tag is normal:
	// accept either width and stay quiet about absent tags.
	std::optional<int64_t> read_long_or_int(nbt::value& parent,
											const char* name)
	{
		try {
			auto& namedValue = parent.at(name);
			switch (namedValue.get_type()) {
				case nbt::tag_type::Long:
					return namedValue.as<nbt::tag_long>().get();
				case nbt::tag_type::Int:
					return namedValue.as<nbt::tag_int>().get();
				default:
					return std::nullopt;
			}
		} catch (const std::out_of_range&) {
			return std::nullopt;
		} catch (const std::bad_cast&) {
			return std::nullopt;
		}
	}

	GameType read_gametype(nbt::value& parent, const char* name)
	{
		return GameType(read_int(parent, name));
	}

	// One Minecraft day is 24000 ticks in every version so far.
	const int64_t ticksPerDay = 24000;

	// Clocks are signed and mods can run them backwards: clamp, never report a
	// negative day.
	int64_t ticksToDays(int64_t ticks)
	{
		return (ticks > 0 ? ticks : 0) / ticksPerDay;
	}

	// The day number the game shows is the daylight cycle, "DayTime" in
	// level.dat: it counts past 24000 and is never reset. "Time" is the ticks
	// the world has run (/time query gametime); sleeping jumps DayTime to
	// morning but not Time, so Time reports far fewer days than the player saw.
	//
	// "DayTime" exists from 1.3 on. Older worlds have no separate daylight
	// clock, so there Time is the day count; they also predate "DataVersion"
	// (1.9), which tells them apart from 26.1+, where the clock moved to
	// world_clocks.dat and Time would be wrong again.
	std::optional<int64_t> read_day_count(nbt::value& parent)
	{
		auto ticks = read_long_or_int(parent, "DayTime");
		if (!ticks && !read_long_or_int(parent, "DataVersion")) {
			ticks = read_long_or_int(parent, "Time");
		}
		if (!ticks) {
			return std::nullopt;
		}
		return ticksToDays(*ticks);
	}

} // namespace

static std::optional<int64_t> readWorldGenSettingsSeed(const QByteArray& data)
{
	auto worldGenData = parseLevelDat(data);
	if (!worldGenData) {
		return std::nullopt;
	}

	nbt::value* valPtr = nullptr;
	try {
		// Saved data files wrap their payload in a "data" compound.
		valPtr = &worldGenData->at("data");
	} catch (const std::out_of_range&) {
		qWarning() << "Unable to read the \"data\" compound from"
				   << worldGenSettingsPath;
		return std::nullopt;
	}
	nbt::value& val = *valPtr;

	if (val.get_type() != nbt::tag_type::Compound) {
		return std::nullopt;
	}

	auto seed = read_long(val, "seed");
	if (seed) {
		qDebug() << "Seed:" << *seed;
	}
	return seed;
}

// 26.1+ keeps the daylight clock here instead of level.dat's DayTime. Each
// dimension has its own clock keyed by dimension id, recording its total ticks
// at 24000 per day (no wrapping). The Overworld clock is the day players quote.
static std::optional<int64_t> readWorldClocksDayCount(const QByteArray& data)
{
	auto clockData = parseLevelDat(data);
	if (!clockData) {
		return std::nullopt;
	}

	nbt::value* valPtr = nullptr;
	try {
		// Saved data files wrap their payload in a "data" compound.
		valPtr = &clockData->at("data");
	} catch (const std::out_of_range&) {
		qWarning() << "Unable to read the \"data\" compound from"
				   << worldClocksPath;
		return std::nullopt;
	}
	nbt::value& val = *valPtr;

	if (val.get_type() != nbt::tag_type::Compound) {
		return std::nullopt;
	}

	nbt::value* clockPtr = nullptr;
	try {
		clockPtr = &val.at(overworldClockKey);
	} catch (const std::out_of_range&) {
		qWarning() << "No" << overworldClockKey << "clock in"
				   << worldClocksPath;
		return std::nullopt;
	}
	nbt::value& clock = *clockPtr;

	if (clock.get_type() != nbt::tag_type::Compound) {
		return std::nullopt;
	}

	auto ticks = read_long_or_int(clock, clockTicksKey);
	if (!ticks) {
		return std::nullopt;
	}
	auto days = ticksToDays(*ticks);
	qDebug() << "Day Count:" << days;
	return days;
}

void World::loadFromLevelDat(QByteArray data)
{
	auto levelData = parseLevelDat(data);
	if (!levelData) {
		is_valid = false;
		return;
	}

	nbt::value* valPtr = nullptr;
	try {
		valPtr = &levelData->at("Data");
	} catch (const std::out_of_range& e) {
		qWarning() << "Unable to read NBT tags from " << m_folderName << ":"
				   << e.what();
		is_valid = false;
		return;
	}
	nbt::value& val = *valPtr;

	is_valid = val.get_type() == nbt::tag_type::Compound;
	if (!is_valid)
		return;

	auto name = read_string(val, "LevelName");
	m_actualName = name ? *name : m_folderName;

	auto timestamp = read_long(val, "LastPlayed");
	m_lastPlayed =
		timestamp ? QDateTime::fromMSecsSinceEpoch(*timestamp) : levelDatTime;

	m_gameType = read_gametype(val, "GameType");

	m_dayCount = read_day_count(val);

	std::optional<int64_t> randomSeed;
	try {
		auto& WorldGen_val = val.at("WorldGenSettings");
		randomSeed = read_long(WorldGen_val, "seed");
	} catch (const std::out_of_range&) {
	}
	if (!randomSeed) {
		randomSeed = read_long(val, "RandomSeed");
	}
	m_randomSeed = randomSeed ? *randomSeed : 0;

	qDebug() << "World Name:" << m_actualName;
	qDebug() << "Last Played:" << m_lastPlayed.toString();
	if (m_dayCount) {
		qDebug() << "Day Count:" << *m_dayCount;
	}
	if (randomSeed) {
		qDebug() << "Seed:" << *randomSeed;
	}
	qDebug() << "GameType:" << m_gameType.toLogString();
}

bool World::replace(World& with)
{
	if (!destroy())
		return false;
	bool success =
		FS::copy(with.m_containerFile.filePath(), m_containerFile.path())();
	if (success) {
		m_folderName = with.m_folderName;
		m_containerFile.refresh();
	}
	return success;
}

bool World::destroy()
{
	if (!is_valid)
		return false;
	if (m_containerFile.isDir()) {
		QDir d(m_containerFile.filePath());
		return d.removeRecursively();
	} else if (m_containerFile.isFile()) {
		QFile file(m_containerFile.absoluteFilePath());
		return file.remove();
	}
	return true;
}

bool World::operator==(const World& other) const
{
	return is_valid == other.is_valid && folderName() == other.folderName();
}
