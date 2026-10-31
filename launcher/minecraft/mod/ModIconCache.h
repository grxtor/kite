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

#pragma once

#include <QByteArray>
#include <QFileInfo>
#include <QString>

/*
 * On-disk cache of the logos mods and packs declare (fabric.mod.json "icon",
 * mods.toml "logoFile", pack.png, ...), so a mod list need not reopen every jar
 * each time it draws.
 *
 * KEY. One key per file version: the SHA-1 of the file's absolute path (a
 * trailing ".disabled" dropped, so toggling reuses the entry) plus its
 * modification time. A replaced jar gets a new key, never a stale logo. The key
 * is lowercase hex, safe in an image:// URL; isValidKey() rejects anything else
 * so a key can never point outside directory().
 *
 * WRITES. store() runs on LocalModParseTask's worker thread. It decodes the
 * logo, shrinks anything larger than kMaxExtent (keeping the aspect ratio;
 * smaller pixel art is kept) and saves PNG through QSaveFile, so a reader never
 * sees a half-written file. Entries for files that no longer exist are not
 * pruned; each is a few KiB.
 */
namespace ModIconCache
{
	/// Largest edge kept on disk; matches the biggest size a mod list draws.
	constexpr int kMaxExtent = 128;

	/// Encoded logos above this are ignored rather than decoded.
	constexpr qint64 kMaxEncodedBytes = 4 * 1024 * 1024;

	/// "<data dir>/cache/LocalModIcons". Resolved against the current
	/// directory, which the launcher sets to its data directory at startup:
	/// call it after that.
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
