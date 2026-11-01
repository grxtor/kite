// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * A "Try again" on a friends view that waits for its answer: `retrying` from `run()` until FriendsController says how
 * the refresh went (actionFinished). One per view; `run()` does nothing while a refresh is out.
 */
QtObject {
    id: root

    // FriendsController, or null.
    property var controller: null
    property bool retrying: false

    function run() {
        if (!root.controller || root.retrying)
            return
        root.retrying = true
        root.controller.refresh()
    }

    property Connections answers: Connections {
        target: root.controller
        function onActionFinished(action) {
            if (action === "refresh")
                root.retrying = false
        }
        // A refresh dropped because friends was turned off meanwhile is never answered: the flag lasts while it is
        // failing.
        function onChanged() {
            if (root.controller.state !== "error")
                root.retrying = false
        }
    }
}
