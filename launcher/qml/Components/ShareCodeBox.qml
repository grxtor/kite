// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A share code as one reads it out, large and in a monospace face, with Copy beside it; Copy says "Copied" for a
 * moment after a click.
 */
BoxSurface {
    id: root

    property string code
    property bool copied: false
    readonly property int copiedMs: 2000

    implicitHeight: row.implicitHeight + Theme.space.lg * 2
    radius: Theme.radius.md

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            text: root.code
            textFormat: Text.PlainText
            font.family: Theme.font.mono
            font.pixelSize: Theme.type.heading.pixelSize
            font.weight: Font.DemiBold
            font.letterSpacing: 1
            elide: Text.ElideRight
            Accessible.name: root.code.split("").join(" ")
        }
        Button {
            text: root.copied ? qsTr("Copied") : qsTr("Copy")
            icon.source: Icons.url(root.copied ? "check" : "copy")
            onClicked: {
                clipboard.copyText(root.code)
                root.copied = true
                copiedTimer.restart()
            }
        }
    }

    Timer {
        id: copiedTimer
        interval: root.copiedMs
        onTriggered: root.copied = false
    }
    ClipboardText { id: clipboard }
}
