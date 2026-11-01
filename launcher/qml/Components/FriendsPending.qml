// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Whether a request is on its way on the Requests and Blocked tabs: the rows' buttons wait (`has(uuid)`) from a
 * click until FriendsController says how it went (actionFinished). One request at a time for the whole tab: the
 * answer names the action, not the person.
 */
QtObject {
    id: root

    // FriendsController, or null.
    property var controller: null
    property var actions: []
    property bool waiting: false

    function has(uuid) {
        return root.waiting
    }
    function send(uuid, call) {
        if (root.waiting)
            return
        root.waiting = true
        call()
    }

    property Connections answers: Connections {
        target: root.controller
        function onActionFinished(action) {
            if (root.actions.indexOf(action) >= 0)
                root.waiting = false
        }
        // Turned off with one out: its answer is dropped and would never free the buttons when Friends is turned on
        // again.
        function onChanged() {
            if (root.waiting && !root.controller.enabled)
                root.waiting = false
        }
    }
}
