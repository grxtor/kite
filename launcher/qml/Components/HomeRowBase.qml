// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What every "Continue playing" row of the Sahne, Sade and Gece looks does the same: hover, click opens the
 * instance page, right-click the menu, keyboard (Return opens, Space plays or stops, menu key opens the menu),
 * focus ring. `host` is HomeContinuePlaying. Children go in over the tap handlers, so a Play inside one gets first
 * refusal on a press.
 */
Item {
    id: root

    required property var row
    // HomeContinuePlaying: openRequested, launchRequested, stopRequested and menuRequested are its signals.
    required property Item host
    property int cornerRadius: Theme.radius.lg
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

    FocusFrame { cornerRadius: root.cornerRadius; z: 1; visible: root.activeFocus }
}
