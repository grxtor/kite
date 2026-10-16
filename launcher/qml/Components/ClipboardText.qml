// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * QML has no clipboard API of its own; an invisible text edit does. Put one
 * of these in a page and call copyText() on it.
 */
TextEdit {
    visible: false

    function copyText(value) {
        text = value
        selectAll()
        copy()
    }
}
