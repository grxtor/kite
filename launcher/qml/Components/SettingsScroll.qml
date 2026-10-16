// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

// One settings section: a page heading (what this section is, and why you'd
// come here) over a scrolling column of groups, kept to a readable width on
// wide windows.
//
// The column runs to the same right edge as the header above it: the
// scroll bar overlays the cards instead of keeping an empty gutter beside
// them, and the bottom rows fade out (ScrollFade) rather than being sliced
// off by the window edge while there is more below.
Item {
    id: root

    property string title
    property string description
    default property alias groups: column.data

    // Past this the rows' label-left/control-right layout stops reading as
    // one line; the column stops growing here on wide windows.
    readonly property int readableWidth: 760

    Accessible.name: title

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
            width: Math.min(flick.width, root.readableWidth)
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
                    lineHeight: Theme.type.label.lineHeight
                }
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
