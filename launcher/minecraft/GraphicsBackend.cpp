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

#include "GraphicsBackend.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#include "PresetDefaults.h"
#include "gameoptions/GameOptions.h"

namespace
{
	/* 26.2 is the first version with the setting. */
	constexpr int kFirstMajor = 26;
	constexpr int kFirstMinor = 2;

	const QString kOptionsFile = QStringLiteral("options.txt");
	const QString kKey = QStringLiteral("preferredGraphicsBackend");
	const QString kDefault = QStringLiteral("default");
	const QStringList kBackends = {kDefault, QStringLiteral("opengl"),
								   QStringLiteral("vulkan")};

	QString optionsPath(const QString& gameRoot)
	{
		return QDir(gameRoot).filePath(kOptionsFile);
	}
} // namespace

bool GraphicsBackend::supportsVersion(const QString& mcVersion)
{
	static const QRegularExpression release(
		QStringLiteral("^(\\d+)\\.(\\d+)"));
	const auto match = release.match(mcVersion);
	if (!match.hasMatch()) {
		return false;
	}
	const int major = match.captured(1).toInt();
	return major > kFirstMajor ||
		   (major == kFirstMajor && match.captured(2).toInt() >= kFirstMinor);
}

QString GraphicsBackend::read(const QString& gameRoot)
{
	const QString path = optionsPath(gameRoot);
	/* GameOptions logs a warning for a file it cannot open. */
	if (!QFile::exists(path)) {
		return kDefault;
	}
	const GameOptions options(path);
	for (int row = 0; row < options.rowCount(); ++row) {
		const QModelIndex index = options.index(row, 0);
		if (options.data(index, GameOptions::KeyRole).toString() != kKey) {
			continue;
		}
		const QString value =
			options.data(index, GameOptions::ValueRole).toString();
		return kBackends.contains(value) ? value : kDefault;
	}
	return kDefault;
}

bool GraphicsBackend::write(const QString& gameRoot, const QString& backend)
{
	if (!kBackends.contains(backend)) {
		return false;
	}
	const QString path = optionsPath(gameRoot);
	if (QFile::exists(path)) {
		return GameOptions(path).setValue(kKey, backend);
	}
	if (backend == kDefault) {
		return true;
	}
	return PresetDefaults::writeOptions(gameRoot, {{kKey, backend}});
}
