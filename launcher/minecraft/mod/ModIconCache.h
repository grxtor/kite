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

#pragma once

#include <QByteArray>
#include <QFileInfo>
#include <QString>

/*
 * On-disk cache of the logos installed mods and packs declare for
 * themselves (fabric.mod.json "icon", mods.toml "logoFile", pack.png, ...),
 * so a mod list can show them without reopening every jar each time it is
 * drawn.
 *
 * KEY. One key per file *version*: the SHA-1 of the file's absolute path
 * (with a trailing ".disabled" dropped, so toggling a mod off and on reuses
 * the same entry) plus its modification time. A jar replaced by a newer
 * build therefore gets a new key -- a cache miss, never a stale logo -- and
 * no invalidation logic is needed. The key is plain lowercase hex, which is
 * also what makes it safe to put in an image:// URL and to turn back into a
 * path: isValidKey() rejects anything else, so a key can never point
 * outside directory().
 *
 * WRITES. store() runs on LocalModParseTask's worker thread. It decodes the
 * logo, shrinks anything larger than kMaxExtent (keeping the aspect ratio;
 * smaller pixel-art logos are kept as they are, for the provider to scale),
 * and saves it as PNG through QSaveFile, so a reader never sees a half
 * written file. Entries for files that no longer exist are not pruned; each
 * is a few KiB.
 */
namespace ModIconCache
{
	/// Largest edge kept on disk; matches the biggest size a mod list draws.
	constexpr int kMaxExtent = 128;

	/// Encoded logos above this are ignored rather than decoded.
	constexpr qint64 kMaxEncodedBytes = 4 * 1024 * 1024;

	/// "<data dir>/cache/LocalModIcons", next to the launcher's other
	/// caches. Resolved against the current directory, which the launcher
	/// sets to its data directory at startup -- call it after that.
	QString directory();

	QString keyFor(const QFileInfo& file);
	bool isValidKey(const QString& key);
	QString pathFor(const QString& cacheDir, const QString& key);

	bool contains(const QString& cacheDir, const QString& key);

	/// Decodes @p encoded and writes it under @p key. False if it is not
	/// an image (or is too large to be a logo) or cannot be written.
	bool store(const QString& cacheDir, const QString& key,
			   const QByteArray& encoded);
} // namespace ModIconCache
