// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The capes an account owns, as one labelled group on its card: "No cape" first, then a tile per cape with its
 * name underneath, the one worn outlined. Picking one asks the host (`capeRequested`), which changes it through
 * the account's services; while that runs (`busy`) the tiles wait, and what went wrong (`error`) is said below.
 */
Column {
    id: root

    // accountSkinInfo()'s "capes": [{ id, alias, url }].
    property var capes: []
    property string currentCapeId: ""
    property bool busy: false
    property string error: ""

    // An empty id takes the cape off.
    signal capeRequested(string capeId)

    spacing: Theme.space.sm

    Row {
        spacing: Theme.space.sm

        Copy {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Cape")
            role: "bodyStrong"
        }
        BusyIndicator {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.busy
            running: visible
            width: Theme.icon.sm
            height: Theme.icon.sm
        }
    }

    Flow {
        width: parent.width
        spacing: Theme.space.sm

        CapeTile {
            label: qsTr("No cape")
            isNoCapeOption: true
            selected: root.currentCapeId.length === 0
            enabled: !root.busy
            onClicked: if (!selected) root.capeRequested("")
        }
        Repeater {
            model: root.capes
            delegate: CapeTile {
                required property var modelData
                label: modelData.alias
                capeUrl: modelData.url
                selected: modelData.id === root.currentCapeId
                enabled: !root.busy
                onClicked: if (!selected) root.capeRequested(modelData.id)
            }
        }
    }

    Copy {
        width: parent.width
        visible: root.error.length > 0
        text: root.error
        role: "caption"
        tone: "danger"
        wrapMode: Text.Wrap
    }
}
