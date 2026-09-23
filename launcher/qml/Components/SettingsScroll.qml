// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

// One settings section: a page heading (what this section is, and why you'd
// come here) over a scrolling column of groups, kept to a readable width on
// wide windows.
Flickable {
    id: root

    property string title
    property string description
    default property alias groups: column.data

    contentWidth: width
    contentHeight: column.height + Theme.space.xxl
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    Accessible.name: title

    ScrollBar.vertical: ScrollBar {}

    Column {
        id: column
        width: Math.min(root.width - Theme.space.md, 760)
        y: Theme.space.xs
        spacing: Theme.space.xl

        Column {
            width: parent.width
            spacing: Theme.space.xxs

            Text {
                text: root.title
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.heading.pixelSize
                font.weight: Theme.type.heading.weight
            }

            Text {
                visible: root.description.length > 0
                width: parent.width
                text: root.description
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                lineHeight: 1.3
            }
        }
    }
}
