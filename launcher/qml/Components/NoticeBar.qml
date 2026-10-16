// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One line about the state of the page above its content -- "The game is
 * running...", "This instance has no mod loader" -- in the same tinted
 * block InstanceSettingsTab uses for its own lock notice, with an optional
 * action at its end.
 */
Rectangle {
    id: root

    property string text: ""
    property string tone: "warning" // warning | info
    property string actionText: ""
    signal actionTriggered()

    implicitHeight: Math.max(label.implicitHeight + Theme.space.md * 2,
                             action.visible ? action.implicitHeight + Theme.space.sm * 2 : 0)
    radius: Theme.radius.lg
    color: root.tone === "info" ? Theme.palette.infoSubtle : Theme.palette.warningSubtle

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.md
        anchors.rightMargin: action.visible ? Theme.space.sm : Theme.space.md
        spacing: Theme.space.md

        Text {
            id: label
            Layout.fillWidth: true
            text: root.text
            wrapMode: Text.Wrap
            color: root.tone === "info" ? Theme.palette.info : Theme.palette.warning
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.Medium
        }

        Button {
            id: action
            visible: root.actionText.length > 0
            text: root.actionText
            onClicked: root.actionTriggered()
        }
    }
}
