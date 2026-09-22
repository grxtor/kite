// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One setting: what it is and what it does on the left, its control on the
 * right. `wide` puts the control under the text instead, for text fields
 * that need the room.
 */
Item {
    id: root

    property string label
    property string description
    property bool wide: false
    property bool showDivider: false
    default property alias control: controlSlot.data

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(Theme.control.heightLg + Theme.space.md,
                             layout.implicitHeight + Theme.space.lg * 2)
    opacity: enabled ? 1 : Theme.opacity.disabled

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

    GridLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.lg
        columns: root.wide ? 1 : 2
        columnSpacing: Theme.space.xl
        rowSpacing: Theme.space.sm

        Column {
            Layout.fillWidth: true
            spacing: 2

            Text {
                width: parent.width
                text: root.label
                wrapMode: Text.Wrap
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: Font.Medium
            }

            Text {
                width: parent.width
                visible: root.description.length > 0
                text: root.description
                wrapMode: Text.Wrap
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                lineHeight: 1.3
            }
        }

        Item {
            id: controlSlot
            Layout.fillWidth: root.wide
            Layout.alignment: Qt.AlignVCenter | (root.wide ? Qt.AlignLeft : Qt.AlignRight)
            // A wide control takes the width the layout gives it, so it must
            // not also report its own width back as the implicit one.
            implicitWidth: root.wide ? 0 : childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
