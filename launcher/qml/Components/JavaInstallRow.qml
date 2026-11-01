// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One Java runtime as a row: its name, where it lives, and small marks (32-bit, vendor, Recommended, In use). The
 * one row of the first-run list, the Choose Java dialog and the downloaded runtimes; a button that is `chosen` while
 * it is the Java in use. Children added to it land at the right (a ⋯ menu button).
 */
AbstractButton {
    id: root

    property string title
    property string location
    property bool chosen: false
    property bool suggested: false
    property bool inUse: false
    property bool bit32: false
    property string vendor: ""
    // False for a row that only shows a runtime: no hover, press or focus.
    property bool interactive: true
    property bool showDivider: false
    default property alias trailing: layout.data

    Accessible.role: root.interactive ? Accessible.Button : Accessible.ListItem
    Accessible.name: root.title

    implicitHeight: Theme.control.heightLg + Theme.space.xs
    leftPadding: Theme.space.md
    rightPadding: Theme.space.md
    hoverEnabled: root.interactive
    focusPolicy: root.interactive ? Qt.StrongFocus : Qt.NoFocus

    background: StateFill {
        hovered: root.interactive && root.hovered
        down: root.interactive && root.down
        active: root.chosen
        radius: Theme.radius.md

        Rectangle {
            visible: root.showDivider
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.lg
            height: 1
            color: Theme.palette.divider
        }
        FocusFrame {
            around: 0
            visible: root.visualFocus && root.interactive
        }
    }

    contentItem: RowLayout {
        id: layout
        spacing: Theme.space.md

        Column {
            Layout.fillWidth: true
            spacing: Theme.space.xxs

            Copy {
                width: parent.width
                text: root.title
                role: "bodyStrong"
                tone: root.chosen ? "accent" : "primary"
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: root.location
                elide: Text.ElideMiddle
                color: Theme.palette.textTertiary
                font.family: Theme.font.mono
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
        Tag {
            visible: root.bit32
            text: qsTr("32-bit")
        }
        Tag {
            visible: root.vendor.length > 0
            text: root.vendor
        }
        StatusBadge {
            visible: root.suggested
            text: qsTr("Recommended")
        }
        StatusBadge {
            visible: root.inUse
            tone: "success"
            text: qsTr("In use")
        }
    }
}
