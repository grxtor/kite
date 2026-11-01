// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One choice in a picker of looks or palettes, on the card it sits in: a preview drawn by the host (the tile's
 * children fill its top), the name and a short note. No fill or frame at rest, the hover overlay under the pointer,
 * a bare check beside the name when chosen (the accent ring is the keyboard focus). `clip` only clips to the
 * bounding box, so a CornerMatte in the colour behind the tile rounds whatever the host draws edge to edge.
 */
AbstractButton {
    id: root

    property string label: ""
    property string note: ""
    property bool chosen: false
    property int previewHeight: 86
    property int previewRadius: Theme.radius.md
    default property alias preview: previewHost.data

    hoverEnabled: true
    padding: Theme.space.sm
    Accessible.name: root.label
    Accessible.role: Accessible.RadioButton
    Accessible.checked: root.chosen

    background: StateFill {
        id: fill
        hovered: root.hovered
        radius: Theme.radius.lg

        FocusFrame { around: Theme.space.xxs; visible: root.visualFocus }
    }

    contentItem: Column {
        spacing: Theme.space.sm

        Item {
            width: parent.width
            height: root.previewHeight

            Item {
                id: previewHost
                anchors.fill: parent
            }
            Item {
                anchors.fill: parent
                clip: true

                CornerMatte {
                    matte: Qt.tint(Theme.palette.surface, fill.color)
                    cornerRadius: root.previewRadius
                }
            }
        }

        Column {
            width: parent.width
            Row {
                width: parent.width
                spacing: Theme.space.xs
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - (check.visible ? check.width + parent.spacing : 0)
                    text: root.label
                    elide: Text.ElideRight
                    role: "bodyStrong"
                }
                MeshIcon {
                    id: check
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.chosen
                    iconName: "check"
                    size: Theme.icon.sm
                    color: Theme.palette.accentText
                }
            }
            Copy {
                width: parent.width
                text: root.note
                elide: Text.ElideRight
                role: "caption"
                tone: "tertiary"
            }
        }
    }
}
