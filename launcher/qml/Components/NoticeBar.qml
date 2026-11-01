// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One line about the state of the page above its content ("The game is
 * running...", "Updating 3 files...") with an optional action at its end and,
 * for work in progress, a progress line along its foot.
 */
Rectangle {
    id: root

    property string text: ""
    property string tone: "warning" // warning | info
    property string actionText: ""
    // 0..1, or -1 for "busy"; anything below -1 shows no progress line.
    property real progress: -2
    signal actionTriggered()

    implicitHeight: Math.max(label.implicitHeight + Theme.space.md * 2,
                             action.visible ? action.implicitHeight + Theme.space.sm * 2 : 0)
    radius: Theme.radius.lg
    color: Theme.tone(root.tone).soft
    clip: true

    LaunchProgressBar {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space.md
        anchors.rightMargin: Theme.space.md
        anchors.bottomMargin: Theme.space.xs
        height: Theme.space.xxs
        visible: root.progress >= -1
        progress: root.progress
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.md
        anchors.rightMargin: action.visible ? Theme.space.sm : Theme.space.md
        spacing: Theme.space.md

        Copy {
            id: label
            Layout.fillWidth: true
            text: root.text
            role: "label"
            wrapMode: Text.Wrap
            color: Theme.tone(root.tone).ink
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
