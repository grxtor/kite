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

/* api.minecraftservices.com, the host every profile, skin, cape and
 * entitlement call goes to. */
namespace MinecraftServices
{

	inline QString baseUrl()
	{
		return QStringLiteral("https://api.minecraftservices.com");
	}

	/* @p path has no leading slash, e.g. "minecraft/profile". */
	inline QString url(const QString& path)
	{
		return baseUrl() + QLatin1Char('/') + path;
	}

} // namespace MinecraftServices
