/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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
#include <functional>

class QMenu;

/* Builds the system tray menu from a plugin's "mmco-tray-menu/1" document
 * (MMCOContext::tray_set_menu). */
class PluginUiRenderer
{
  public:
	/* nodeId, event ("click"), value_json (always empty for a menu item). */
	using EventSink = std::function<void(
		const QString& nodeId, const QString& event, const QString& valueJson)>;

	/* Parses a tray-menu document (button/separator/section nodes; section = submenu) and populates
	 * `menu` with QActions wired to sink(nodeId, "click", ""). False on malformed JSON. */
	static bool buildTrayMenu(QMenu* menu, const QString& jsonDoc,
							 EventSink sink);
};
