// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * One page of the shell's StackLayout, built the first time it is shown and kept
 * after that, so its scroll position and tab survive. A hidden page still builds
 * everything it has, so a page nobody opens must cost nothing. Set `current`
 * first (root.page = ...) to build a page synchronously: `item` is the page
 * right after.
 */
Loader {
    id: root

    property string page
    property string current
    // Latched once the page has been wanted; `active` alone would drop the
    // page again on the next change of `current`.
    property bool built: false

    active: built || current === page
    onCurrentChanged: if (current === page) built = true
}
