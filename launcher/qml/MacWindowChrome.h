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

class QWindow;

namespace MacWindowChrome {

/* Where the window's own controls ended up: how far the traffic lights
 * reach from the window's left edge, with the same margin after them as
 * before them, and how tall a bar from the window's top edge has them in
 * its middle. Zeros when nothing was changed. */
struct Geometry
{
	int controlsWidth = 0;
	int barHeight = 0;
};

/* Lays the macOS title bar over the window's content instead of above it:
 * no title bar background and no title text, the content drawn up to the
 * window's top edge. An empty unified toolbar makes the title bar a
 * toolbar's height, and AppKit sets the traffic lights in it as in any
 * toolbar window -- as far in from the left edge as from the top. Moving
 * them anywhere else does not hold: AppKit puts them back on every layout
 * pass. Call before the window is first shown; the window must be on the
 * native Cocoa platform. */
Geometry mergeTitleBar(QWindow* window);

} // namespace MacWindowChrome
