// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A dialog's title row: the title, plain, the same in every dialog. No
 * band, wash, tile or logo behind or beside it -- a dialog about exporting
 * or installing does not need an icon to say so.
 *
 * An icon appears only when the dialog is a warning or an error: pass the
 * icon together with the tone colour (Theme.palette.danger, .warning...).
 * Callers that name an icon but no tone colour (the accent is the default)
 * get the plain title; the icon is a bare glyph in that tone, never a
 * tinted tile.
 */
Item {
    id: root

    property string title
    property string icon: ""
    property color iconColor: Theme.palette.accent

    readonly property bool toned: root.icon.length > 0
                                  && !Qt.colorEqual(root.iconColor, Theme.palette.accent)

    implicitHeight: Theme.space.lg + Math.max(Theme.icon.md, label.implicitHeight)

    MeshIcon {
        id: glyph
        visible: root.toned
        x: Theme.space.lg
        anchors.verticalCenter: label.verticalCenter
        iconName: root.icon
        size: Theme.icon.md
        color: root.iconColor
    }

    Label {
        id: label
        y: Theme.space.lg
        anchors.left: root.toned ? glyph.right : parent.left
        anchors.leftMargin: root.toned ? Theme.space.sm : Theme.space.lg
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.lg
        text: root.title
        elide: Label.ElideRight
        font.pixelSize: Theme.type.title.pixelSize
        font.weight: Theme.type.title.weight
    }
}
