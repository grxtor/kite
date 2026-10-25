// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "2 friend requests" on Home's friends panel: waiting requests want an
 * answer, so this is a line of its own, with an accent dot, before the rows.
 * It bleeds out of the panel's column by its own padding, as the rows'
 * hover does; the caller gives it the column's x and width.
 */
AbstractButton {
    id: root

    property int count: 0

    // The chevron's glyph sits in the middle of its square, so this much of
    // the square is empty on its right (LinkButton.chevronBearing): the
    // padding is taken in by it, so the glyph ends where the "All" link's
    // does, on the column's edge.
    readonly property real chevronBearing: Theme.icon.sm * 0.34

    hoverEnabled: true
    height: Theme.control.height
    leftPadding: Theme.space.sm
    rightPadding: Theme.space.sm - root.chevronBearing
    text: root.count === 1 ? qsTr("1 friend request") : qsTr("%1 friend requests").arg(root.count)

    background: Rectangle {
        radius: Theme.radius.md
        color: root.down ? Theme.palette.pressedOverlay
             : root.hovered ? Theme.palette.surfaceHover : "transparent"
        border.width: root.visualFocus ? Theme.focus.width : 0
        border.color: Theme.palette.focusRing
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }
    contentItem: RowLayout {
        spacing: Theme.space.sm

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Theme.space.sm
            Layout.preferredHeight: Theme.space.sm
            radius: width / 2
            color: Theme.palette.accent
        }
        Text {
            Layout.fillWidth: true
            text: root.text
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.DemiBold
        }
        MeshIcon {
            iconName: "chevron-right"
            size: Theme.icon.sm
            color: Theme.palette.textSecondary
        }
    }
}
