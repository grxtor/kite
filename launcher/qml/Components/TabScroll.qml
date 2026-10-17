// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The scrolling body of an instance tab that is a column of setting groups
 * (Settings, Game options). The tab strip above already names the tab, so
 * there is no heading here -- only an optional one-line description -- and
 * the column runs the full width of the page like every other tab's
 * content, instead of stopping short of the tab strip's right edge the way
 * SettingsScroll's readable-width column does.
 *
 * The scroll bar overlays the groups instead of keeping a gutter, and the
 * bottom rows fade out (ScrollFade) rather than being sliced off.
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
