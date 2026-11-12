// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * One line of "What's new": its icon in a quiet square, the title and one sentence under it.
 */
RowLayout {
    id: root

    property string iconName: ""
    property string title: ""
    property string text: ""

    readonly property int tileSize: Theme.control.height

    spacing: Theme.space.md

    Rectangle {
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: root.tileSize
        Layout.preferredHeight: root.tileSize
        radius: Theme.radius.md
        color: Theme.palette.surfaceSunken
        border.width: 1
        border.color: Theme.palette.border

        MeshIcon {
            anchors.centerIn: parent
            iconName: root.iconName
            size: Theme.icon.sm
            color: Theme.palette.accentText
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        spacing: Theme.space.xxs

        Copy {
            Layout.fillWidth: true
            text: root.title
            role: "bodyStrong"
            wrapMode: Text.Wrap
        }
        Copy {
            Layout.fillWidth: true
            text: root.text
            role: "label"
            tone: "secondary"
            wrapMode: Text.Wrap
            lineHeight: 1.3
        }
    }
}
