// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * What Join asks when it is not obvious which instance should enter a
 * friend's server: the friend is on Minecraft `version`, so an instance on
 * exactly that version is the one that follows.
 *
 * Several such instances: pick one and press Join (`picked`; a double click
 * or Enter on the row does too), the first one ready to start already
 * selected (FriendJoinFlow puts the one selected in the play bar first).
 * None: say so and offer to create one on that version (`createRequested`,
 * which Main.qml opens in the New instance dialog) -- the way on, with
 * nothing selected, so Enter never starts a game on the wrong version --
 * and, since many servers take a range of versions, the instances on other
 * versions to try instead. A row only selects, as in the picker, and "Join
 * anyway" starts it: a click that looks like choosing never starts a game
 * on a version that may not connect. One ready instance never gets here;
 * FriendJoinFlow joins with it straight away.
 */
Dialog {
    id: root

    // The friend, the instance they play, their Minecraft version and the
    // server they are on.
    property string friendName
    property string instanceName
    property string version
    property string host
    // Where Join sends the game: the host, and the port unless it is the
    // game's own (Format.friendAddress). Main.qml joins with it once an
    // instance is picked.
    property string address
    // The instances on exactly `version` and, for none of those, the others
    // (QmlShell.instancesForVersion / otherInstances): [{ id, name,
    // iconKey, version, loader, lastLaunch, running, canLaunch }].
    property var candidates: []
    property var others: []

    readonly property bool none: root.candidates.length === 0
    readonly property var rows: root.none ? root.others : root.candidates
    readonly property int rowHeight: Theme.control.height + Theme.space.sm * 2
    // The list scrolls past this many rows; the fallback has fewer, so it
    // weighs less than the way on above it.
    readonly property int maxRows: root.none ? 3 : 5
    readonly property int tileSize: Theme.control.height
    readonly property int iconSize: Theme.icon.lg

    signal picked(string id, string name)
    signal createRequested(string version)

    // The row to start on: the first one that can start.
    function firstReady() {
        for (var i = 0; i < root.rows.length; ++i) {
            if (root.rows[i].canLaunch)
                return i
        }
        return -1
    }
    // Whether Join can start with the row now selected.
    readonly property bool canJoin: list.currentIndex >= 0 && list.currentIndex < root.rows.length
                                    && root.rows[list.currentIndex].canLaunch

    parent: Overlay.overlay
    anchors.centerIn: parent
    // The fallback has three buttons to fit, and a longer title.
    readonly property int preferredWidth: root.none ? 520 : 440
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    modal: true
    // The title row sets the inset; the gap under it is the dialog's own spacing.
    topPadding: 0
    bottomPadding: Theme.space.sm
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // With none on the friend's version nothing is selected: the instances
    // listed are a fallback, not the answer.
    onAboutToShow: list.currentIndex = root.none ? -1 : root.firstReady()
    onOpened: root.none ? createButton.forceActiveFocus() : list.forceActiveFocus()

    header: DialogHeader {
        title: root.none ? qsTr("No Minecraft %1 instance").arg(root.version)
                         : qsTr("Join with which instance?")
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            // The instance name is whatever the friend called it, up to 64
            // characters of anything: not markup (a tag in it would fetch a
            // picture from any address, and restyle the question).
            textFormat: Text.PlainText
            text: root.none ? qsTr("%1 is on %2 with Minecraft %3. Create a %3 instance, then press Join again once it's ready.")
                              .arg(root.friendName).arg(root.host).arg(root.version)
                : root.instanceName.length > 0 ? qsTr("%1 is on %2, playing their instance \"%4\" (Minecraft %3).")
                                                   .arg(root.friendName).arg(root.host).arg(root.version).arg(root.instanceName)
                : qsTr("%1 is on %2 with Minecraft %3.")
                  .arg(root.friendName).arg(root.host).arg(root.version)
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            lineHeight: Theme.type.label.lineHeight
        }

        Text {
            visible: root.none && root.others.length > 0
            Layout.fillWidth: true
            text: qsTr("Or join with another version (may not connect)")
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.DemiBold
        }

        Rectangle {
            visible: root.rows.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(root.rows.length, root.maxRows) * root.rowHeight + Theme.space.xs * 2
            radius: Theme.radius.lg
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: Theme.space.xs
                model: root.rows
                clip: true
                // Only a list that overflows scrolls, so a plain click on a row
                // is never held back as the start of a flick.
                interactive: contentHeight > height
                boundsBehavior: Flickable.StopAtBounds
                // Up and Down step over the rows that cannot start.
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
                    if (row && row.canLaunch) {
                        root.close()
                        root.picked(row.id, row.name)
                    }
                }

                delegate: AbstractButton {
                    id: choice
                    required property var modelData
                    required property int index
                    // Selected: the row Join starts.
                    readonly property bool current: ListView.isCurrentItem
                    // The line under the name: what it runs and when it last did.
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
                    // Picking starts the game, so a click only selects and
                    // Join (or a double click, or Enter) starts it.
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
                        // A row that cannot start is still shown, so the list is
                        // the user's instances on this version; it is only dimmed.
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

                            Text {
                                width: parent.width
                                text: choice.modelData.name
                                elide: Text.ElideRight
                                // A fallback is quieter than a match.
                                color: choice.current ? Theme.palette.accentText
                                     : root.none ? Theme.palette.textSecondary : Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.body.pixelSize
                                font.weight: Theme.type.bodyStrong.weight
                            }
                            Text {
                                width: parent.width
                                text: choice.detail
                                elide: Text.ElideRight
                                color: Theme.palette.textTertiary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
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
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.xl
        topPadding: Theme.space.sm

        Button {
            id: createButton
            visible: root.none
            highlighted: true
            text: qsTr("Create %1 instance").arg(root.version)
            icon.source: Icons.url("plus")
            onClicked: {
                root.close()
                root.createRequested(root.version)
            }
        }
        Button {
            visible: !root.none
            highlighted: true
            enabled: root.canJoin
            text: qsTr("Join")
            icon.source: Icons.url("play")
            onClicked: list.pickCurrent()
        }
        // The fallback's way on, quieter than creating the right instance:
        // it waits for a row to be chosen.
        Button {
            visible: root.none && root.rows.length > 0
            enabled: root.canJoin
            text: qsTr("Join anyway")
            icon.source: Icons.url("play")
            onClicked: list.pickCurrent()
        }
        Button {
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }
}
