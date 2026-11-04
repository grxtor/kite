// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * "What is sent": a crash report exactly as it goes to Kite (CrashDoctorModel.payload, from
 * CrashPayload::readable()), in words: the versions, the error lines, the mods, the rule that
 * matched, and what never goes. No surface of its own; CrashSendBlock folds it open.
 */
ColumnLayout {
    id: root

    // {versions, game, lines, mods, modCount, rules, culprit}.
    property var payload: ({})
    readonly property var lines: root.payload.lines || []
    readonly property int modCount: root.payload.modCount || 0

    spacing: Theme.space.sm

    component Field: ColumnLayout {
        id: field
        property alias label: name.text
        property alias value: body.text
        property bool mono: false
        Layout.fillWidth: true
        spacing: Theme.space.xxs
        Copy {
            id: name
            role: "caption"
            tone: "tertiary"
        }
        Copy {
            id: body
            Layout.fillWidth: true
            role: field.mono ? "caption" : "label"
            tone: "secondary"
            // A frame is one long word: code breaks anywhere, so "at" stays on its line.
            wrapMode: field.mono ? Text.WrapAnywhere : Text.WrapAtWordBoundaryOrAnywhere
            textFormat: Text.PlainText
            font.family: field.mono ? Theme.font.mono : Theme.font.family
        }
    }

    Field {
        label: qsTr("Versions")
        value: root.payload.versions + "\n" + root.payload.game
    }
    Field {
        label: qsTr("Error")
        value: root.lines.join("\n")
        mono: true
    }
    Field {
        visible: root.modCount > 0
        label: root.modCount === 1 ? qsTr("1 mod") : qsTr("%1 mods").arg(root.modCount)
        value: root.payload.mods || ""
    }
    Field {
        visible: (root.payload.culprit || "").length > 0
        label: qsTr("Mod it points at")
        value: root.payload.culprit || ""
    }
    Field {
        visible: (root.payload.rules || "").length > 0
        label: qsTr("Rule that matched")
        value: root.payload.rules || ""
    }
    Copy {
        Layout.fillWidth: true
        text: qsTr("Not sent: your account, your Minecraft name, your IP address, your folders, and the names of your instances and worlds.")
        role: "caption"
        tone: "tertiary"
        wrapMode: Text.Wrap
    }
}
