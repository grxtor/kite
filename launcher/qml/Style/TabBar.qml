// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// A row of text tabs (TabButton.qml) over a 1 px hairline that runs under the
// whole row; one 2 px accent underline sits on it and slides to the active tab.
T.TabBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    spacing: 0

    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex

        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        snapMode: ListView.SnapToItem

        // The one underline, sliding and resizing to the current tab.
        highlight: Item {
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: Theme.palette.accent
            }
        }
        highlightMoveDuration: Theme.motion.slow
        highlightMoveVelocity: -1
        highlightResizeDuration: Theme.motion.slow
        highlightResizeVelocity: -1
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: Theme.control.heightLg
        preferredHighlightEnd: width - Theme.control.heightLg
    }

    background: Item {
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.palette.border
        }
    }
}
