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
 *
 * `brand` leads with MeshMC's own mark instead of a badge, for the one
 * dialog that is the launcher's showpiece (New instance). Either way the
 * row is plain: no band, wash or texture behind it, so it reads as part
 * of the dialog's one surface.
 */
Item {
    id: root

    property string title
    property string icon: ""
    property color iconColor: Theme.palette.accent
    property bool brand: false

    readonly property bool leading: badge.visible || mark.visible

    // As tall as what is actually in the row: the badge's full control
    // height only when there is a badge. Reserving it for the brand mark or
    // a bare title left an empty strip under the title, which read as a gap
    // between the header and the dialog's first row.
    implicitHeight: Theme.space.lg + Math.max(badge.visible ? Theme.control.height
                                            : mark.visible ? mark.size : 0,
                                              label.implicitHeight)

    BrandMark {
        id: mark
        visible: root.brand && root.icon.length === 0
        x: Theme.space.lg
        anchors.verticalCenter: label.verticalCenter
        size: Theme.icon.lg
    }

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
        anchors.left: badge.visible ? badge.right : mark.visible ? mark.right : parent.left
        anchors.leftMargin: root.leading ? Theme.space.sm : Theme.space.lg
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.lg
        text: root.title
        elide: Label.ElideRight
        font.pixelSize: Theme.type.title.pixelSize
        font.weight: Theme.type.title.weight
    }
}
