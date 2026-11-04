// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The words at the top of Home's hero: the instance's name, what it runs and whether it crashed last time, and,
 * when the hero has no room for its facts, when it was last played and for how long; while a launch runs, its
 * stage and a bar. Sahne, Sade and Gece set `nameSize` and turn the line under the name into icons (`iconMeta`).
 */
ColumnLayout {
    id: root

    property string name: ""
    property string versionText: ""
    property bool hasCrashed: false
    property string crashCause: ""
    property var lastLaunch: 0
    property string playTimeText: ""
    property bool showsWhen: true
    property string launchStatus: ""
    property real launchProgress: -1
    property bool onMedia: false
    property color ink: Theme.palette.textPrimary
    property color inkSecondary: Theme.palette.textSecondary
    property int progressMaxWidth: Theme.control.heightLg * 8
    property int nameSize: Theme.type.display.pixelSize
    property real nameLineHeight: 1.0
    property bool iconMeta: false
    property int metaSize: Theme.type.body.pixelSize
    property int metaSpacing: Theme.space.lg

    // Rounded up: a layout rounds widths, and a name a fraction too narrow elides.
    readonly property int naturalWidth: Math.ceil(Math.max(nameText.implicitWidth, identity.implicitWidth))
    readonly property bool launching: root.launchStatus.length > 0
    readonly property string playedText: Number(root.lastLaunch) > 0
                                         ? qsTr("Played %1").arg(Format.lastPlayed(root.lastLaunch))
                                         : qsTr("Never played")

    readonly property int metaIconSize: Theme.icon.xs

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
                text: Format.crashLine(root.crashCause)
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

    // The play time drops to a second line rather than run under Play when the column is narrow.
    Flow {
        Layout.fillWidth: true
        visible: !root.iconMeta
        spacing: Theme.space.sm

        Row {
            id: identity
            spacing: Theme.space.sm

            Copy {
                anchors.verticalCenter: parent.verticalCenter
                text: root.versionText
                color: root.inkSecondary
            }
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.hasCrashed
                iconName: "alert-triangle"
                size: Theme.icon.sm
                color: Theme.palette.warning
            }
            Copy {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.hasCrashed
                text: Format.crashLine(root.crashCause)
                tone: "warning"
            }
        }
        Copy {
            visible: root.showsWhen && !root.hasCrashed
            text: "•  " + root.playedText
            color: root.inkSecondary
            // The gap to the play time; at the end of a line it is not seen.
            rightPadding: Theme.space.sm
        }
        Row {
            visible: root.showsWhen && root.playTimeText.length > 0
            spacing: Theme.space.xs
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "hourglass"
                size: Theme.icon.sm
                color: root.inkSecondary
            }
            Copy {
                anchors.verticalCenter: parent.verticalCenter
                text: root.playTimeText
                color: root.inkSecondary
                font.features: ({ "tnum": 1 })
            }
        }
    }

    Column {
        Layout.fillWidth: true
        Layout.maximumWidth: root.progressMaxWidth
        Layout.topMargin: Theme.space.sm
        visible: root.launching
        spacing: Theme.space.xs

        Copy {
            width: parent.width
            text: root.launchProgress >= 0
                  ? qsTr("%1  •  %2%").arg(root.launchStatus).arg(Math.round(root.launchProgress * 100))
                  : root.launchStatus
            role: "label"
            elide: Text.ElideRight
            color: root.inkSecondary
        }
        LaunchProgressBar {
            width: parent.width
            onMedia: root.onMedia
            progress: root.launchProgress
        }
    }
}
