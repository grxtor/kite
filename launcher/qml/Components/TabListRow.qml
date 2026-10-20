// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One row of an instance tab's list -- a world, a server, a data pack, a
 * backup, a component -- so those tabs read as one set, at the scale of
 * Home's rows and Content's table: a card with the thing's square
 * (RowThumb), its name over one line of what it is ("Survival  •  Day 12
 * •  Yesterday"), and its own controls on the right (a button, a switch,
 * ⋯). A right-click asks for the row's menu.
 *
 * Items declared inside go to the controls on the right; `badges` go after
 * the meta line. `thumb` is the RowThumb itself.
 *
 *     TabListRow {
 *         title: name
 *         meta: address
 *         thumb.source: iconUrl
 *         thumb.glyph: "server"
 *         onMenuRequested: rowMenu.popup()
 *         Button { text: qsTr("Join") ... }
 *         IconButton { iconName: "more" ... }
 *     }
 */
Rectangle {
    id: root

    property string title
    property string meta
    // Switched off: what it is fades, its controls stay as they are.
    property bool dimmed: false
    readonly property alias thumb: thumb
    default property alias actions: actionRow.data
    property alias badges: badgeRow.data
    readonly property bool hovered: hover.hovered
    signal menuRequested()

    implicitHeight: thumb.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.lg
    color: root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
    border.width: 1
    border.color: root.hovered ? Theme.neutral.borderHover : Theme.palette.border
    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

    HoverHandler { id: hover }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root.menuRequested()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.md
        spacing: Theme.space.md

        RowThumb {
            id: thumb
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
            matte: root.color
            opacity: root.dimmed ? Theme.opacity.disabled : 1
        }

        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xxs
            // As Content's switched-off rows: faded, still readable.
            opacity: root.dimmed ? Theme.opacity.disabled + 0.2 : 1

            Text {
                width: parent.width
                text: root.title
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Theme.type.title.weight
            }
            Row {
                id: metaRow
                width: parent.width
                visible: root.meta.length > 0 || badgeRow.visibleChildren.length > 0
                spacing: Theme.space.sm

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text.length > 0
                    width: Math.min(implicitWidth, metaRow.width - (badgeRow.width > 0 ? badgeRow.width + metaRow.spacing : 0))
                    text: root.meta
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Row {
                    id: badgeRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs
                }
            }
        }

        RowLayout {
            id: actionRow
            spacing: Theme.space.sm
        }
    }
}
