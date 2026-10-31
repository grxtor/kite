// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The scrolling body of an instance tab that is a column of setting groups
 * (Settings, Game options). The tab strip names the tab, so there is no heading,
 * only an optional description; the column runs the page's full width, unlike
 * SettingsScroll's readable-width column. The scroll bar overlays the groups and
 * the bottom rows fade out (ScrollFade).
 */
Item {
    id: root

    property string description
    default property alias groups: column.data

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            width: flick.width
            spacing: Theme.space.xl

            Text {
                visible: root.description.length > 0
                width: parent.width
                text: root.description
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                lineHeight: Theme.type.label.lineHeight
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: flick
    }
}
