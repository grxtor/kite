// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What every "Continue playing" row of the Sahne, Sade and Gece looks does the same: hover, a click (the host opens
 * the instance's page), right-click the menu, keyboard (Space selects, Return opens, menu key opens the menu), focus
 * ring. `host` is HomeContinuePlaying. Children go in over the tap handlers, so a Play inside one gets first refusal
 * on a press.
 */
Item {
    id: root

    required property var row
    // HomeContinuePlaying: tap(), selectRequested, openRequested, launchRequested, stopRequested and menuRequested
    // are its; selectedId is the pick.
    required property Item host
    property int cornerRadius: Theme.radius.lg
    // How far the row's content lifts under the pointer: Theme.motion.lift for a card, 0 for a flat row. The hover
    // area itself stays put, so the edge never slides out from under the pointer.
    property int lift: 0
    default property alias content: liftBox.data
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
        id: leftTap
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.host.tap(root.row.instanceId)
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.openMenu(null)
    }

    Item {
        id: liftBox
        anchors.fill: parent
        transform: Translate {
            y: !root.hovered || root.lift === 0 ? 0 : leftTap.pressed ? -1 : -root.lift
            Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }

        FocusFrame { cornerRadius: root.cornerRadius; z: 1; visible: root.activeFocus }
    }
}
