// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The one-time "Friends is on" card atop the Friends page and Home's friends panel: says what friends can see of
 * you and where to change it. It waits until the account's settings (kept on the server) are read. Either button
 * settles it for good (FriendsNoticeSeen); Settings asks for the Friends section (`settingsRequested`).
 */
Rectangle {
    id: root

    // FriendsController, or null.
    property var controller: null

    // Connected, settings and lists read, and not yet dismissed.
    readonly property bool due: !!root.controller && root.controller.enabled
                                && root.controller.state === "ready"
                                && root.controller.loaded
                                && !SettingsStore.bool("FriendsNoticeSeen")
    readonly property var privacy: root.controller ? root.controller.settings : ({})
    readonly property bool invisible: root.privacy.visibility === "invisible"
    readonly property bool serverShown: !root.invisible && !root.privacy.hideServer
    readonly property bool joinable: root.serverShown && root.privacy.allowJoin !== false
    readonly property string fullText: root.invisible
        ? qsTr("Friends is on. You are invisible: your friends see you as offline, and nothing about what you play.")
        : !root.serverShown
          ? qsTr("Friends is on. Your friends can see when you're online and what you're playing: instance and Minecraft version, but not the server.")
        : !root.joinable
          ? qsTr("Friends is on. Your friends can see when you're online and what you're playing: instance, Minecraft version and server.")
        : qsTr("Friends is on. Your friends can see when you're online and what you're playing: instance, Minecraft version and server. They can join you there.")
    readonly property string shortText: root.invisible
        ? qsTr("Friends is on. You appear offline.")
        : !root.serverShown ? qsTr("Friends is on. Your friends see you online, but not your server.")
        : !root.joinable ? qsTr("Friends is on. Your friends see your server.")
        : qsTr("Friends is on. Your friends see your server and can join you.")
    readonly property int fullWidth: 520
    readonly property bool wide: root.width >= root.fullWidth

    signal settingsRequested()

    function dismiss() {
        SettingsStore.setValue("FriendsNoticeSeen", true)
    }
    function openSettings() {
        root.dismiss()
        root.settingsRequested()
    }

    visible: root.due
    implicitHeight: (root.wide ? fullLayout.implicitHeight : shortLayout.implicitHeight) + Theme.space.md * 2
    radius: Theme.radius.lg
    color: Theme.palette.accentSubtle

    RowLayout {
        id: fullLayout
        visible: root.wide
        anchors.fill: parent
        anchors.margins: Theme.space.md
        spacing: Theme.space.md

        MeshIcon {
            Layout.alignment: Qt.AlignVCenter
            iconName: "users"
            size: Theme.icon.lg
            color: Theme.palette.accentText
        }
        PanelNote {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.fullText
        }
        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.sm

            Button {
                buttonSize: "sm"
                variant: "ghost"
                text: qsTr("Settings")
                onClicked: root.openSettings()
            }
            Button {
                buttonSize: "sm"
                text: qsTr("Got it")
                onClicked: root.dismiss()
            }
        }
    }

    ColumnLayout {
        id: shortLayout
        visible: !root.wide
        anchors.fill: parent
        anchors.margins: Theme.space.md
        spacing: Theme.space.xs

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            PanelNote {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                text: root.shortText
            }
            IconButton {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: -Theme.space.xs
                Layout.rightMargin: -Theme.space.xs
                size: Theme.control.heightSm
                iconName: "x"
                tip: qsTr("Got it")
                onClicked: root.dismiss()
            }
        }
        LinkButton {
            id: settingsLink
            Layout.alignment: Qt.AlignLeft
            // Its text starts on the card's text, not a chevron's bearing in.
            Layout.leftMargin: -settingsLink.chevronBearing
            text: qsTr("Settings")
            onClicked: root.openSettings()
        }
    }
}
