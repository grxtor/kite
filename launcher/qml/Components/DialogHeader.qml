// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A dialog's title row with an optional colour-tinted icon badge leading
 * the title -- for the handful of dialogs whose title alone doesn't say
 * enough (a destructive confirm, a host request carrying its own
 * severity). Set as `header:` only where the badge earns its place;
 * everything else keeps Style/Dialog.qml's plain title-only header.
 */
Item {
    id: root

    property string title
    property string icon: ""
    property color iconColor: Theme.palette.accent

    implicitHeight: Theme.space.lg + Math.max(Theme.control.height, label.implicitHeight)

    Rectangle {
        id: badge
        visible: root.icon.length > 0
        x: Theme.space.lg
        anchors.verticalCenter: label.verticalCenter
        width: Theme.control.height
        height: Theme.control.height
        radius: Theme.radius.md
        color: Qt.rgba(root.iconColor.r, root.iconColor.g, root.iconColor.b, 0.16)

        MeshIcon {
            anchors.centerIn: parent
            iconName: root.icon
            size: Theme.icon.md
            color: root.iconColor
        }
    }

    Label {
        id: label
        y: Theme.space.lg
        anchors.left: badge.visible ? badge.right : parent.left
        anchors.leftMargin: badge.visible ? Theme.space.sm : Theme.space.lg
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.lg
        text: root.title
        elide: Label.ElideRight
        font.pixelSize: Theme.type.title.pixelSize
        font.weight: Theme.type.title.weight
    }
}
