// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

T.TabBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    spacing: Theme.space.sm

    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex

        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        snapMode: ListView.SnapToItem

        highlightMoveDuration: Theme.motion.normal
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: Theme.control.heightLg
        preferredHighlightEnd: width - Theme.control.heightLg
    }

    background: Rectangle {
        color: "transparent"

        // One hairline for the whole bar rather than each TabButton drawing
        // its own bottom border -- otherwise adjoining tabs would double up
        // the line at the seam and it would read thicker than every other
        // divider in the UI.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.palette.divider
        }
    }
}
