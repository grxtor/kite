// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Which instance joins a big server of the quick-join bar, when the selected one cannot (its Minecraft is outside
 * the server's versions) or the user asked for another. The instances that fit: pick one (`picked`). None: offer a
 * new instance (`createRequested`) with nothing selected, plus the others to try anyway.
 */
AppDialog {
    id: root

    // A FeaturedServers.servers entry.
    property var server: ({})
    // In words ("1.8 or newer"); "" when every version is let in.
    property string range: ""
    // The selected instance when it does not fit, else null: the dialog says why it is not used.
    property var refused: null
    // QmlShell::instancesForServer(): those that fit, then the others.
    property var instances: []

    readonly property var fitting: root.instances.filter((one) => one.fits)
    readonly property bool none: root.fitting.length === 0
    readonly property var rows: root.none ? root.instances : root.fitting
    readonly property int maxRows: root.none ? 3 : 5

    signal picked(string id, string name)
    signal createRequested()

    size: root.none ? "md" : "sm"
    closeOnOutsidePress: true
    title: root.none ? qsTr("No instance for %1").arg(root.server.name)
                     : qsTr("Join %1 with which instance?").arg(root.server.name)

    // With none that fits nothing is selected: the listed instances are a fallback.
    onAboutToShow: list.currentIndex = root.none ? -1 : list.firstReady()
    onOpened: root.none ? createButton.forceActiveFocus() : list.focusList()

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.Wrap
            // Instance names are the user's own words, not markup.
            textFormat: Text.PlainText
            text: {
                var takes = root.range.length > 0 ? qsTr("%1 takes Minecraft %2.").arg(root.server.name).arg(root.range) : ""
                if (root.none)
                    return qsTr("None of your instances can join %1.").arg(root.server.name) + " " + takes
                if (root.refused)
                    return qsTr("%1 is on Minecraft %2.").arg(root.refused.name).arg(root.refused.version) + " " + takes
                return takes
            }
            tone: "secondary"
            lineHeight: Theme.type.label.lineHeight
        }

        Copy {
            visible: root.none && root.rows.length > 0
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
            text: qsTr("New instance")
            icon.source: Icons.url("plus")
            onClicked: {
                root.close()
                root.createRequested()
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
