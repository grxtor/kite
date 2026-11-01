// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * The bar over a friends list when a poll failed after the lists were read: they stay, and this says they may be out
 * of date, with the time they were read when `updatedAt` is given (the panel is too narrow for it).
 */
NoticeBar {
    id: root

    // FriendsController, or null.
    property var controller: null
    property string updatedAt: ""

    text: root.updatedAt.length > 0 ? qsTr("Can't update friends right now. Showing them as of %1.").arg(root.updatedAt)
                                    : qsTr("Can't update friends right now.")
    actionText: retry.retrying ? qsTr("Trying…") : qsTr("Try again")
    onActionTriggered: retry.run()

    FriendsRetry {
        id: retry
        controller: root.controller
    }

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && !!root.controller && root.controller.error.length > 0
    ToolTip.delay: 500
    ToolTip.text: root.controller ? root.controller.error : ""
}
