// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance at a glance: a stat strip, a peek at its recent screenshots
 * and mods, and the user's own notes -- each a short summary that hands off
 * to the tab that has the full picture, rather than trying to be all of
 * them at once.
 *
 * Not a SettingsScroll: that caps its column at 760px for reading comfort,
 * which is right for a form but starves this dashboard's two-column layout
 * and screenshot strip of the width they need.
 */
Flickable {
    id: root

    property string loader
    property string gameVersion
    property var lastLaunch: 0
    property var totalTimePlayed: 0
    // InstanceDetails, for the screenshots/mods previews below.
    property var details: null
    property string notes
    signal notesEdited(string text)
    signal screenshotsRequested()
    signal manageContentRequested()

    readonly property var shotsModel: root.details ? root.details.screenshots : null
    readonly property var modsModel: root.details ? root.details.mods : null

    contentWidth: width
    contentHeight: content.height + Theme.space.xl
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    Accessible.name: qsTr("Overview")

    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: content
        x: Theme.space.xs
        y: Theme.space.xs
        width: root.width - Theme.space.xs * 2
        spacing: Theme.space.lg

        // Four facts worth knowing at a glance, wrapping to two columns (or
        // one) rather than shrinking once the page gets narrower than a
        // comfortable row of four.
        Flow {
            Layout.fillWidth: true
            spacing: Theme.space.md
            readonly property int perRow: width >= 640 ? 4 : width >= 340 ? 2 : 1
            readonly property real tileWidth: Math.floor((width - spacing * (perRow - 1)) / perRow)

            StatTile {
                width: parent.tileWidth
                iconName: "cube"
                label: qsTr("Minecraft")
                value: root.gameVersion
            }
            StatTile {
                width: parent.tileWidth
                iconName: "layers"
                label: qsTr("Mod loader")
                value: root.loader.length > 0 ? root.loader : qsTr("Vanilla")
            }
            StatTile {
                width: parent.tileWidth
                iconName: "play"
                label: qsTr("Time played")
                value: Format.playTime(root.totalTimePlayed) || qsTr("Not yet")
            }
            StatTile {
                width: parent.tileWidth
                iconName: "clock"
                label: qsTr("Last played")
                value: Format.lastPlayed(root.lastLaunch)
            }
        }

        // Screenshots and mods need real width to read well, and notes
        // should not have to fight them for it -- two columns above ~1000px
        // of content width, one below.
        GridLayout {
            id: board
            Layout.fillWidth: true
            columns: width >= 1000 ? 2 : 1
            columnSpacing: Theme.space.lg
            rowSpacing: Theme.space.lg

            // Recent screenshots
            Rectangle {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border
                implicitHeight: shotsColumn.implicitHeight + Theme.space.lg * 2

                ColumnLayout {
                    id: shotsColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space.lg
                    spacing: Theme.space.md

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.sm
                        Text {
                            text: qsTr("Recent screenshots")
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.title.pixelSize
                            font.weight: Theme.type.title.weight
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            flat: true
                            visible: !!root.shotsModel && root.shotsModel.count > 0
                            text: qsTr("View all")
                            onClicked: root.screenshotsRequested()
                        }
                    }

                    Row {
                        id: shotsRow
                        Layout.fillWidth: true
                        Layout.preferredHeight: tileHeight
                        spacing: Theme.space.sm
                        visible: !!root.shotsModel && root.shotsModel.count > 0

                        readonly property int tileWidth: Math.min(160, (width - spacing * 3) / 4)
                        readonly property int tileHeight: Math.round(tileWidth * 9 / 16)

                        Repeater {
                            model: root.shotsModel
                            delegate: Rectangle {
                                id: shot
                                required property int index
                                required property string path

                                visible: index < 4
                                width: visible ? parent.tileWidth : 0
                                height: parent.tileHeight
                                radius: Theme.radius.md
                                color: Theme.palette.surfaceSunken
                                border.width: 1
                                border.color: shotHover.hovered ? Theme.palette.borderStrong : Theme.palette.border
                                clip: true

                                HoverHandler { id: shotHover }

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    source: "image://screenshot/" + encodeURIComponent(shot.path)
                                    sourceSize: Qt.size(256, 256)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    scale: shotHover.hovered ? 1.05 : 1.0
                                    Behavior on scale { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                                }

                                TapHandler { onTapped: root.screenshotsRequested() }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !root.shotsModel || root.shotsModel.count === 0
                        text: qsTr("Press F2 in game; screenshots show up here.")
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }
            }

            // Sidebar: a mods peek, and notes underneath it.
            ColumnLayout {
                Layout.fillWidth: board.columns === 1
                Layout.preferredWidth: board.columns === 2 ? 320 : -1
                Layout.alignment: Qt.AlignTop
                spacing: Theme.space.lg

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radius.lg
                    color: Theme.palette.surface
                    border.width: 1
                    border.color: Theme.palette.border
                    implicitHeight: modsColumn.implicitHeight + Theme.space.lg * 2

                    ColumnLayout {
                        id: modsColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.space.lg
                        spacing: Theme.space.sm

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.sm
                            Text {
                                text: qsTr("Mods")
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.title.pixelSize
                                font.weight: Theme.type.title.weight
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                flat: true
                                visible: modsRepeater.count > 0
                                text: qsTr("Manage")
                                onClicked: root.manageContentRequested()
                            }
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: Theme.space.sm
                            visible: modsRepeater.count > 0

                            Repeater {
                                id: modsRepeater
                                model: root.modsModel
                                delegate: Row {
                                    id: modRow
                                    required property int index
                                    required property var model
                                    visible: index < 5
                                    width: parent.width
                                    spacing: Theme.space.sm

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: modRow.model.enabled ? Theme.palette.success : Theme.palette.textDisabled
                                    }
                                    Text {
                                        width: parent.width - 6 - parent.spacing
                                        text: modRow.model.name
                                        elide: Text.ElideRight
                                        color: modRow.model.enabled ? Theme.palette.textPrimary : Theme.palette.textTertiary
                                        font.family: Theme.font.family
                                        font.pixelSize: Theme.type.label.pixelSize
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: modsRepeater.count === 0
                            text: root.details && root.details.isMinecraft ? qsTr("No mods installed yet.")
                                                                            : qsTr("This instance cannot have mods.")
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.label.pixelSize
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radius.lg
                    color: Theme.palette.surface
                    border.width: 1
                    border.color: Theme.palette.border
                    implicitHeight: notesColumn.implicitHeight + Theme.space.lg * 2

                    ColumnLayout {
                        id: notesColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.space.lg
                        spacing: Theme.space.sm

                        Text {
                            text: qsTr("Notes")
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.title.pixelSize
                            font.weight: Theme.type.title.weight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            radius: Theme.radius.md
                            color: Theme.palette.surfaceSunken
                            border.width: 1
                            border.color: Theme.palette.border

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: Theme.space.xs

                                TextArea {
                                    id: notesArea
                                    placeholderText: qsTr("Seed, server address, which mods to update…")
                                    wrapMode: TextEdit.Wrap
                                    selectByMouse: true
                                    background: null
                                    color: Theme.palette.textPrimary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.body.pixelSize
                                    Component.onCompleted: text = root.notes
                                    onActiveFocusChanged: if (!activeFocus && text !== root.notes) root.notesEdited(text)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
