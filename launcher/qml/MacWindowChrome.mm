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

#include "qml/MacWindowChrome.h"

#include <QWindow>
#include <QtGlobal>

#include <AppKit/AppKit.h>

namespace MacWindowChrome {

Geometry mergeTitleBar(QWindow* window)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
	// Qt draws under the title bar from 6.9 on.
	Q_UNUSED(window);
	return {};
#else
	/* Qt's own switches for the first half: the content view grows under
	 * the title bar (Qt then maps the pointer for it) and the title bar
	 * stops painting a background of its own. */
	window->setFlags(window->flags() | Qt::ExpandedClientAreaHint
					 | Qt::NoTitleBarBackgroundHint);

	auto* view = reinterpret_cast<NSView*>(window->winId());
	NSWindow* nsWindow = view.window;
	if (!nsWindow)
		return {};

	/* The title stays the window's name for the Dock, the Window menu and
	 * Mission Control; only the title bar stops drawing it. */
	nsWindow.titleVisibility = NSWindowTitleHidden;
	nsWindow.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;

	/* Empty: it is only there for the title bar's height. Clicks under it
	 * still reach the content. */
	NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:@"io.github.grxtor.Kite.TitleBar"];
	nsWindow.toolbar = toolbar;
	[toolbar release];
	nsWindow.toolbarStyle = NSWindowToolbarStyleUnified;
	[nsWindow layoutIfNeeded];

	NSButton* close = [nsWindow standardWindowButton:NSWindowCloseButton];
	NSButton* zoom = [nsWindow standardWindowButton:NSWindowZoomButton];
	if (!close || !zoom)
		return {};

	const NSRect lights = NSUnionRect([close convertRect:close.bounds toView:nil],
									  [zoom convertRect:zoom.bounds toView:nil]);
	// Window coordinates run up from the bottom edge.
	const CGFloat middle = NSHeight(nsWindow.frame) - NSMidY(lights);
	return { qRound(NSMaxX(lights) + NSMinX(lights)), qRound(2 * middle) };
#endif
}

} // namespace MacWindowChrome
