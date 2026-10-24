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

#include <QString>
#include <QStringList>

/* What ModHealth knows about the loaders themselves: the ids the runtime
 * provides, and the versions a built-in answers to besides its own. Private
 * to ModHealth.cpp and ModHealthLoaders.cpp, which holds the definitions and
 * the reasons for each. */
namespace ModHealth
{
	namespace detail
	{
		extern const QString MinecraftId;
		extern const QStringList RuntimeIds;
		extern const QStringList LoaderIds;

		QStringList alsoAnswersTo(const QString& id, const QStringList& loaders,
								  const QString& minecraft);
	} // namespace detail
} // namespace ModHealth
