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

#include <QList>
#include <QString>
#include <QStringList>
#include <QMetaType>

namespace LegacyFTB
{

	enum class PackType { Public, ThirdParty, Private };

	struct Modpack {
		QString name;
		QString description;
		QString author;
		QStringList oldVersions;
		QString currentVersion;
		QString mcVersion;
		QString mods;
		QString logo;

		QString dir;
		QString file; //<- Url in the xml, but doesn't make much sense

		bool bugged = false;
		bool broken = false;

		PackType type;
		QString packCode;
	};

	typedef QList<Modpack> ModpackList;

} // namespace LegacyFTB

// Needed for the proxy model
Q_DECLARE_METATYPE(LegacyFTB::Modpack)
