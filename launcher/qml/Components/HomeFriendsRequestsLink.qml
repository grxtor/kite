// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "2 friend requests" on Home's friends panel: waiting requests want an answer, so this is a line of its own, with an
 * accent dot, before the rows. It bleeds out of the panel's column by its own padding; the caller gives it the
 * column's x and width.
 */
AbstractButton {
    id: root

    property int count: 0

    // The chevron's glyph sits mid-square, leaving this much empty on its right (LinkButton.chevronBearing); the
    // padding takes it in so the glyph ends at the column's edge.
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
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        FocusFrame { around: 0; visible: root.visualFocus }
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
        Copy {
            Layout.fillWidth: true
            text: root.text
            elide: Text.ElideRight
            role: "label"
            font.weight: Font.DemiBold
        }
        MeshIcon {
            iconName: "chevron-right"
            size: Theme.icon.sm
            color: Theme.palette.textSecondary
        }
    }
}
