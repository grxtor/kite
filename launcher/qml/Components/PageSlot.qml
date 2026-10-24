// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * One page of the shell's StackLayout, built the first time it is the page
 * showing and kept after that, so its scroll position and tab survive. A
 * page nobody opens costs nothing: a hidden page still builds every item,
 * binding and delegate it has, and the shell used to build all six at start.
 *
 * `current` is the shell's page; `page` is the one this slot holds. Ask for
 * a page before touching it by setting `current` first (root.page = ...),
 * which builds it synchronously: `item` is the page right after.
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
