// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Log lines in a scrolling list, shared by the live log, an instance's other
 * logs and the launcher's own. `model` is a LogModel (roles `line`, `level`) or a
 * plain array of strings. `matches` are the ascending row numbers containing the
 * search text; `current` points into it and scrolls to that row. With wrap off,
 * lines scroll sideways to the widest line seen so far.
 */
Item {
    id: root

    property var model: null
    property bool wrap: true
    property bool follow: false
    // Ascending row numbers of the lines that contain the search text.
    property var matches: []
    // Index into `matches` of the match being looked at, or -1.
    property int current: -1

    readonly property int count: view.count
    readonly property alias atEnd: view.atYEnd

    // The user scrolled by hand: a live view stops following.
    signal scrolledAway()
    // The user scrolled back to the end: a live view follows again.
    signal reachedEnd()

    // MessageLevel::Enum
    function levelColor(level) {
        switch (level) {
        case 2: case 8: case 9: return Theme.palette.danger   // StdErr, Error, Fatal
        case 7: return Theme.palette.warning                  // Warning
        case 3: return Theme.palette.accentText               // Kite
        case 4: return Theme.palette.textTertiary             // Debug
        default: return Theme.palette.textSecondary
        }
    }

    readonly property var matchSet: {
        var set = {}
        for (var i = 0; i < matches.length; ++i)
            set[matches[i]] = true
        return set
    }
    readonly property int currentRow: current >= 0 && current < matches.length ? matches[current] : -1

    onCurrentRowChanged: if (currentRow >= 0) view.positionViewAtIndex(currentRow, ListView.Contain)

    readonly property real currentMatchAlpha: 0.45

    property real widest: 0
    onModelChanged: widest = 0

    ListView {
        id: view
        anchors.fill: parent
        clip: true
        model: root.model
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        flickableDirection: root.wrap ? Flickable.VerticalFlick : Flickable.HorizontalAndVerticalFlick
        contentWidth: root.wrap ? width : Math.max(width, root.widest + Theme.space.md)
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}

        onCountChanged: if (root.follow) positionViewAtEnd()
        onMovementStarted: root.scrolledAway()
        onAtYEndChanged: if (atYEnd && moving) root.reachedEnd()

        delegate: Item {
            id: row
            required property int index
            // Roles of a LogModel (line, level), or `modelData` for an array.
            required property var model

            readonly property string lineText: row.model.line !== undefined ? row.model.line : row.model.modelData
            readonly property bool matched: root.matchSet[row.index] === true
            readonly property bool isCurrent: row.index === root.currentRow

            width: root.wrap ? view.width : Math.max(view.width, root.widest + Theme.space.md)
            height: label.implicitHeight

            Rectangle {
                anchors.fill: parent
                visible: row.matched
                radius: Theme.radius.sm
                color: row.isCurrent ? Qt.alpha(Theme.palette.warning, root.currentMatchAlpha)
                                     : Theme.palette.warningSubtle
            }

            Text {
                id: label
                width: root.wrap ? parent.width - Theme.space.md : implicitWidth
                text: row.lineText
                wrapMode: root.wrap ? Text.WrapAnywhere : Text.NoWrap
                color: row.model.level !== undefined ? root.levelColor(row.model.level) : Theme.palette.textSecondary
                font.family: Theme.font.mono
                font.pixelSize: Theme.type.label.pixelSize
                lineHeight: Theme.type.label.lineHeight
                textFormat: Text.PlainText

                onImplicitWidthChanged: if (!root.wrap && implicitWidth > root.widest) root.widest = implicitWidth
                Component.onCompleted: if (!root.wrap && implicitWidth > root.widest) root.widest = implicitWidth
            }
        }
    }
}
