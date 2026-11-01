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

#include "IconUtils.h"
#include "Logging.h"

#include "FileSystem.h"
#include <QDirIterator>

#include <array>

namespace
{
	std::array<const char*, 6> validIconExtensions = {
		{"svg", "png", "ico", "gif", "jpg", "jpeg"}};
}

namespace IconUtils
{

	QString findBestIconIn(const QString& folder, const QString& iconKey)
	{
		int best_found = validIconExtensions.size();
		QString best_filename;

		QDirIterator it(folder, QDir::NoDotAndDotDot | QDir::Files,
						QDirIterator::NoIteratorFlags);
		while (it.hasNext()) {
			it.next();
			auto fileInfo = it.fileInfo();

			if (fileInfo.completeBaseName() != iconKey)
				continue;

			auto extension = fileInfo.suffix();

			for (int i = 0; i < best_found; i++) {
				if (extension == validIconExtensions[i]) {
					best_found = i;
					qCDebug(iconsLog) << i << " : " << fileInfo.fileName();
					best_filename = fileInfo.fileName();
				}
			}
		}
		return FS::PathCombine(folder, best_filename);
	}

} // namespace IconUtils
