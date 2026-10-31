// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One "Continue playing" row on Home: icon square, name with a "New" chip until first played, what it runs and
 * when it was last played, play time, Play and ⋯. Used through a wrapper against recentModel, with the roles
 * InstanceCard reads. After a crash the sub-line says "Crashed last time" in place of when.
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
    // Newest screenshot url, or "". Not shown in the row; kept so the row takes the same roles as the library card.
    required property string coverImage
    property string packArt: ""
    required property bool hasCrashed

    signal clicked()
    signal playRequested()
    signal stopRequested()
    // @p anchor: the ⋯ button, or null for a right-click (the menu then opens at the pointer).
    signal menuRequested(Item anchor)

    readonly property bool hovered: hoverHandler.hovered
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready
    readonly property bool isNew: !(Number(root.lastLaunch) > 0)
    readonly property int rowHeight: Theme.control.heightLg + Theme.space.lg
    readonly property int squareSize: Theme.control.height
    readonly property real iconShare: 0.84
    readonly property int iconSize: Math.round(root.squareSize * root.iconShare)
    readonly property bool iconFills: Icons.fillsTile(root.iconKey)
    // Empty (so not shown) when "Show play time per instance" is off.
    readonly property string playTimeText: SettingsStore.bool("ShowGameTime") && !root.isNew
                                           ? Format.playTime(root.totalTimePlayed) : ""
    readonly property string versionText: Format.versionLine(root.loader, root.gameVersion)
    readonly property int playTimeMinWidth: 520

    implicitWidth: 640
    implicitHeight: root.rowHeight

    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.isRunning ? root.stopRequested() : root.playRequested()
    Keys.onMenuPressed: root.menuRequested(null)

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

        // Declared before the content so Play still gets first refusal on a press inside its bounds.
        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.menuRequested(null) }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.md

            InstanceTile {
                Layout.preferredWidth: root.squareSize
                Layout.preferredHeight: root.squareSize
                radius: Theme.radius.md
                matte: root.showsPackArt || root.iconFills ? card.color : "transparent"
                lifted: root.hovered

                Image {
                    anchors.centerIn: parent
                    visible: !root.showsPackArt
                    width: root.iconFills ? parent.width : root.iconSize
                    height: width
                    source: Icons.instanceIcon(root.iconKey)
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
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs

                // Placed by hand: a layout sizing the name from its own width loops.
                Item {
                    Layout.fillWidth: true
                    implicitHeight: Theme.type.title.lineHeightPx

                    Text {
                        id: nameText
                        width: Math.min(implicitWidth, parent.width - (newChip.visible ? newChip.width + Theme.space.sm : 0))
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.name
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.heading
                        font.pixelSize: Theme.type.title.pixelSize
                        font.weight: Theme.type.title.weight
                    }
                    Rectangle {
                        id: newChip
                        visible: root.isNew
                        x: nameText.width + Theme.space.sm
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.ceil(newLabel.implicitWidth) + Theme.space.sm * 2
                        height: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.space.xxs * 2
                        radius: Theme.radius.indicator
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.palette.accent

                        Text {
                            id: newLabel
                            anchors.centerIn: parent
                            text: qsTr("New")
                            color: Theme.palette.accentText
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                            font.weight: Font.DemiBold
                        }
                    }
                }

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

            QuietPlayButton {
                Layout.leftMargin: Theme.space.sm
                running: root.isRunning
                enabled: root.isRunning || root.canLaunch
                focusPolicy: Qt.NoFocus
                onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
            }

            IconButton {
                id: moreButton
                Layout.leftMargin: -Theme.space.xs
                size: Theme.control.heightSm
                iconName: "more"
                tip: qsTr("More")
                focusPolicy: Qt.NoFocus
                onClicked: root.menuRequested(moreButton)
            }
        }
    }

    // Over everything so the logo's corner matte never paints across it.
    Rectangle {
        anchors.fill: card
        radius: card.radius
        color: "transparent"
        border.width: 1
        border.color: root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Rectangle {
        anchors.fill: card
        anchors.margins: -Theme.space.xs
        radius: card.radius + Theme.space.xs
        color: "transparent"
        border.width: Theme.focus.width
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
