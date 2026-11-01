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
#include <QString>

#include <optional>

/* The identifier CurseForge looks files up by: it has no SHA-1 endpoint, so
 * exports naming mods in a manifest send this to `/v1/fingerprints`.
 * MurmurHash2 (32-bit, seed 1) over the file with every tab, newline,
 * carriage return and space removed first; the filtering is unprincipled
 * but part of the identifier. Not a security primitive: a match only
 * selects a candidate, checked against the file's real size and digest. */
namespace FlameFingerprint
{
	/* The fingerprint of `data`, filtered here, not by the caller. */
	quint32 ofData(const QByteArray& data);

	/* The fingerprint of the file at `path`, or nullopt if unreadable. Read in
	 * blocks: resource packs run to hundreds of megabytes. An unreadable file
	 * just travels inside the pack, so absence is reported, not a reason. */
	std::optional<quint32> ofFile(const QString& path);
} // namespace FlameFingerprint
