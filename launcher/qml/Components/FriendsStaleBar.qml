// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * The bar over the friends panel's list when a poll failed after the lists
 * were read: they stay, and this says they may be out of date. The retry is
 * the button, so the words do not also say "try again"; what the server said
 * is the tip. Same as the Friends page's bar, without its "as of" time,
 * which does not fit the panel's width.
 */
NoticeBar {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // "Try again" was pressed and has not been answered.
    property bool retrying: false

    text: qsTr("Can't update friends right now.")
    actionText: root.retrying ? qsTr("Trying…") : qsTr("Try again")
    onActionTriggered: {
        if (root.retrying)
            return
        root.retrying = true
        root.controller.refresh()
    }

    Connections {
        target: root.controller
        function onActionFinished(action) {
            if (action === "refresh")
                root.retrying = false
        }
        // A refresh dropped because friends was turned off meanwhile is
        // never answered: the flag is for as long as it is failing.
        function onChanged() {
            if (root.controller.state !== "error")
                root.retrying = false
        }
    }

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && !!root.controller && root.controller.error.length > 0
    ToolTip.delay: 500
    ToolTip.text: root.controller ? root.controller.error : ""
}
