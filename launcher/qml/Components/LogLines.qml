// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Log lines in a scrolling list, shared by the live log, an instance's
 * other logs and the launcher's own logs.
 *
 * `model` is either a LogModel (roles `line` and `level`, coloured by level)
 * or a plain array of strings (one per line, all in the default colour).
 * Find results come in as `matches`, the ascending row numbers that contain
 * the search text: those rows get a soft highlight, and the one `current`
 * points at a stronger one. Setting `current` also scrolls to it.
 *
 * Wrap off keeps every line on one row and scrolls sideways instead, which is
 * what you want for a stack trace; the width it scrolls to is the widest line
 * seen so far.
 */
Item {
    id: root

    property var model: null
    property bool wrap: true
    // Keep the newest line in view as lines arrive (the live log only).
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
        case 3: return Theme.palette.accent                   // MeshMC
        case 4: return Theme.palette.textTertiary             // Debug
        default: return Theme.palette.textSecondary
        }
    }

    function positionAtEnd() {
        view.positionViewAtEnd()
    }

    // The rows in `matches`, as a lookup for the delegates.
    readonly property var matchSet: {
        var set = {}
        for (var i = 0; i < matches.length; ++i)
            set[matches[i]] = true
        return set
    }
    readonly property int currentRow: current >= 0 && current < matches.length ? matches[current] : -1

    onCurrentRowChanged: if (currentRow >= 0) view.positionViewAtIndex(currentRow, ListView.Contain)

    // How much of the warning colour the match being looked at gets, over
    // the soft highlight every match has.
    readonly property real currentMatchAlpha: 0.45

    // The widest line seen, for the sideways scroll range when not wrapping.
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
                font.pixelSize: Theme.type.label.pixelSize - 1
                textFormat: Text.PlainText

                onImplicitWidthChanged: if (!root.wrap && implicitWidth > root.widest) root.widest = implicitWidth
                Component.onCompleted: if (!root.wrap && implicitWidth > root.widest) root.widest = implicitWidth
            }
        }
    }
}
