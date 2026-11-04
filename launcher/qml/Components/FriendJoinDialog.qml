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
    readonly property int maxRows: root.none ? 3 : 5

    signal picked(string id, string name)
    signal createRequested(string version)

    size: root.none ? "md" : "sm"
    closeOnOutsidePress: true
    title: root.none ? qsTr("No Minecraft %1 instance").arg(root.version)
                     : qsTr("Join with which instance?")

    // With none on the friend's version nothing is selected: the listed instances are a fallback.
    onAboutToShow: list.currentIndex = root.none ? -1 : list.firstReady()
    onOpened: root.none ? createButton.forceActiveFocus() : list.focusList()

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

        JoinInstanceList {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            rows: root.rows
            maxRows: root.maxRows
            fallback: root.none
            onPicked: (id, name) => {
                root.close()
                root.picked(id, name)
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
            enabled: list.canJoin
            text: qsTr("Join")
            icon.source: Icons.url("play")
            onClicked: list.pickCurrent()
        }
        Button {
            visible: root.none && root.rows.length > 0
            enabled: list.canJoin
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
