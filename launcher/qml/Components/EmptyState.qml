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
    // Opt-in: true makes this fill its own visual parent and settle its
    // content near the top third of it instead of dead centre -- for a host
    // whose empty state sits in an otherwise-empty, very tall scrolling
    // column (a list that would hold a handful of rows' worth of real
    // content were it not empty), where centering wastes most of that
    // height (design-plan.md §4 "Empty states"/§5). Every existing centred
    // call site (`anchors.centerIn: parent` and similar) is unaffected:
    // this only changes anything once a caller opts in, since a plain Item
    // still just takes its content's own implicit size otherwise.
    property bool upperThird: false

    signal actionTriggered()

    default property alias illustration: illustrationSlot.data

    implicitWidth: root.upperThird && root.parent ? root.parent.width : column.implicitWidth
    implicitHeight: root.upperThird && root.parent ? root.parent.height : column.implicitHeight

    Column {
        id: column
        anchors.horizontalCenter: parent.horizontalCenter
        // In the default (non-upperThird) mode this Item is always exactly
        // column's own size, so this is 0 regardless of how a caller
        // anchors the Item itself -- the same effective position
        // `anchors.centerIn: parent` gave before.
        y: root.upperThird ? Math.max(0, Math.round(parent.height * 0.30 - height / 2))
                            : Math.round((parent.height - height) / 2)
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
