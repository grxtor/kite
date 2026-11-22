// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The one line that says the machine is offline, above the page: shown while NetworkStatus (`network`,
 * QmlShell.network) reports no connection, gone by itself when it returns. "Try again" reads the state anew
 * and says stillOffline() when nothing changed. Takes no room while hidden.
 */
Item {
    id: root

    property var network: null
    // The page column's side margin, so the line stands over the page's content.
    property int sideMargin: Theme.space.xl

    readonly property bool shown: !!root.network && root.network.online === false

    signal stillOffline()

    implicitHeight: root.shown ? bar.implicitHeight + Theme.space.md : 0
    visible: height > 0
    clip: true

    Behavior on implicitHeight {
        NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing }
    }

    NoticeBar {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        text: qsTr("You're offline. Installed instances still play; Discover and downloads need a connection.")
        actionText: qsTr("Try again")
        onActionTriggered: {
            if (root.network && !root.network.recheck())
                root.stillOffline()
        }
    }
}
