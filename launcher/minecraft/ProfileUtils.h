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
#include "Library.h"
#include "VersionFile.h"

namespace ProfileUtils
{
	typedef QStringList PatchOrder;

	/// Read and parse a OneSix format order file
	bool readOverrideOrders(QString path, PatchOrder& order);

	/// Parse a version file in JSON format
	VersionFilePtr parseJsonFile(const QFileInfo& fileInfo,
								 const bool requireOrder);

	/// Save a JSON file (in any format)
	bool saveJsonFile(const QJsonDocument doc, const QString& filename);

	/// Parse a version file in binary JSON format
	VersionFilePtr parseBinaryJsonFile(const QFileInfo& fileInfo);

	/// Remove LWJGL from a patch file. This is applied to all Mojang-like
	/// profile files.
	void removeLwjglFromPatch(VersionFilePtr patch);

} // namespace ProfileUtils
