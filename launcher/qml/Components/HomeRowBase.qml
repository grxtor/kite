// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What every "Continue playing" row of the Sahne, Sade and Gece looks does the same: hover, clicks (the host tells one,
 * two and three apart: select, open, launch), right-click the menu, keyboard (Space selects, Return opens, menu key
 * opens the menu), focus ring. `host` is HomeContinuePlaying. Children go in over the tap handlers, so a Play inside
 * one gets first refusal on a press.
 */
Item {
    id: root

    required property var row
    // HomeContinuePlaying: tap(), selectRequested, openRequested, launchRequested, stopRequested and menuRequested
    // are its; selectedId is the pick.
    required property Item host
    property int cornerRadius: Theme.radius.lg
    readonly property bool hovered: hoverHandler.hovered
    readonly property bool running: root.row.isRunning
    readonly property bool selected: root.host.selectedId === root.row.instanceId
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
    Keys.onSpacePressed: root.host.selectRequested(root.row.instanceId)
    Keys.onMenuPressed: root.openMenu(null)

    Accessible.role: Accessible.ListItem
    Accessible.name: root.row.name
    Accessible.description: Format.versionLine(root.row.loader, root.row.gameVersion)
    Accessible.selectable: true
    Accessible.selected: root.selected

    HoverHandler { id: hoverHandler }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.host.tap(root.row.instanceId)
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.openMenu(null)
    }

    FocusFrame { cornerRadius: root.cornerRadius; z: 1; visible: root.activeFocus }
}
