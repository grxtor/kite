// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Not "SearchField": Qt 6.10 added a QtQuick.Controls.SearchField, which
 * would win the name over this file wherever QtQuick.Controls is imported.
 *
 * TextField with a leading magnifier and a trailing clear button. Escape
 * clears too, so a keyboard user can back out of a search without reaching
 * for the mouse.
 */
TextField {
    id: control

    leftPadding: Theme.space.md + Theme.icon.sm + Theme.space.sm
    rightPadding: clearButton.visible ? clearButton.width + Theme.space.xs : Theme.space.md
    implicitHeight: Theme.control.height
    selectByMouse: true

    Keys.onEscapePressed: (event) => {
        if (control.text.length > 0) {
            control.clear()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    MeshIcon {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space.md
        anchors.verticalCenter: parent.verticalCenter
        iconName: "search"
        size: Theme.icon.sm
        color: control.activeFocus ? Theme.palette.textSecondary : Theme.palette.textTertiary
    }

    IconButton {
        id: clearButton
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.xxs
        anchors.verticalCenter: parent.verticalCenter
        size: Theme.control.heightSm
        iconName: "x"
        tip: qsTr("Clear search")
        visible: control.text.length > 0
        focusPolicy: Qt.NoFocus
        onClicked: control.clear()
    }
}
