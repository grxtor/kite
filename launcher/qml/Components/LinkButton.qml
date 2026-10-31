// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A section header's trailing action ("See all", "Manage"): text with a small
 * chevron and no padding of its own, so its last glyph lines up with the content
 * edge below it. Hover changes only the colour.
 */
AbstractButton {
    id: control

    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    implicitWidth: row.implicitWidth
    implicitHeight: Math.max(row.implicitHeight, Theme.control.heightSm)

    Accessible.role: Accessible.Link
    Accessible.name: control.text

    readonly property color ink: control.hovered || control.visualFocus ? Theme.palette.textPrimary
                                                                         : Theme.palette.textSecondary

    contentItem: Item {}

    // The chevron glyph sits in the middle of its square; the row is pulled out by the empty part
    // so the glyph's edge is on the content edge.
    readonly property real chevronBearing: Theme.icon.sm * 0.34

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: -control.chevronBearing
        spacing: Theme.space.xxs

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.ink
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Theme.type.label.weight
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "chevron-right"
            size: Theme.icon.sm
            color: control.ink
        }
    }

    Rectangle {
        anchors.left: row.left
        anchors.right: row.right
        anchors.top: row.bottom
        height: 1
        color: Theme.palette.focusRing
        visible: control.visualFocus
    }
}
