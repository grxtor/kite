// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * What a drag over the page will do: the page goes dark under a dashed outline and a card in the middle says
 * "Drop to add", then one line per kind of file with its count ("2 mods", "1 data pack" and, since it needs a
 * choice, that it will ask for a world). `rows` is ContentDrop.preview()'s list of {kind, text}; `unknown` is its line
 * for files that are none of them. Both empty (the files cannot be read yet) leaves just the title. Takes no pointer
 * events, the drop area under it does.
 */
Item {
    id: root

    property var rows: []
    property string unknown: ""
    // The page's own corner, so the scrim ends where the page does.
    property int radius: Theme.radius.xl

    readonly property string title: qsTr("Drop to add")
    readonly property int cardWidth: 340
    readonly property int outlineInset: Theme.space.md
    readonly property int dashLength: 6
    readonly property var kindIcons: ({
        mods: "package", resourcepacks: "image", datapacks: "archive", shaderpacks: "layers",
        worlds: "globe", modpacks: "package"
    })

    Accessible.role: Accessible.StaticText
    Accessible.name: [root.title].concat(root.rows.map((row) => row.text)).concat([root.unknown])
                     .filter((part) => part.length > 0).join(", ")

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Theme.palette.scrim
    }

    // The outline is drawn dashed on a canvas: Rectangle borders cannot.
    Canvas {
        id: outline
        anchors.fill: parent
        anchors.margins: root.outlineInset
        readonly property color stroke: Theme.palette.textSecondary
        onStrokeChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const r = Math.min(Theme.radius.xl, width / 2, height / 2)
            ctx.strokeStyle = stroke
            ctx.lineWidth = 1
            ctx.setLineDash([root.dashLength, root.dashLength])
            ctx.beginPath()
            ctx.roundedRect(0.5, 0.5, width - 1, height - 1, r, r)
            ctx.stroke()
        }
    }

    GlassSurface {
        anchors.centerIn: parent
        width: Math.min(root.cardWidth, root.width - Theme.space.xxl * 2)
        height: content.implicitHeight + Theme.space.xl * 2
        radius: Theme.radius.xl
        level: Theme.elevation.lg

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: Theme.space.xl
            spacing: Theme.space.md

            MeshIcon {
                Layout.alignment: Qt.AlignHCenter
                iconName: "download"
                size: Theme.icon.xl
                color: Theme.palette.accentText
            }
            Copy {
                Layout.alignment: Qt.AlignHCenter
                text: root.title
                role: "subheading"
            }

            Repeater {
                model: root.rows
                delegate: RowLayout {
                    id: line
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.space.sm

                    MeshIcon {
                        iconName: root.kindIcons[line.modelData.kind] || "package"
                        size: Theme.icon.mid
                        color: Theme.palette.textSecondary
                    }
                    Copy {
                        text: line.modelData.text
                        Layout.fillWidth: line.modelData.kind !== "datapacks"
                    }
                    Copy {
                        visible: line.modelData.kind === "datapacks"
                        Layout.fillWidth: true
                        text: qsTr("→ you will pick a world")
                        tone: "secondary"
                        elide: Text.ElideRight
                    }
                }
            }

            Copy {
                visible: root.unknown.length > 0
                Layout.fillWidth: true
                text: root.unknown
                tone: "tertiary"
                wrapMode: Text.Wrap
            }
        }
    }
}
