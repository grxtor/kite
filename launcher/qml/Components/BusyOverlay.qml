// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Shown while the core runs something the user has to wait for and cannot
 * interact around (UiHost::showBusy): a dimmed window, a spinner and what
 * is happening. It swallows input, as the modal progress dialog did.
 *
 * Some waits offer one way out ("Play offline" while an account refreshes
 * before a launch): `skipLabel` names it, and the button below emits
 * skipRequested() for Main.qml to pass on (QmlUiHost::skipBusy()).
 */
Rectangle {
    id: root

    property bool busy: false
    property string text
    property string skipLabel

    signal skipRequested()

    anchors.fill: parent
    z: 900
    color: Theme.palette.scrim
    opacity: busy ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }

    // Eat clicks and wheel so nothing underneath reacts.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(360, parent.width - Theme.space.xxl * 2)
        height: column.implicitHeight + Theme.space.xl * 2
        radius: Theme.radius.xl
        color: Theme.palette.surfaceRaised
        border.width: 1
        border.color: Theme.palette.border

        Column {
            id: column
            anchors.centerIn: parent
            width: parent.width - Theme.space.xl * 2
            spacing: Theme.space.md
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: root.busy
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: root.text.length > 0 ? root.text : qsTr("Working…")
                wrapMode: Text.Wrap
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: root.skipLabel.length > 0
                text: root.skipLabel
                onClicked: root.skipRequested()
            }
        }
    }
}
