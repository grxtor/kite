// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What a drag over the window will do, drawn over the page: "Drop to add 2 mods and 1 resource pack". `summary` is
 * ContentDrop.describe() of the files being dragged; empty when it cannot tell yet, which leaves the plain
 * "Drop to add". Takes no pointer events, the drop area under it does.
 */
Rectangle {
    id: root

    property string summary: ""
    readonly property string message: root.summary.length > 0 ? qsTr("Drop to add %1").arg(root.summary)
                                                              : qsTr("Drop to add")

    radius: Theme.radius.lg
    color: Theme.palette.accentSubtle
    border.width: 1
    border.color: Theme.palette.accent

    Accessible.role: Accessible.StaticText
    Accessible.name: root.message

    Copy {
        anchors.centerIn: parent
        width: parent.width - Theme.space.xl * 2
        horizontalAlignment: Text.AlignHCenter
        text: root.message
        role: "bodyStrong"
        tone: "accent"
        wrapMode: Text.Wrap
    }
}
