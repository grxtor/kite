// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The user's own notes on an instance -- a seed, a server address, which
 * mods to update -- as Prism's Notes page keeps them: one plain text box the
 * size of the tab. Saved a moment after typing stops, and at once when the
 * tab is left or another instance opens, always into the instance the text
 * was typed for.
 */
Item {
    id: root

    // InstanceDetails: its `notes` is read and written here.
    property var details: null

    // The instance the unsaved text belongs to: `details` may already be the
    // next instance by the time the text is saved.
    property var editing: null
    // Set while the text is filled in from the instance, so that is not
    // taken for typing.
    property bool loading: false
    readonly property int saveDelayMs: 800

    function save() {
        if (!saveTimer.running)
            return
        saveTimer.stop()
        if (root.editing)
            root.editing.notes = area.text
    }
    function load() {
        root.loading = true
        area.text = root.details ? root.details.notes : ""
        root.loading = false
        root.editing = root.details
    }

    onDetailsChanged: {
        root.save()
        root.load()
    }
    onVisibleChanged: if (!visible) root.save()
    Component.onCompleted: root.load()
    Component.onDestruction: root.save()

    // Changed elsewhere (the library's own notes, a second window): shown,
    // unless the user is in the middle of typing here.
    Connections {
        target: root.details
        ignoreUnknownSignals: true
        function onNotesChanged() {
            if (!saveTimer.running && area.text !== root.details.notes)
                root.load()
        }
    }

    Timer {
        id: saveTimer
        interval: root.saveDelayMs
        onTriggered: {
            if (root.editing)
                root.editing.notes = area.text
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: area.activeFocus ? Theme.palette.borderStrong : Theme.palette.border

        ScrollView {
            id: scroll
            anchors.fill: parent
            anchors.margins: Theme.space.xs

            TextArea {
                id: area
                placeholderText: qsTr("Seed, server address, which mods to update…")
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                background: null
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                Accessible.name: qsTr("Notes")
                onTextChanged: if (!root.loading) saveTimer.restart()
            }
        }
    }
}
