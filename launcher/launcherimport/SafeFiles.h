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

#pragma once

#include <QByteArray>
#include <QFileInfo>
#include <QJsonObject>
#include <QString>

/*
 * Reading another launcher's files as untrusted input: every read has a size
 * cap, a path from a manifest is only joined after it is checked, and a link
 * is followed only when it ends up inside the folder being read.
 */
namespace LauncherImport
{
	/* Caps on what one file may cost to read. A manifest past its cap is
	 * refused rather than read in part. */
	constexpr qint64 kMaxConfigBytes = 256 * 1024;			 /* instance.cfg */
	constexpr qint64 kMaxSmallJsonBytes = 4 * 1024 * 1024;	 /* mmc-pack.json, profiles */
	constexpr qint64 kMaxManifestBytes = 32 * 1024 * 1024;	 /* minecraftinstance.json */
	/* Instances looked at per launcher; a folder with more is cut short. */
	constexpr int kMaxInstancesPerLauncher = 1000;
	/* An instance icon larger than this is not a picture worth showing. */
	constexpr qint64 kMaxIconBytes = 1024 * 1024;

	/* The file's bytes, or false when it is missing, not a regular file or
	 * larger than @p maxBytes. @p tooLarge tells the last case apart. */
	bool readCapped(const QString& path, qint64 maxBytes, QByteArray* out,
					bool* tooLarge = nullptr);

	/* A JSON object read with readCapped(); false when the file is missing,
	 * too large or not an object. @p error says which, for the user. */
	bool readJsonObject(const QString& path, qint64 maxBytes, QJsonObject* out,
						QString* error);

	/* A symbolic link, a Windows junction or a macOS alias: anything that
	 * stands for another place. */
	bool isLink(const QFileInfo& info);

	/* Whether @p path, with its links resolved, lies inside @p root (or is
	 * it). False when either does not exist. */
	bool resolvesInside(const QString& root, const QString& path);

	/* A folder an instance lives in: an existing directory that is not a
	 * link, or a link that resolves inside @p root. */
	bool isUsableDir(const QString& root, const QString& path);

	/* @p path when it is a picture an instance list may show: a png, jpg,
	 * gif or svg of at most kMaxIconBytes that lies inside @p root (links
	 * resolved); "" otherwise. A manifest naming a file elsewhere gets no
	 * icon. */
	QString usableIconFile(const QString& root, const QString& path);

	/* One file or folder name, no separators, not "." or ".." (on Windows a
	 * "C:" is a path too). */
	bool isSafeName(const QString& name);

	/* @p root joined with a '/'-separated @p relative made of safe names, or
	 * "" when any part is unsafe. */
	QString joinSafe(const QString& root, const QString& relative);
} // namespace LauncherImport
