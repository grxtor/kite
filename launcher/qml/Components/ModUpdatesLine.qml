// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "3 updates" on Home's hero: how many of the instance's mods have a newer version (ModUpdateReminder), and a way to
 * its Content tab, where the update is reviewed. Quiet like the facts beside it; only the hover says it is a link.
 * Hidden at zero.
 */
AbstractButton {
    id: root

    property int count: 0
    property color ink: Theme.palette.textSecondary
    property int textSize: Theme.type.body.pixelSize
    property int iconSize: Theme.icon.xs

    readonly property color shownInk: root.hovered || root.visualFocus ? Theme.palette.accentText : root.ink

    visible: root.count > 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    padding: 0
    text: root.count === 1 ? qsTr("1 update") : qsTr("%1 updates").arg(root.count)
    Accessible.role: Accessible.Link
    Accessible.name: root.count === 1 ? qsTr("1 mod has a new version. Open the mods")
                                      : qsTr("%1 mods have a new version. Open the mods").arg(root.count)

    ToolTip.visible: root.hovered
    ToolTip.delay: 500
    ToolTip.text: root.count === 1 ? qsTr("1 mod has a new version") : qsTr("%1 mods have a new version").arg(root.count)

    background: null
    contentItem: Row {
        spacing: Theme.space.sm - 1

        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "download"
            size: root.iconSize
            color: root.shownInk
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            textFormat: Text.PlainText
            color: root.shownInk
            font.family: Theme.font.family
            font.pixelSize: root.textSize
            font.features: ({ "tnum": 1 })
        }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
