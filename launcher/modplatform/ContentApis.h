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

#include <QString>

#include "modplatform/ContentApi.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"

/* The two ContentApi implementations looked up by the id stored in sidecars
 * ("modrinth" / "curseforge"). Header-only so the lookup stays next to the
 * interface without the interface knowing its implementations. */
namespace ModPlatform
{

	/* The provider whose id() is @p id, or nullptr for any other id. */
	inline const ContentApi* apiById(const QString& id)
	{
		if (id == ModrinthApi::get().id()) {
			return &ModrinthApi::get();
		}
		if (id == FlameApi::get().id()) {
			return &FlameApi::get();
		}
		return nullptr;
	}

	/* The provider's display name, or @p fallback for an unknown id. */
	inline QString displayNameById(const QString& id,
								   const QString& fallback = QString())
	{
		const ContentApi* api = apiById(id);
		return api ? api->displayName() : fallback;
	}

	/* The project's website page on that provider, or an empty string for an
	 * unknown id. */
	inline QString projectPageUrlById(const QString& id,
									  const QString& projectId)
	{
		const ContentApi* api = apiById(id);
		return api ? api->projectPageUrl(projectId).toString() : QString();
	}

} // namespace ModPlatform
