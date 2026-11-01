// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

// One settings section: a page heading over a scrolling column of groups, centred up to the page's
// maximum content width like the other pages. The scroll bar overlays the cards, and the bottom rows
// fade out (ScrollFade).
Item {
    id: root

    property string title
    property string description
    default property alias groups: column.data

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
            width: Math.min(flick.width, Theme.layout.maxContentWidth)
            x: Math.round((flick.width - width) / 2)
            y: Theme.space.xs
            spacing: Theme.space.xl

            Column {
                width: parent.width
                spacing: Theme.space.xxs

                Copy {
                    text: root.title
                    role: "heading"
                }

                Copy {
                    visible: root.description.length > 0
                    width: parent.width
                    text: root.description
                    tone: "secondary"
                    wrapMode: Text.Wrap
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
