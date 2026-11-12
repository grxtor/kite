// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The one-time note under Home's quick-join bar after an update: the bar can now be arranged. Opened by the bar,
 * right-aligned under it; however it closes (×, Esc, a press elsewhere) it counts as seen and never comes back
 * (QuickJoinLayout.dismissHint()).
 */
Popup {
    id: root

    // QuickJoinLayout, or null.
    property var layout: null

    readonly property int cardWidth: 320

    width: root.cardWidth
    padding: Theme.space.md
    margins: Theme.space.sm
    // Under the bar, its right edge on the bar's.
    x: parent ? parent.width - root.width : 0
    y: parent ? parent.height + Theme.space.xs : 0
    background: GlassSurface {
        radius: Theme.radius.md
        level: Theme.elevation.md
    }
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: if (root.layout) root.layout.dismissHint()

    contentItem: RowLayout {
        spacing: Theme.space.sm

        Copy {
            objectName: "quickJoinHintText"
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            text: qsTr("Drag these servers to reorder them, right-click one to remove it, or add your own server.")
            role: "label"
            wrapMode: Text.Wrap
            lineHeight: 1.3
        }
        IconButton {
            objectName: "quickJoinHintClose"
            Layout.alignment: Qt.AlignTop
            size: Theme.control.heightSm
            iconName: "x"
            tip: qsTr("Close")
            onClicked: root.close()
        }
    }
}
