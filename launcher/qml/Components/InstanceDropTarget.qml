// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Files and links dropped on an instance's tile (library grid, list row, sidebar) are added to that instance. The
 * window's Main.qml sorts and installs them (dropOnInstance), so a tile only has to say which instance it is and
 * light up while a drag is over it. Only file drags: an instance dragged between groups carries its own key.
 */
DropArea {
    id: root

    property string instanceId: ""
    property int radius: Theme.radius.md

    keys: ["text/uri-list"]
    enabled: root.instanceId.length > 0
    onDropped: (drop) => {
        const window = Window.window
        if (!drop.hasUrls || !window || typeof window.dropOnInstance !== "function")
            return
        const urls = []
        for (let i = 0; i < drop.urls.length; ++i)
            urls.push(drop.urls[i].toString())
        drop.accept(Qt.CopyAction)
        window.dropOnInstance(root.instanceId, urls)
    }

    Rectangle {
        anchors.fill: parent
        visible: root.containsDrag
        radius: root.radius
        color: Theme.palette.accentSubtle
        border.width: 1
        border.color: Theme.palette.accent
    }
}
