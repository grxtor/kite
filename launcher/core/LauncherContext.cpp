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

#include "core/LauncherContext.h"

namespace
{
	/* Deliberately a plain pointer and not an owning one: the implementation
	 * is the Application object itself, which owns its own lifetime and
	 * unregisters on the way out. */
	LauncherContext* g_context = nullptr;
}

LauncherContext* LauncherContext::instance()
{
	return g_context;
}

void LauncherContext::setInstance(LauncherContext* context)
{
	g_context = context;
}
