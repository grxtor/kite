// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The one-time "Friends is on" card, at the top of the Friends page and of
 * Home's friends panel: friends are on from the start, so the first time
 * they connect this says what your friends can see (and that they can join
 * you) and where to change it. What it says follows the account's settings,
 * which live on the server: an account that is invisible, hides its server or
 * does not let friends join (set on another computer, say) is not told the
 * opposite, so it waits until they are read. Either button settles it for
 * good (the FriendsNoticeSeen setting), so the card goes from the page and
 * the panel at once. Settings also asks for the Friends section of Settings
 * (`settingsRequested`).
 *
 * It is a tinted message, not a card, so it does not read as the page's
 * own furniture next to the self card, and lays itself out for its width.
 * On the page it is the whole disclosure, with its buttons beside it; in
 * the narrow panel it is two short lines, a link to Settings and a close,
 * so it does not push the friends below it out of sight.
 */
Rectangle {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null

    // Connected, the settings and lists read (the settings come first), and
    // not yet read by the user.
    readonly property bool due: !!root.controller && root.controller.enabled
                                && root.controller.state === "ready"
                                && root.controller.loaded
                                && !SettingsStore.bool("FriendsNoticeSeen")
    // What the account shows of you (FriendsController.settings).
    readonly property var privacy: root.controller ? root.controller.settings : ({})
    readonly property bool invisible: root.privacy.visibility === "invisible"
    readonly property bool serverShown: !root.invisible && !root.privacy.hideServer
    readonly property bool joinable: root.serverShown && root.privacy.allowJoin !== false
    // The words, for the page and for the narrow panel.
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
    // Below this the short form.
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
                // Level with the first line of text, not the card's edge.
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
