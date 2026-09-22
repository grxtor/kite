// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Centered "nothing here" placeholder, shared by InstanceGrid (zero
 * instances) and anything else that needs the same shape later.
 *
 * The illustration is a default-property slot rather than an iconSource
 * string: callers vary from a single glyph (Gallery, InstanceGrid) to a
 * fuller drawing, and a slot lets either be dropped in without this file
 * needing to know which.
 */
Item {
    id: root

    property alias title: titleLabel.text
    property alias body: bodyLabel.text
    property string actionText: ""
    property string actionIcon: ""

    signal actionTriggered()

    default property alias illustration: illustrationSlot.data

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    Column {
        id: column
        anchors.centerIn: parent
        spacing: Theme.space.md
        width: Math.min(320, root.width)

        Item {
            id: illustrationSlot
            anchors.horizontalCenter: parent.horizontalCenter
            width: childrenRect.width
            height: childrenRect.height
        }

        Text {
            id: titleLabel
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Theme.type.title.weight
        }

        Text {
            id: bodyLabel
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            font.weight: Theme.type.body.weight
            visible: text.length > 0
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.actionText
            highlighted: true
            icon.source: root.actionIcon.length > 0 ? Icons.url(root.actionIcon) : ""
            visible: root.actionText.length > 0
            onClicked: root.actionTriggered()
        }
    }
}
