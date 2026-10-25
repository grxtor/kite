// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Whether a request is on its way, on the Friends page's Requests and
 * Blocked tabs: the rows' buttons wait (`has(uuid)`) from a click until
 * FriendsController says how it went (actionFinished), so a double click or
 * Accept then Decline does not send two requests that contradict each
 * other. `actions` are the controller actions this tab sends.
 *
 * One request at a time for the whole tab, not one per person: the answer
 * names the action, not the person, so with two out there would be no
 * telling whose came back, and the other row would be released while its own
 * request is still out.
 */
QtObject {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    property var actions: []
    // A request is out.
    property bool waiting: false

    // Whether @p uuid has to wait: any person does while a request is out.
    function has(uuid) {
        return root.waiting
    }
    // Runs @p call for @p uuid, unless a request is already out.
    function send(uuid, call) {
        if (root.waiting)
            return
        root.waiting = true
        call()
    }

    property Connections answers: Connections {
        target: root.controller
        // Whatever it was, the answer is in: the next click is a new question.
        function onActionFinished(action) {
            if (root.actions.indexOf(action) >= 0)
                root.waiting = false
        }
        // Turned off with one out: its answer is dropped, and would never
        // free the buttons when Friends is turned on again.
        function onChanged() {
            if (root.waiting && !root.controller.enabled)
                root.waiting = false
        }
    }
}
