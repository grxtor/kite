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

#include "PresetDefaults.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <algorithm>

namespace
{
	constexpr quint64 kMiBPerGiB = 1024;
	constexpr int kMemoryStepMiB = 512;
	constexpr int kLeastMaxMemoryMiB = 1024;

	struct MemoryTier {
		quint64 minRamGiB;
		int maxMemoryMiB;
	};
	/* Largest first. */
	constexpr MemoryTier kMemoryTiers[] = {{32, 6144}, {16, 4096}, {8, 3072}};

	constexpr int kFirstModernMinor = 13;
	constexpr int kFirstNumberedMajor = 2;
	/* 17w43a, the first 1.13 snapshot. */
	constexpr int kFirstModernSnapshotYear = 17;
	constexpr int kFirstModernSnapshotWeek = 43;

	const QString kOptionsFile = QStringLiteral("options.txt");
} // namespace

int PresetDefaults::maxMemoryMiB(quint64 systemRamMiB)
{
	for (const MemoryTier& tier : kMemoryTiers) {
		if (systemRamMiB >= tier.minRamGiB * kMiBPerGiB) {
			return tier.maxMemoryMiB;
		}
	}
	const int half = int(systemRamMiB / 2);
	return std::max(kLeastMaxMemoryMiB, half - half % kMemoryStepMiB);
}

bool PresetDefaults::usesLegacyOptions(const QString& mcVersion)
{
	static const QRegularExpression snapshot(
		QStringLiteral("^(\\d\\d)w(\\d\\d)[a-z]$"));
	const auto weekly = snapshot.match(mcVersion);
	if (weekly.hasMatch()) {
		const int year = weekly.captured(1).toInt();
		return year < kFirstModernSnapshotYear ||
			   (year == kFirstModernSnapshotYear &&
				weekly.captured(2).toInt() < kFirstModernSnapshotWeek);
	}
	static const QRegularExpression release(
		QStringLiteral("^(\\d+)\\.(\\d+)"));
	const auto match = release.match(mcVersion);
	if (!match.hasMatch()) {
		return true;
	}
	const int major = match.captured(1).toInt();
	if (major >= kFirstNumberedMajor) {
		return false;
	}
	return major < 1 || match.captured(2).toInt() < kFirstModernMinor;
}

bool PresetDefaults::writeOptions(const QString& gameRoot,
								  const QMap<QString, QString>& options)
{
	if (options.isEmpty() || !QDir().mkpath(gameRoot)) {
		return false;
	}
	/* NewOnly: an existing options.txt is the player's, never replaced. */
	QFile file(QDir(gameRoot).filePath(kOptionsFile));
	if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
		return false;
	}
	for (auto it = options.cbegin(); it != options.cend(); ++it) {
		file.write(it.key().toUtf8() + ':' + it.value().toUtf8() + '\n');
	}
	return file.flush();
}
