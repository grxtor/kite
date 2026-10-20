// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The words at the top of Home's hero (HomeHero): the instance's name at
 * display size, what it runs and whether it crashed last time, and -- when
 * the hero has no room for its facts -- when it was last played and for how
 * long; while a launch runs, its stage and a bar.
 */
ColumnLayout {
    id: root

    property string name: ""
    // Format.versionLine()'s "Fabric 1.21.4".
    property string versionText: ""
    property bool hasCrashed: false
    property var lastLaunch: 0
    property string playTimeText: ""
    // Whether the line under the name also says when and how long.
    property bool showsWhen: true
    property string launchStatus: ""
    property real launchProgress: -1
    // Over a screenshot: the bar takes its media look.
    property bool onMedia: false
    property color ink: Theme.palette.textPrimary
    property color inkSecondary: Theme.palette.textSecondary
    property int progressMaxWidth: Theme.control.heightLg * 8

    // As wide as the name and what it runs need, whatever else the line
    // says. Rounded up: a layout rounds widths, and a name a fraction too
    // narrow elides.
    readonly property int naturalWidth: Math.ceil(Math.max(nameText.implicitWidth, identity.implicitWidth))
    readonly property bool launching: root.launchStatus.length > 0
    readonly property string playedText: Number(root.lastLaunch) > 0
                                         ? qsTr("Played %1").arg(Format.lastPlayed(root.lastLaunch))
                                         : qsTr("Never played")

    spacing: Theme.space.xs

    Text {
        id: nameText
        Layout.fillWidth: true
        text: root.name
        elide: Text.ElideRight
        color: root.ink
        font.family: Theme.font.heading
        font.pixelSize: Theme.type.display.pixelSize
        font.weight: Theme.type.display.weight
    }

    // "Fabric 1.21.4", the crash if it crashed; with showsWhen also
    // "• Played 2 h ago • ⌛ 3 h 15 min".
    Row {
        Layout.fillWidth: true
        spacing: Theme.space.sm

        // What it runs and whether it crashed: what the name's side
        // always needs, whatever else the line says.
        Row {
            id: identity
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.sm

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.versionText
                color: root.inkSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.hasCrashed
                iconName: "alert-triangle"
                size: Theme.icon.sm
                color: Theme.palette.warning
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.hasCrashed
                text: qsTr("Crashed last time")
                color: Theme.palette.warning
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.showsWhen && !root.hasCrashed
            text: "•  " + root.playedText
            color: root.inkSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
        }
        Row {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.showsWhen && root.playTimeText.length > 0
            leftPadding: Theme.space.sm
            spacing: Theme.space.xs
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "hourglass"
                size: Theme.icon.sm
                color: root.inkSecondary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.playTimeText
                color: root.inkSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.features: ({ "tnum": 1 })
            }
        }
    }

    // While a launch runs: what it is doing, and how far.
    Column {
        Layout.fillWidth: true
        Layout.maximumWidth: root.progressMaxWidth
        Layout.topMargin: Theme.space.sm
        visible: root.launching
        spacing: Theme.space.xs

        Text {
            width: parent.width
            text: root.launchProgress >= 0
                  ? qsTr("%1  •  %2%").arg(root.launchStatus).arg(Math.round(root.launchProgress * 100))
                  : root.launchStatus
            elide: Text.ElideRight
            color: root.inkSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
        LaunchProgressBar {
            width: parent.width
            onMedia: root.onMedia
            progress: root.launchProgress
        }
    }
}
