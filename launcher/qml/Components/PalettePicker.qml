// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The colour schemes side by side, each drawn as a miniature launcher in its
 * own colours (sidebar, two cards, a Play button) for the current light or
 * dark mode, so the choice is made by looking rather than by name.
 */
Row {
    id: root

    // The scheme shown as chosen; "amethyst", "ember" or "diamond".
    property string current: Theme.scheme
    signal picked(string scheme)

    readonly property var schemes: [
        { id: "amethyst", label: qsTr("Obsidian"), note: qsTr("Graphite and amethyst") },
        { id: "ember", label: qsTr("Ember"), note: qsTr("Charcoal and lava") },
        { id: "diamond", label: qsTr("Diamond"), note: qsTr("Navy and diamond blue") }
    ]

    spacing: Theme.space.md

    Repeater {
        model: root.schemes

        delegate: AbstractButton {
            id: card
            required property var modelData
            // Re-read whenever the mode flips, so the previews follow it.
            readonly property var colors: Theme.dark, Theme.previewPalette(modelData.id)
            readonly property bool chosen: root.current === modelData.id

            width: 188
            height: 150
            hoverEnabled: true
            Accessible.name: modelData.label
            Accessible.role: Accessible.RadioButton
            Accessible.checked: chosen
            onClicked: root.picked(modelData.id)

            background: Rectangle {
                radius: Theme.radius.lg
                color: Theme.palette.surfaceRaised
                border.width: card.chosen ? 2 : 1
                border.color: card.chosen ? Theme.palette.accent
                              : card.hovered ? Theme.palette.borderStrong : Theme.palette.border
                Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }
            }

            contentItem: Column {
                spacing: Theme.space.sm

                // The miniature launcher.
                Rectangle {
                    width: parent.width
                    height: 86
                    radius: Theme.radius.md
                    color: card.colors.canvas
                    border.width: 1
                    border.color: card.colors.border
                    clip: true

                    Rectangle {
                        id: miniSidebar
                        x: 0
                        width: 34
                        height: parent.height
                        color: card.colors.surface
                        Rectangle {
                            x: 6; y: 10
                            width: 22; height: 6
                            radius: 2
                            color: card.colors.accentSubtle
                            Rectangle { width: 2; height: parent.height; color: card.colors.accent }
                        }
                        Repeater {
                            model: 2
                            delegate: Rectangle {
                                required property int index
                                x: 6; y: 22 + index * 10
                                width: 18; height: 4
                                radius: 2
                                color: card.colors.textTertiary
                                opacity: 0.6
                            }
                        }
                    }
                    Row {
                        x: miniSidebar.width + 8
                        y: 10
                        spacing: 6
                        Repeater {
                            model: 3
                            delegate: Rectangle {
                                width: 36; height: 30
                                radius: 4
                                color: card.colors.surfaceRaised
                                border.width: 1
                                border.color: card.colors.border
                            }
                        }
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.horizontalCenterOffset: miniSidebar.width / 2
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        width: 64; height: 18
                        radius: 4
                        color: card.colors.accent
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("PLAY")
                            color: card.colors.textOnAccent
                            font.family: Theme.font.family
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            font.letterSpacing: 1
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.space.xs
                    Column {
                        width: parent.width - check.width - parent.spacing
                        Text {
                            width: parent.width
                            text: card.modelData.label
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.bodyStrong.pixelSize
                            font.weight: Theme.type.bodyStrong.weight
                        }
                        Text {
                            width: parent.width
                            text: card.modelData.note
                            elide: Text.ElideRight
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                    Rectangle {
                        id: check
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.icon.md
                        height: width
                        radius: width / 2
                        color: card.chosen ? Theme.palette.accent : "transparent"
                        border.width: card.chosen ? 0 : 1
                        border.color: Theme.palette.borderStrong
                        MeshIcon {
                            anchors.centerIn: parent
                            visible: card.chosen
                            iconName: "check"
                            size: Theme.icon.sm - 2
                            color: Theme.palette.textOnAccent
                        }
                    }
                }
            }
            padding: Theme.space.sm
        }
    }
}
