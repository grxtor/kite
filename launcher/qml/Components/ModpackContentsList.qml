// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * What one version of a modpack holds (ModpackDetail's "Contents" tab), as far as the provider lists it without the
 * pack file: each project's logo, name and summary in one card, split by hairlines. A row opens the project's page
 * in the browser. `items` is the detail's `contents`.
 */
Column {
    id: root

    property var items: []
    property bool loading: false
    // The version these belong to, as the Install card names it.
    property string versionLabel: ""

    readonly property int logoSize: 36
    readonly property int rowHeight: 60

    spacing: Theme.space.md

    Copy {
        width: parent.width
        visible: !root.loading && root.items.length > 0
        text: qsTr("%n projects in %1", "", root.items.length).arg(root.versionLabel)
        role: "label"
        tone: "tertiary"
        elide: Text.ElideRight
    }
    BusyIndicator {
        visible: root.loading
        running: visible
    }

    BoxSurface {
        width: parent.width
        visible: root.items.length > 0
        implicitHeight: rows.implicitHeight + Theme.space.xs * 2

        Column {
            id: rows
            y: Theme.space.xs
            width: parent.width

            Repeater {
                model: root.items
                delegate: AbstractButton {
                    id: row
                    required property var modelData
                    required property int index
                    width: rows.width
                    height: root.rowHeight
                    leftPadding: Theme.space.lg
                    rightPadding: Theme.space.lg
                    hoverEnabled: true
                    enabled: (modelData.url || "").length > 0
                    Accessible.name: modelData.title
                    onClicked: ExternalLinks.open(row, modelData.url)

                    background: Item {
                        StateFill {
                            id: strip
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space.xs
                            anchors.rightMargin: Theme.space.xs
                            anchors.topMargin: Theme.space.xxs
                            anchors.bottomMargin: Theme.space.xxs
                            radius: Theme.radius.sm
                            card: true
                            hovered: row.hovered
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: Theme.space.md
                            anchors.rightMargin: Theme.space.md
                            height: 1
                            color: Theme.palette.divider
                            visible: row.index < root.items.length - 1 && !row.hovered
                        }
                    }
                    contentItem: Row {
                        spacing: Theme.space.md
                        Item {
                            anchors.verticalCenter: parent.verticalCenter
                            width: root.logoSize
                            height: root.logoSize
                            Image {
                                id: logo
                                anchors.fill: parent
                                source: row.modelData.iconUrl || ""
                                sourceSize: Qt.size(width * 2, height * 2)
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                mipmap: true
                                asynchronous: true
                            }
                            MeshIcon {
                                anchors.centerIn: parent
                                visible: logo.status !== Image.Ready
                                iconName: "package"
                                size: Theme.icon.md
                                color: Theme.palette.textTertiary
                            }
                            CornerMatte {
                                cornerRadius: Theme.radius.sm
                                matte: strip.color
                            }
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - root.logoSize - parent.spacing
                            spacing: Theme.space.xxs
                            Copy {
                                width: parent.width
                                text: row.modelData.title
                                elide: Text.ElideRight
                                role: "bodyStrong"
                            }
                            Copy {
                                width: parent.width
                                visible: text.length > 0
                                // One line whatever the summary holds: some carry line breaks.
                                text: String(row.modelData.description || "").replace(/\s+/g, " ")
                                maximumLineCount: 1
                                elide: Text.ElideRight
                                role: "caption"
                                tone: "tertiary"
                            }
                        }
                    }
                }
            }
        }
    }
}
