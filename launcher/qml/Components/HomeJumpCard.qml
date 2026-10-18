// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One "Jump back in" row on the Home page, as in Modrinth App: an 80px row
 * with the instance's icon square (the library card's, smaller), its name,
 * one line of what it runs and when it was last played, how long it has
 * been played, and Play. Used through a wrapper against the shell's
 * recentModel, with the same named roles InstanceCard reads.
 *
 * Clicking the row opens the instance page; Play plays it (Stop while it
 * runs). After a crash the sub-line says "Crashed last time" in place of
 * when -- a calm reminder, not an alarm.
 */
Item {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch
    required property var totalTimePlayed
    required property string gameVersion
    required property string loader
    required property color iconTint
    // Newest screenshot url (InstanceList's coverImage role), or "". Not
    // shown in the row (HomePage puts the played-last one in its header);
    // kept so the row takes the same roles as the library's card.
    required property string coverImage
    // A platform pack's own logo (InstanceList's packArt role), or "": shown
    // across the icon square, as on the library card.
    property string packArt: ""
    // Set by LaunchTask around the game process exit.
    required property bool hasCrashed

    signal clicked()
    signal playRequested()
    signal stopRequested()

    readonly property bool hovered: hoverHandler.hovered
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready
    // Modrinth App's row: 80px, a 48px icon square.
    readonly property int rowHeight: 80
    readonly property int squareSize: 48
    readonly property int iconSize: 32
    // Empty (so not shown) when "Show play time per instance" is off.
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime")
                                           ? Format.playTime(root.totalTimePlayed) : ""
    readonly property string versionText: Format.versionLine(root.loader, root.gameVersion)
    // Play time is the part that gives way on a narrow row.
    readonly property int playTimeMinWidth: 520

    implicitWidth: 640
    implicitHeight: root.rowHeight

    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.isRunning ? root.stopRequested() : root.playRequested()

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: root.versionText

    HoverHandler { id: hoverHandler }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radius.lg
        color: root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Shadow {
            radius: card.radius
            level: Theme.elevation.sm
            opacity: root.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }

        // Declared before the content, so Play (a real control further
        // down) still gets first refusal on a press inside its own bounds.
        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.lg
            spacing: Theme.space.md

            Item {
                Layout.preferredWidth: root.squareSize
                Layout.preferredHeight: root.squareSize

                PlaceholderSurface {
                    anchors.fill: parent
                    radius: Theme.radius.md
                }
                Image {
                    anchors.centerIn: parent
                    visible: !root.showsPackArt
                    width: root.iconSize
                    height: root.iconSize
                    source: "image://instanceicon/" + (root.iconKey.length > 0 ? root.iconKey : "default")
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
                Image {
                    id: packLogo
                    anchors.fill: parent
                    visible: root.showsPackArt
                    source: root.packArt
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }
                CornerMatte {
                    visible: root.showsPackArt
                    cornerRadius: Theme.radius.md
                    matte: card.color
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs

                Text {
                    Layout.fillWidth: true
                    text: root.name
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }

                // "Fabric 1.21.4 • 3 days ago", or the crash in place of
                // when.
                Row {
                    Layout.fillWidth: true
                    spacing: Theme.space.xs

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, parent.width)
                        text: root.hasCrashed ? root.versionText
                                              : [root.versionText, Format.lastPlayed(root.lastLaunch)]
                                                .filter((part) => part.length > 0).join("  •  ")
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hasCrashed
                        iconName: "alert-triangle"
                        size: Theme.icon.sm - 2
                        color: Theme.palette.warning
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.hasCrashed
                        text: qsTr("Crashed last time")
                        color: Theme.palette.warning
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }
            }

            // How long it has been played: a quiet fact before Play.
            Row {
                visible: root.playTimeText.length > 0 && root.width >= root.playTimeMinWidth
                spacing: Theme.space.xs

                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "hourglass"
                    size: Theme.icon.sm
                    color: Theme.palette.textTertiary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.playTimeText
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.features: ({ "tnum": 1 })
                }
            }

            PlayButton {
                Layout.leftMargin: Theme.space.sm
                round: false
                size: Theme.control.height
                running: root.isRunning
                enabled: root.isRunning || root.canLaunch
                focusPolicy: Qt.NoFocus
                onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
            }
        }
    }

    // The hairline, over everything so the logo's corner matte never
    // paints across it.
    Rectangle {
        anchors.fill: card
        radius: card.radius
        color: "transparent"
        border.width: 1
        border.color: root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the
        // row's own border -- the same idiom InstanceCard uses.
        anchors.fill: card
        anchors.margins: -Theme.space.xs
        radius: card.radius + Theme.space.xs
        color: "transparent"
        border.width: Theme.focus.width
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
