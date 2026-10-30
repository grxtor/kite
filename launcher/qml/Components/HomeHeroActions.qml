// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The hero's buttons, whatever the look: Play (Stop while the game runs,
 * "Starting…" and Cancel while a launch prepares it) and ▾ for the other
 * ways to play (LaunchOptionsMenu, as on the play bar). Everything it says
 * and does goes through the hero it belongs to (HomeHero), which holds the
 * instance and the signals; the look only chooses the size, whether ▾ is a
 * translucent glass square (over a photo or the sky).
 */
Row {
    id: root

    // HomeHero: the instance's state, the launch options and the signals.
    required property Item hero
    property int size: Theme.control.heightLg
    property int playWidth: 176
    // ▾ as a glass square instead of the Style button.
    property bool glass: false
    property color glassFill: Qt.rgba(1, 1, 1, 0.12)

    readonly property bool canChoose: root.hero.canLaunch && !!root.hero.launchOptionsFor

    function openLaunchMenu(button) {
        launchMenu.refresh()
        launchMenu.popup(button, button.width - launchMenu.implicitWidth, button.height + Theme.space.xs)
    }

    spacing: Theme.space.sm

    Button {
        anchors.verticalCenter: parent.verticalCenter
        visible: root.hero.launching
        buttonSize: "lg"
        text: qsTr("Cancel")
        onClicked: root.hero.stopRequested(root.hero.instanceId)
    }

    Item {
        anchors.verticalCenter: parent.verticalCenter
        width: play.width
        height: play.height

        PlayButton {
            id: play
            round: false
            size: root.size
            implicitWidth: root.playWidth
            running: root.hero.isRunning && !root.hero.launching
            busy: root.hero.launching
            enabled: !root.hero.launching && (root.hero.isRunning || root.hero.canLaunch)
            onClicked: root.hero.isRunning ? root.hero.stopRequested(root.hero.instanceId)
                                           : root.hero.launchRequested(root.hero.instanceId)
        }
    }

    IconButton {
        id: moreButton
        anchors.verticalCenter: parent.verticalCenter
        visible: !root.glass && !root.hero.isRunning && !root.hero.launching
        size: root.size
        flat: false
        iconName: "chevron-down"
        icon.width: Theme.icon.md
        icon.height: Theme.icon.md
        tip: qsTr("More ways to play")
        enabled: root.canChoose
        onClicked: launchMenu.visible ? launchMenu.close() : root.openLaunchMenu(moreButton)
    }
    HomeGlassButton {
        id: glassButton
        anchors.verticalCenter: parent.verticalCenter
        visible: root.glass && !root.hero.isRunning && !root.hero.launching
        size: root.size
        fill: root.glassFill
        tip: qsTr("More ways to play")
        enabled: root.canChoose
        onClicked: launchMenu.visible ? launchMenu.close() : root.openLaunchMenu(glassButton)
    }

    LaunchOptionsMenu {
        id: launchMenu
        instanceId: root.hero.instanceId
        optionsFor: root.hero.launchOptionsFor
        onLaunchModeRequested: (id, mode) => root.hero.launchModeRequested(id, mode)
        onLaunchAsRequested: (id, accountId) => root.hero.launchAsRequested(id, accountId)
        onProfilerChosen: (id, key) => root.hero.profilerChosen(id, key)
        onPerformanceChosen: (id, enabled) => root.hero.performanceChosen(id, enabled)
        onNavigateRequested: (target) => root.hero.navigateRequested(target)
    }
}
