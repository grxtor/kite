// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * Shows or hides a submenu's entry in its parent menu. A submenu has no `visible` of its own: the entry is a
 * MenuItem found by `subMenu`, and a hidden one still takes its row unless its height is zeroed.
 */
QtObject {
    id: root

    property Menu menu: null
    property Menu submenu: null
    property bool shown: true

    function sync() {
        for (var i = 0; root.menu && i < root.menu.count; ++i) {
            var item = root.menu.itemAt(i)
            if (item && item.subMenu === root.submenu) {
                item.visible = root.shown
                item.height = root.shown ? item.implicitHeight : 0
            }
        }
    }

    onShownChanged: sync()
    Component.onCompleted: Qt.callLater(root.sync)
}
