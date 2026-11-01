// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * What Join asks when it is not obvious which instance should enter a friend's server. Several instances on the
 * exact version: pick one (`picked`). None: offer to create one (`createRequested`) with nothing selected, so
 * Enter never starts the wrong version, plus instances on other versions to try; a row only selects and "Join
 * anyway" starts.
 */
AppDialog {
    id: root

    property string friendName
    property string instanceName
    property string version
    property string host
    // Where Join sends the game (Format.friendAddress).
    property string address
    // Instances on exactly `version`, else the others: [{ id, name, iconKey, version, loader, lastLaunch, running,
    // canLaunch }].
    property var candidates: []
    property var others: []

    readonly property bool none: root.candidates.length === 0
    readonly property var rows: root.none ? root.others : root.candidates
    readonly property int rowHeight: Theme.control.height + Theme.space.sm * 2
    readonly property int maxRows: root.none ? 3 : 5
    readonly property int tileSize: Theme.control.height
    readonly property int iconSize: Theme.icon.lg

    signal picked(string id, string name)
    signal createRequested(string version)

    function firstReady() {
        for (var i = 0; i < root.rows.length; ++i) {
            if (root.rows[i].canLaunch)
                return i
        }
        return -1
    }
    readonly property bool canJoin: list.currentIndex >= 0 && list.currentIndex < root.rows.length
                                    && root.rows[list.currentIndex].canLaunch

    size: root.none ? "md" : "sm"
    closeOnOutsidePress: true
    title: root.none ? qsTr("No Minecraft %1 instance").arg(root.version)
                     : qsTr("Join with which instance?")

    // With none on the friend's version nothing is selected: the listed instances are a fallback.
    onAboutToShow: list.currentIndex = root.none ? -1 : root.firstReady()
    onOpened: root.none ? createButton.forceActiveFocus() : list.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            // The name is whatever the friend called it (up to 64 characters): not markup, a tag would fetch a
            // picture from any address.
            textFormat: Text.PlainText
            text: root.none ? qsTr("%1 is on %2 with Minecraft %3. Create a %3 instance, then press Join again once it's ready.")
                              .arg(root.friendName).arg(root.host).arg(root.version)
                : root.instanceName.length > 0 ? qsTr("%1 is on %2, playing their instance \"%4\" (Minecraft %3).")
                                                   .arg(root.friendName).arg(root.host).arg(root.version).arg(root.instanceName)
                : qsTr("%1 is on %2 with Minecraft %3.")
                  .arg(root.friendName).arg(root.host).arg(root.version)
            tone: "secondary"
            lineHeight: Theme.type.label.lineHeight
        }

        Copy {
            visible: root.none && root.others.length > 0
            Layout.fillWidth: true
            text: qsTr("Or join with another version (may not connect)")
            role: "label"
            tone: "tertiary"
            font.weight: Font.DemiBold
        }

        BoxSurface {
            visible: root.rows.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(root.rows.length, root.maxRows) * root.rowHeight + Theme.space.xs * 2
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
                    if (row && row.canLaunch) {
                        root.close()
                        root.picked(row.id, row.name)
                    }
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
                                     : root.none ? Theme.palette.textSecondary : Theme.palette.textPrimary
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
