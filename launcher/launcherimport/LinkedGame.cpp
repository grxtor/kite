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

#include "LinkedGame.h"

#include <QFileInfo>

#include "BaseInstance.h"
#include "ImportBuild.h"
#include "SafeFiles.h"
#include "settings/INIFile.h"

namespace LauncherImport
{
	QString outsideGameRoot(const BaseInstance& instance)
	{
		const QString game = instance.gameRoot();
		if (!QFileInfo(game).isDir() || resolvesInside(instance.instanceRoot(), game)) {
			return {};
		}
		return QFileInfo(game).canonicalFilePath();
	}

	QByteArray withoutLink(const QByteArray& config)
	{
		INIFile ini;
		ini.loadFile(config);
		ini.remove(kLinkedGameDirKey);
		/* As INIFile::saveFile() writes it. */
		QByteArray out;
		for (auto it = ini.cbegin(); it != ini.cend(); ++it) {
			out += it.key().toUtf8() + '=' + INIFile::escape(it.value().toString()).toUtf8() + '\n';
		}
		return out;
	}
} // namespace LauncherImport
