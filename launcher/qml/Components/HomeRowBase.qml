// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What every "Continue playing" row of the Sahne, Sade and Gece looks does
 * the same, so each look's row is only its drawing: hover, a click that
 * opens the instance page, a right-click for its menu, the keyboard (Return
 * opens, Space plays or stops, the menu key opens the menu) and a focus
 * ring. The row is one of HomeContinuePlaying's plain row objects; its
 * `host` is that list, whose signals carry the actions out. Children go in
 * as the row's content, over the tap handlers, so a Play inside one still
 * gets first refusal on a press.
 */
Item {
    id: root

    // One of HomeContinuePlaying's rows (instanceId, name, iconKey, ...).
    required property var row
    // HomeContinuePlaying: openRequested, launchRequested, stopRequested and
    // menuRequested(id, running, name, iconKey, anchor) are its signals.
    required property Item host
    // The focus ring's corner radius, around the row's own.
    property int cornerRadius: Theme.radius.lg
    // Whether the pointer is over the row (or its keyboard focus is on it).
    readonly property bool hovered: hoverHandler.hovered
    readonly property bool running: root.row.isRunning
    readonly property bool playable: root.row.isRunning || root.row.canLaunch

    function open() { root.host.openRequested(root.row.instanceId) }
    function togglePlay() {
        if (root.row.isRunning)
            root.host.stopRequested(root.row.instanceId)
        else
            root.host.launchRequested(root.row.instanceId)
    }
    // @p anchor: the ⋯ button, or null for the pointer.
    function openMenu(anchor) {
        root.host.menuRequested(root.row.instanceId, root.row.isRunning, root.row.name, root.row.iconKey, anchor)
    }

    activeFocusOnTab: true
    Keys.onReturnPressed: root.open()
    Keys.onSpacePressed: root.togglePlay()
    Keys.onMenuPressed: root.openMenu(null)

    Accessible.role: Accessible.ListItem
    Accessible.name: root.row.name
    Accessible.description: Format.versionLine(root.row.loader, root.row.gameVersion)

    HoverHandler { id: hoverHandler }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: { root.forceActiveFocus(); root.open() }
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: { root.forceActiveFocus(); root.openMenu(null) }
    }

    // Keyboard focus ring, outset so it never competes with the row's own
    // edge.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.space.xs
        z: 1
        radius: root.cornerRadius + Theme.space.xs
        color: "transparent"
        border.width: Theme.focus.width
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
