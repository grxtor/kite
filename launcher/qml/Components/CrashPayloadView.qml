// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "What is sent": a crash report exactly as it goes to Kite (CrashDoctorModel.payload, from
 * CrashPayload::readable()), in words: the versions, the error lines, the mods, the rule that
 * matched, and what never goes. The error lines are code: one line each in a sunken monospace
 * block that scrolls sideways (and down past a few lines), never broken inside a name. CrashSendBlock
 * folds it open.
 */
ColumnLayout {
    id: root

    // {versions, game, lines, mods, modCount, rules, culprit}.
    property var payload: ({})
    readonly property var lines: root.payload.lines || []
    readonly property int modCount: root.payload.modCount || 0
    // About eight lines of code; more scroll.
    readonly property int maxCodeHeight: 160

    spacing: Theme.space.sm

    component Field: ColumnLayout {
        id: field
        property alias label: name.text
        property alias value: body.text
        Layout.fillWidth: true
        spacing: Theme.space.xxs
        Copy {
            id: name
            role: "caption"
            tone: "tertiary"
            textFormat: Text.PlainText
        }
        Copy {
            id: body
            Layout.fillWidth: true
            role: "label"
            tone: "secondary"
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            textFormat: Text.PlainText
        }
    }

    Field {
        label: qsTr("Versions")
        value: root.payload.versions + "\n" + root.payload.game
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space.xxs
        Copy {
            text: qsTr("Error")
            role: "caption"
            tone: "tertiary"
            textFormat: Text.PlainText
        }
        Rectangle {
            Layout.fillWidth: true
            // Room under the last line for the side scroll bar.
            implicitHeight: Math.min(root.maxCodeHeight, code.implicitHeight + Theme.space.sm * 2 + Theme.space.md)
            radius: Theme.radius.sm
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            Flickable {
                id: codeView
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: code.implicitWidth
                contentHeight: code.implicitHeight
                // Shown while there is more to the side, so a cut line reads as one that scrolls.
                ScrollBar.horizontal: ScrollBar {
                    policy: codeView.contentWidth > codeView.width ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                }
                ScrollBar.vertical: ScrollBar {}

                Copy {
                    id: code
                    text: root.lines.join("\n")
                    role: "caption"
                    tone: "secondary"
                    wrapMode: Text.NoWrap
                    textFormat: Text.PlainText
                    font.family: Theme.font.mono
                }
            }
        }
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
        textFormat: Text.PlainText
    }
}
