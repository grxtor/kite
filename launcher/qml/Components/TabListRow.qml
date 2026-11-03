// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One row of an instance tab's list (a world, a server, a data pack, a backup,
 * a component): a card with a square (RowThumb), a name over one line of meta
 * and the row's controls on the right. A right-click asks for the row's menu.
 * Items declared inside go to the controls; `badges` go after the meta line;
 * `thumb` is the RowThumb itself.
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
StateFill {
    id: root

    property string title
    property string meta
    // A meta of markup the caller made safe (a server's message: Text.StyledText), and how many lines it may take.
    property int metaFormat: Text.AutoText
    property int metaLines: 1
    property bool dimmed: false
    readonly property alias thumb: thumb
    default property alias actions: actionRow.data
    property alias badges: badgeRow.data
    signal menuRequested()

    implicitHeight: thumb.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.lg
    card: true
    hovered: hover.hovered
    border.width: 1
    border.color: root.hovered ? Theme.neutral.borderHover : Theme.palette.border
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
            opacity: root.dimmed ? Theme.opacity.disabled + 0.2 : 1

            Copy {
                width: parent.width
                text: root.title
                elide: Text.ElideRight
                role: "title"
                font.family: Theme.font.family
            }
            Row {
                id: metaRow
                width: parent.width
                visible: root.meta.length > 0 || badgeRow.visibleChildren.length > 0
                spacing: Theme.space.sm

                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text.length > 0
                    width: Math.min(implicitWidth, metaRow.width - (badgeRow.width > 0 ? badgeRow.width + metaRow.spacing : 0))
                    text: root.meta
                    textFormat: root.metaFormat
                    maximumLineCount: root.metaLines
                    elide: Text.ElideRight
                    role: "label"
                    tone: "secondary"
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
