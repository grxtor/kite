// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One choice in a picker of looks or palettes (LookPicker, PalettePicker):
 * a preview drawn by the host -- the tile's children fill its top -- then
 * the name with a round mark when it is the chosen one, and a short note
 * under them. A radio button to accessibility.
 *
 * The preview is a miniature in a rounded frame, but `clip` only clips to
 * its bounding box, so whatever the host draws edge to edge (a sidebar, a
 * sky) would keep square corners. A CornerMatte in the tile's own colour
 * covers them, clipped to the preview so its ring never reaches the tile's
 * border, and `previewOutline` redraws the frame's hairline there.
 */
AbstractButton {
    id: root

    property string label: ""
    property string note: ""
    property bool chosen: false
    property int previewHeight: 86
    property int previewRadius: Theme.radius.md
    // The hairline the host draws round its preview.
    property color previewOutline: "transparent"
    // The preview the host draws; it fills the slot above the name.
    default property alias preview: previewHost.data

    hoverEnabled: true
    padding: Theme.space.sm
    Accessible.name: root.label
    Accessible.role: Accessible.RadioButton
    Accessible.checked: root.chosen

    background: Rectangle {
        id: fill
        radius: Theme.radius.lg
        color: Theme.palette.surfaceRaised
        border.width: root.chosen ? 2 : 1
        border.color: root.chosen ? Theme.palette.accent
                      : root.hovered ? Theme.palette.borderStrong : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }
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
                    matte: fill.color
                    cornerRadius: root.previewRadius
                    outline: root.previewOutline
                }
            }
        }

        // Name and the chosen-mark on one line, the note under them at the
        // tile's full width.
        Column {
            width: parent.width
            Row {
                width: parent.width
                spacing: Theme.space.xs
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - check.width - parent.spacing
                    text: root.label
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.bodyStrong.pixelSize
                    font.weight: Theme.type.bodyStrong.weight
                }
                Rectangle {
                    id: check
                    width: Theme.icon.md
                    height: width
                    radius: width / 2
                    color: root.chosen ? Theme.palette.accent : "transparent"
                    border.width: root.chosen ? 0 : 1
                    border.color: Theme.palette.borderStrong
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: root.chosen
                        iconName: "check"
                        size: Theme.icon.sm - 2
                        color: Theme.palette.textOnAccent
                    }
                }
            }
            Text {
                width: parent.width
                text: root.note
                elide: Text.ElideRight
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }
}
