// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One account: face, name, what kind it is and whether it still works,
 * and the account actions. The default account -- the one games launch
 * with -- is marked; any other can be made default in one click.
 */
Rectangle {
    id: root

    required property int index
    required property string profileName
    required property string name
    required property bool isDefault
    required property bool isMSA
    required property string stateKey
    required property string status
    required property string accountId

    signal makeDefaultRequested()
    signal refreshRequested()
    signal removeRequested()

    readonly property string shownName: profileName.length > 0 ? profileName : name

    function stateTone(key) {
        switch (key) {
        case "online": return "success"
        case "working": return "info"
        case "errored": case "gone": return "danger"
        case "expired": return "warning"
        default: return "neutral"
        }
    }

    implicitHeight: 76
    radius: Theme.radius.lg
    color: root.isDefault ? Theme.palette.surfaceRaised : Theme.palette.surface
    border.width: root.isDefault ? 2 : 1
    border.color: root.isDefault ? Theme.palette.accent : Theme.palette.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.md
        spacing: Theme.space.md

        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: Theme.radius.md
            color: Theme.palette.accentSubtle

            Text {
                anchors.centerIn: parent
                text: root.shownName.charAt(0).toUpperCase()
                color: Theme.palette.accent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.heading.pixelSize
                font.weight: Font.Bold
            }
            // Drawn over the initial; an account without a skin comes back
            // transparent and the initial shows through.
            Image {
                anchors.fill: parent
                anchors.margins: 2
                source: root.accountId.length > 0 ? "image://accountface/" + root.accountId : ""
                sourceSize: Qt.size(80, 80)
                smooth: false
            }
        }

        Column {
            Layout.fillWidth: true
            spacing: Theme.space.xs

            Row {
                spacing: Theme.space.sm
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.shownName
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }
                StatusBadge {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.isDefault
                    tone: "info"
                    text: qsTr("Default")
                }
            }

            Row {
                spacing: Theme.space.sm
                Tag {
                    iconName: root.isMSA ? "user" : "users"
                    text: root.isMSA ? qsTr("Microsoft") : qsTr("Offline")
                }
                StatusBadge {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.isMSA && root.status.length > 0
                    tone: root.stateTone(root.stateKey)
                    text: root.status
                }
            }
        }

        Button {
            visible: !root.isDefault
            text: qsTr("Use this account")
            onClicked: root.makeDefaultRequested()
        }
        IconButton {
            visible: root.isMSA
            iconName: "refresh"
            tip: qsTr("Sign in again")
            onClicked: root.refreshRequested()
        }
        IconButton {
            iconName: "trash"
            tip: qsTr("Remove account")
            onClicked: root.removeRequested()
        }
    }
}
