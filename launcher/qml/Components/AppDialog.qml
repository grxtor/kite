// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The launcher's dialog: centred over the window, modal, the title (DialogHeader) on top and a footer with Cancel and
 * one primary button at the right; `footerLeading` takes what belongs at the left (a checkbox, a progress line).
 * Fill `contentItem` or add children as in any Dialog. Clicking the primary button emits primaryClicked() and then
 * closes unless `closeOnPrimary` is false (a dialog that goes on working). A dialog with another footer sets
 * `footer:` itself and keeps the rest.
 */
Dialog {
    id: root

    // sm, md, lg or xl: Theme.layout.dialogSm..dialogXl.
    property string size: "md"
    property int preferredWidth: size === "sm" ? Theme.layout.dialogSm
                               : size === "lg" ? Theme.layout.dialogLg
                               : size === "xl" ? Theme.layout.dialogXl : Theme.layout.dialogMd
    // A header icon shows only in a tone other than the accent: pass iconColor with it.
    property string icon: ""
    property color iconColor: root.danger ? Theme.palette.danger : Theme.palette.accent

    property string primaryText: qsTr("OK")
    // Icon name for the primary button (Icons.url), none when empty.
    property string primaryIcon: ""
    property bool primaryEnabled: true
    // The primary button warns: the dialog guards something that cannot be undone.
    property bool danger: false
    property bool closeOnPrimary: true
    property string cancelText: qsTr("Cancel")
    property bool showCancel: true
    // A press outside closes the dialog; leave false when it holds input.
    property bool closeOnOutsidePress: false
    property alias footerLeading: leadingSlot.data

    signal primaryClicked()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    modal: true
    topPadding: 0
    bottomPadding: Theme.space.sm
    closePolicy: root.closeOnOutsidePress ? Popup.CloseOnEscape | Popup.CloseOnPressOutside : Popup.CloseOnEscape

    header: DialogHeader {
        title: root.title
        icon: root.icon
        iconColor: root.iconColor
    }

    footer: Item {
        implicitWidth: footerRow.implicitWidth + Theme.space.xl * 2
        implicitHeight: footerRow.implicitHeight + Theme.space.xl

        RowLayout {
            id: footerRow
            anchors.fill: parent
            anchors.margins: Theme.space.xl
            anchors.topMargin: 0
            spacing: Theme.space.sm

            Row {
                id: leadingSlot
                Layout.fillWidth: true
                spacing: Theme.space.sm
            }
            Button {
                visible: root.showCancel
                text: root.cancelText
                onClicked: root.close()
            }
            Button {
                text: root.primaryText
                icon.source: root.primaryIcon.length > 0 ? Icons.url(root.primaryIcon) : ""
                highlighted: true
                danger: root.danger
                enabled: root.primaryEnabled
                onClicked: {
                    root.primaryClicked()
                    if (root.closeOnPrimary)
                        root.close()
                }
            }
        }
    }
}
