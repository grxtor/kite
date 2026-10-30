// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * An icon-only button on a translucent white square: the hero's ▾ (more
 * ways to play) in the Sahne, Sade and Gece looks, where it sits on a photo
 * or the sky and the opaque Style button would be a block. The fill is the
 * host's; hover and press add the palette's overlays.
 */
AbstractButton {
    id: control

    property string iconName: "chevron-down"
    property string tip
    property int size: Theme.control.heightLg
    property color fill: Qt.rgba(1, 1, 1, 0.12)

    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    padding: 0

    Accessible.name: tip
    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.delay: 500
    ToolTip.text: tip

    opacity: enabled ? 1.0 : Theme.opacity.disabled

    background: Rectangle {
        radius: Theme.radius.lg
        color: control.fill
        border.width: control.visualFocus ? Theme.focus.width : 0
        border.color: Theme.palette.focusRing

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Theme.palette.pressedOverlay
                 : control.hovered ? Theme.palette.hoverOverlay : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
    }

    contentItem: Item {
        MeshIcon {
            anchors.centerIn: parent
            iconName: control.iconName
            size: Theme.icon.md
            color: Theme.palette.textPrimary
        }
    }
}
