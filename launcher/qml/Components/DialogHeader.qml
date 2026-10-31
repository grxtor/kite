// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A dialog's title row: the title, plain, the same in every dialog. An icon appears only for a warning or an
 * error: pass it with the tone colour (Theme.palette.danger, .warning...); without a tone colour the plain title
 * shows. The title is a size over the dialog's section titles so the dialog's name still leads.
 */
Item {
    id: root

    property string title
    property string icon: ""
    property color iconColor: Theme.palette.accent

    readonly property bool toned: root.icon.length > 0
                                  && !Qt.colorEqual(root.iconColor, Theme.palette.accent)

    implicitHeight: Theme.space.xl + Math.max(Theme.icon.md, label.implicitHeight)

    MeshIcon {
        id: glyph
        visible: root.toned
        x: Theme.space.xl
        anchors.verticalCenter: label.verticalCenter
        iconName: root.icon
        size: Theme.icon.md
        color: root.iconColor
    }

    Label {
        id: label
        y: Theme.space.xl
        anchors.left: root.toned ? glyph.right : parent.left
        anchors.leftMargin: root.toned ? Theme.space.sm : Theme.space.xl
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.xl
        text: root.title
        elide: Label.ElideRight
        font.pixelSize: Theme.type.subheading.pixelSize
        font.family: Theme.font.heading
        font.weight: Theme.type.subheading.weight
    }
}
