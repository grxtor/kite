// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What Home's hero says about its instance beside the name, label over
 * value as a game library's header has it: when it was last played, how
 * long it has been played in all, and the world it was last in -- read off
 * the recent worlds, and a way to its Worlds tab. A fact with nothing to
 * say is left out; "Last played" always shows.
 */
Row {
    id: root

    property string instanceId: ""
    property var lastLaunch: 0
    // Format.playTime()'s text, or empty to leave the fact out.
    property string playTimeText: ""
    // QmlShell.recentWorlds (RecentWorldsModel), newest first, or null.
    property var worldsModel: null
    // The hero's own colours, which follow its backdrop.
    property color ink: Theme.palette.textPrimary
    property color inkLabel: Theme.palette.textTertiary

    signal worldRequested(string instanceId)

    // The newest of the recent worlds played in this instance, or empty.
    property string worldName: ""
    function findWorld() {
        for (var i = 0; i < worlds.count; ++i) {
            var world = worlds.itemAt(i)
            if (world && world.instanceId === root.instanceId) {
                root.worldName = world.worldName
                return
            }
        }
        root.worldName = ""
    }
    onInstanceIdChanged: Qt.callLater(root.findWorld)

    // Its width whether it is shown or not -- a Row's implicitWidth drops
    // to nothing while hidden -- for a host deciding where it fits.
    readonly property real naturalWidth: {
        var shown = [lastFact, timeFact, worldFact].filter((fact) => fact.says)
        return shown.reduce((sum, fact) => sum + fact.naturalWidth, 0)
               + root.spacing * Math.max(0, shown.length - 1)
    }

    spacing: Theme.space.xxl

    Repeater {
        id: worlds
        model: root.worldsModel
        delegate: Item {
            required property string instanceId
            required property string worldName
            visible: false
        }
        onItemAdded: Qt.callLater(root.findWorld)
        onItemRemoved: Qt.callLater(root.findWorld)
    }

    component Fact: Item {
        id: fact
        property string label
        property string value
        property color ink
        property color inkLabel
        // A fact that leads somewhere: the value lights as a link does.
        property bool link: false
        signal clicked()

        // Whether it has anything to say; it shows only then.
        property bool says: true
        readonly property real naturalWidth: Math.max(labelText.implicitWidth, valueText.implicitWidth)

        visible: fact.says
        implicitWidth: fact.naturalWidth
        implicitHeight: lines.implicitHeight

        Column {
            id: lines
            spacing: Theme.space.xxs

            Text {
                id: labelText
                text: fact.label
                color: fact.inkLabel
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
            Text {
                id: valueText
                text: fact.value
                color: fact.link && linkHover.hovered ? Theme.palette.accentText : fact.ink
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
                font.features: ({ "tnum": 1 })

                Behavior on color {
                    ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
                }
            }
        }

        HoverHandler {
            id: linkHover
            enabled: fact.link
            cursorShape: Qt.PointingHandCursor
        }
        // A MouseArea rather than a TapHandler: it takes the press, so the
        // hero's own tap (open the instance page) does not fire as well.
        MouseArea {
            anchors.fill: parent
            enabled: fact.link
            onClicked: fact.clicked()
        }
    }

    Fact {
        id: lastFact
        label: qsTr("Last played")
        value: Number(root.lastLaunch) > 0 ? Format.lastPlayed(root.lastLaunch) : qsTr("Never")
        ink: root.ink
        inkLabel: root.inkLabel
    }
    Fact {
        id: timeFact
        says: root.playTimeText.length > 0
        label: qsTr("Play time")
        value: root.playTimeText
        ink: root.ink
        inkLabel: root.inkLabel
    }
    Fact {
        id: worldFact
        says: root.worldName.length > 0
        label: qsTr("Last world")
        value: root.worldName
        ink: root.ink
        inkLabel: root.inkLabel
        link: true
        onClicked: root.worldRequested(root.instanceId)
        Accessible.role: Accessible.Link
        Accessible.name: qsTr("Last world: %1, open its worlds").arg(root.worldName)
    }
}
