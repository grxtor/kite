// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * Which Java runs the game, in one row: a path field, "Choose…" (the
 * installs MeshMC found), Browse, and Test, which runs the Java the way the
 * launcher does (with the JVM arguments and memory of `source`) and shows
 * what came back right under the field.
 *
 * Used for the launcher-wide setting and, inside an OverrideGroup, for one
 * instance (`source` is then that instance's settings). Without
 * JavaStore.controller (the C++ side not wired up) it is the plain field.
 */
SettingRow {
    id: root

    property string key: "JavaPath"
    property string argsKey: "JvmArgs"
    property var source: SettingsStore
    property string placeholder: qsTr("Automatic")
    // Tells a test started here from one started by another page that
    // shares the controller.
    property string token: "java-" + Math.random()

    readonly property var java: JavaStore.controller
    readonly property string path: field.text.trim()
    // What Test last said: { ok, version, architecture, vendor, warnings,
    // error, requiresPermGen }, or null before a test / after the path changed.
    property var result: null
    // Whether the chosen Java is old enough to want the PermGen setting:
    // from the last pick or test of it, else from the version the launcher
    // last saw run. -1 while neither says.
    property int legacyHint: -1
    readonly property bool legacy: legacyHint >= 0
                                   ? legacyHint === 1
                                   : !!root.java && root.java.requiresPermGen(root.source.string("JavaVersion"))
    readonly property bool testing: !!root.java && root.java.testingToken === root.token

    wide: true

    function setPath(newPath, hint) {
        field.text = newPath
        field.cursorPosition = 0
        root.source.setValue(root.key, newPath)
        root.result = null
        root.legacyHint = hint
    }

    function runTest() {
        if (!root.java || root.path.length === 0)
            return
        root.result = null
        root.java.test(root.token, root.path, root.source.string(root.argsKey),
                       root.source.number("MinMemAlloc"), root.source.number("MaxMemAlloc"),
                       root.source.number("PermGen"))
    }

    Connections {
        target: root.java
        ignoreUnknownSignals: true
        function onTestFinished(token, outcome) {
            if (token !== root.token)
                return
            root.result = outcome
            if (outcome.ok)
                root.legacyHint = outcome.requiresPermGen ? 1 : 0
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.space.sm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: root.placeholder
                font.family: Theme.font.mono
                selectByMouse: true
                Accessible.name: root.label

                readonly property string stored: root.source.string(root.key)
                onStoredChanged: if (!activeFocus) { text = stored; cursorPosition = 0 }
                Component.onCompleted: { text = stored; cursorPosition = 0 }
                // A long path starts at its beginning, not its tail.
                onWidthChanged: if (!activeFocus) cursorPosition = 0
                onActiveFocusChanged: if (!activeFocus) cursorPosition = 0
                onEditingFinished: if (text !== stored) {
                    root.source.setValue(root.key, text)
                    root.result = null
                    root.legacyHint = -1
                }
            }
            Button {
                visible: !!root.java
                text: qsTr("Choose…")
                onClicked: picker.open()
            }
            IconButton {
                visible: !!root.java
                iconName: "folder"
                tip: qsTr("Browse for a Java file")
                flat: false
                onClicked: browse.open()
            }
            Button {
                visible: !!root.java
                text: root.testing ? qsTr("Testing…") : qsTr("Test")
                enabled: !root.testing && root.path.length > 0 && !!root.java && root.java.testingToken.length === 0
                onClicked: root.runTest()
            }
        }

        Item {
            Layout.fillWidth: true
            visible: root.testing || root.result !== null
            implicitHeight: outcome.height

            Rectangle {
                id: outcome
                width: parent.width
                height: outcomeText.implicitHeight + Theme.space.md * 2
                radius: Theme.radius.lg
                color: root.testing ? Theme.palette.infoSubtle
                     : root.result && root.result.ok ? Theme.palette.successSubtle
                     : Theme.palette.dangerSubtle

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space.md
                    spacing: Theme.space.sm

                    BusyIndicator {
                        visible: root.testing
                        running: visible
                        Layout.preferredWidth: Theme.icon.md
                        Layout.preferredHeight: Theme.icon.md
                    }
                    MeshIcon {
                        visible: !root.testing
                        iconName: root.result && root.result.ok ? "check" : "alert-triangle"
                        size: Theme.icon.md
                        color: root.result && root.result.ok ? Theme.palette.success : Theme.palette.danger
                    }
                    Text {
                        id: outcomeText
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                        lineHeight: Theme.type.label.lineHeight
                        text: {
                            if (root.testing)
                                return qsTr("Checking this Java…")
                            if (!root.result)
                                return ""
                            if (!root.result.ok)
                                return root.result.error
                            var line = qsTr("Works. Java %1, %2, from %3.")
                                .arg(root.result.version).arg(root.result.architecture).arg(root.result.vendor)
                            return root.result.warnings.length > 0 ? line + "\n" + root.result.warnings : line
                        }
                    }
                }
            }
        }
    }

    JavaPickerDialog {
        id: picker
        currentPath: root.path
        onPicked: (chosen, version) => root.setPath(chosen, root.java.requiresPermGen(version) ? 1 : 0)
    }

    FileDialog {
        id: browse
        title: qsTr("Find Java executable")
        onAccepted: {
            var picked = root.java ? root.java.pickedJava(selectedFile) : ""
            if (picked.length === 0) {
                root.result = { ok: false, error: qsTr("That file cannot be run as Java.") }
                return
            }
            root.setPath(picked, -1)
            root.runTest()
        }
    }
}
