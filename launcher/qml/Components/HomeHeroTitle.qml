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
 *
 * The Sahne, Sade and Gece looks set it bigger or smaller (`nameSize`) and
 * turn the line under the name into icons (`iconMeta`: layers, clock,
 * hourglass), which always says when and how long.
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
    // The name's size and line height (a multiplier); the Sahne look sets
    // 48, Sade 36, Gece 46.
    property int nameSize: Theme.type.display.pixelSize
    property real nameLineHeight: 1.0
    // The line under the name as icons and facts (layers + what it runs,
    // clock + when, hourglass + how long) instead of words and bullets.
    property bool iconMeta: false
    property int metaSize: Theme.type.body.pixelSize
    // Between the facts of the icon line.
    property int metaSpacing: Theme.space.lg

    // As wide as the name and what it runs need, whatever else the line
    // says. Rounded up: a layout rounds widths, and a name a fraction too
    // narrow elides.
    readonly property int naturalWidth: Math.ceil(Math.max(nameText.implicitWidth, identity.implicitWidth))
    readonly property bool launching: root.launchStatus.length > 0
    readonly property string playedText: Number(root.lastLaunch) > 0
                                         ? qsTr("Played %1").arg(Format.lastPlayed(root.lastLaunch))
                                         : qsTr("Never played")

    readonly property int metaIconSize: 16

    // One fact of the icon line: its icon, then the words.
    component MetaFact: Row {
        property string iconName
        property string text
        property bool tabular: false
        spacing: Theme.space.sm - 1

        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: parent.iconName
            size: root.metaIconSize
            color: root.inkSecondary
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: parent.text
            color: root.inkSecondary
            font.family: Theme.font.family
            font.pixelSize: root.metaSize
            font.features: parent.tabular ? ({ "tnum": 1 }) : ({})
        }
    }

    spacing: Theme.space.xs

    Text {
        id: nameText
        Layout.fillWidth: true
        text: root.name
        // A name too long for the room steps down to the heading size
        // before it elides.
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: Theme.type.heading.pixelSize
        elide: Text.ElideRight
        color: root.ink
        lineHeight: root.nameLineHeight
        lineHeightMode: Text.ProportionalHeight
        font.family: Theme.font.heading
        font.pixelSize: root.nameSize
        font.weight: Theme.type.display.weight
    }

    // The icon line: what it runs, the crash if it crashed (else when it
    // was played), how long it has been played.
    Row {
        Layout.fillWidth: true
        visible: root.iconMeta
        spacing: root.metaSpacing

        MetaFact {
            iconName: "layers"
            text: root.versionText
        }
        Row {
            visible: root.hasCrashed
            spacing: Theme.space.xs
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "alert-triangle"
                size: root.metaIconSize
                color: Theme.palette.warning
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Crashed last time")
                color: Theme.palette.warning
                font.family: Theme.font.family
                font.pixelSize: root.metaSize
            }
        }
        MetaFact {
            visible: !root.hasCrashed
            iconName: "clock"
            text: Format.lastPlayed(root.lastLaunch)
        }
        MetaFact {
            visible: root.playTimeText.length > 0
            iconName: "hourglass"
            text: root.playTimeText
            tabular: true
        }
    }

    // "Fabric 1.21.4", the crash if it crashed; with showsWhen also
    // "• Played 2 h ago • ⌛ 3 h 15 min".
    Row {
        Layout.fillWidth: true
        visible: !root.iconMeta
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
