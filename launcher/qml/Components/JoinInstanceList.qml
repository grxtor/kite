// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The instances a Join dialog offers (FriendJoinDialog, ServerJoinDialog), on a sunken box: icon, name, loader,
 * version and when last played, with "Running" or "Can't start" on one that cannot. A click only selects; a double
 * click, Enter or the dialog's Join (pickCurrent()) picks (`picked`). Arrow keys step over the ones that cannot start.
 */
BoxSurface {
    id: root

    // [{ id, name, iconKey, version, loader, lastLaunch, running, canLaunch }] (QmlShell::instancesForVersion()).
    property var rows: []
    property int maxRows: 5
    // Names in the secondary tone: the rows are a fallback, not the answer.
    property bool fallback: false
    property alias currentIndex: list.currentIndex

    readonly property int rowHeight: Theme.control.height + Theme.space.sm * 2
    readonly property int tileSize: Theme.control.height
    readonly property int iconSize: Theme.icon.lg
    readonly property bool canJoin: list.currentIndex >= 0 && list.currentIndex < root.rows.length
                                    && root.rows[list.currentIndex].canLaunch

    signal picked(string id, string name)

    function firstReady() {
        for (var i = 0; i < root.rows.length; ++i) {
            if (root.rows[i].canLaunch)
                return i
        }
        return -1
    }
    function pickCurrent() {
        list.pickCurrent()
    }
    function focusList() {
        list.forceActiveFocus()
    }

    visible: root.rows.length > 0
    implicitHeight: Math.min(root.rows.length, root.maxRows) * root.rowHeight + Theme.space.xs * 2
    sunken: true

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: Theme.space.xs
        model: root.rows
        clip: true
        // Only a list that overflows scrolls, so a plain click is never held back as the start of a flick.
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        highlightFollowsCurrentItem: true
        ScrollBar.vertical: ScrollBar {}

        Keys.onUpPressed: list.step(-1)
        Keys.onDownPressed: list.step(1)
        Keys.onReturnPressed: list.pickCurrent()
        Keys.onEnterPressed: list.pickCurrent()

        function step(direction) {
            for (var i = list.currentIndex + direction; i >= 0 && i < root.rows.length; i += direction) {
                if (root.rows[i].canLaunch) {
                    list.currentIndex = i
                    list.positionViewAtIndex(i, ListView.Contain)
                    return
                }
            }
        }
        function pickCurrent() {
            var row = root.rows[list.currentIndex]
            if (row && row.canLaunch)
                root.picked(row.id, row.name)
        }

        delegate: AbstractButton {
            id: choice
            required property var modelData
            required property int index
            readonly property bool current: ListView.isCurrentItem
            readonly property string detail: (modelData.loader.length > 0 ? modelData.loader : qsTr("Vanilla"))
                                             + " " + modelData.version + " · " + Format.lastPlayed(modelData.lastLaunch)

            width: list.width
            height: root.rowHeight
            hoverEnabled: true
            enabled: modelData.canLaunch
            leftPadding: Theme.space.sm
            rightPadding: Theme.space.md
            Accessible.name: modelData.running ? qsTr("%1, %2, running").arg(modelData.name).arg(detail)
                                               : qsTr("%1, %2").arg(modelData.name).arg(detail)
            // A click only selects; Join (or a double click, or Enter) starts it.
            onClicked: {
                list.currentIndex = choice.index
                list.forceActiveFocus()
            }
            onDoubleClicked: list.pickCurrent()

            background: Rectangle {
                radius: Theme.radius.md
                color: choice.down ? Theme.palette.pressedOverlay
                     : choice.hovered ? Theme.palette.surfaceHover
                     : choice.current ? Theme.palette.accentSubtle : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
            contentItem: RowLayout {
                spacing: Theme.space.md
                opacity: choice.enabled ? 1 : Theme.opacity.disabled

                InstanceTile {
                    Layout.preferredWidth: root.tileSize
                    Layout.preferredHeight: root.tileSize
                    radius: Theme.radius.sm

                    Image {
                        anchors.centerIn: parent
                        width: root.iconSize
                        height: root.iconSize
                        sourceSize: Qt.size(width * 2, height * 2)
                        source: Icons.instanceIcon(choice.modelData.iconKey)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                }
                Column {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: Theme.space.xxs

                    Copy {
                        width: parent.width
                        text: choice.modelData.name
                        elide: Text.ElideRight
                        role: "bodyStrong"
                        color: choice.current ? Theme.palette.accentText
                             : root.fallback ? Theme.palette.textSecondary : Theme.palette.textPrimary
                    }
                    Copy {
                        width: parent.width
                        text: choice.detail
                        elide: Text.ElideRight
                        role: "label"
                        tone: "tertiary"
                    }
                }
                StatusBadge {
                    visible: !choice.modelData.canLaunch
                    tone: choice.modelData.running ? "warning" : "danger"
                    text: choice.modelData.running ? qsTr("Running") : qsTr("Can't start")
                }
            }
        }
    }
}
