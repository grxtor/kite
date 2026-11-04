// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "What's new in Kite 1.3.0": this version's bundled notes (WhatsNew), one short line each beside its icon. Main.qml
 * shows it once after an update, and Settings → About on request. "Great" closes it; a quiet "All notes" opens the
 * release on GitHub. Closing it any way counts as seen.
 */
AppDialog {
    id: root

    // QmlShell.whatsNew, or null.
    property var notes: null

    readonly property string version: root.notes ? root.notes.version : ""
    readonly property string releaseUrl: root.notes ? root.notes.releaseUrl : ""

    size: "md"
    closeOnOutsidePress: true
    //: %1 is a Kite release such as 1.3.0; notes are only written for releases ending in .0.
    title: root.version.length > 0 ? qsTr("What's new in Kite %1").arg(root.version) : qsTr("What's new")
    showCancel: false
    primaryText: qsTr("Great")
    onClosed: if (root.notes) root.notes.markSeen()

    footerLeading: [
        AbstractButton {
            id: allNotes
            objectName: "whatsNewAllNotes"
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            visible: root.releaseUrl.length > 0
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            text: qsTr("All notes")
            implicitWidth: label.implicitWidth
            implicitHeight: Theme.control.heightSm
            Accessible.role: Accessible.Link
            Accessible.name: allNotes.text
            contentItem: Copy {
                id: label
                verticalAlignment: Text.AlignVCenter
                text: allNotes.text
                role: "label"
                color: allNotes.hovered || allNotes.visualFocus ? Theme.palette.textPrimary : Theme.palette.textSecondary
                font.underline: allNotes.hovered
            }
            background: null
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            onClicked: ExternalLinks.open(allNotes, root.releaseUrl, true)
        }
    ]

    // A short window scrolls the list rather than push the buttons off it.
    readonly property real bodyRoom: (root.parent ? root.parent.height : 0) - Theme.space.xxl * 2
                                     - (root.header ? root.header.implicitHeight : 0)
                                     - (root.footer ? root.footer.implicitHeight : 0)
                                     - root.topPadding - root.bottomPadding

    contentItem: Flickable {
        implicitWidth: list.implicitWidth
        implicitHeight: root.bodyRoom > 0 ? Math.min(list.implicitHeight, root.bodyRoom) : list.implicitHeight
        contentWidth: width
        contentHeight: list.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        clip: interactive

        ScrollBar.vertical: ScrollBar { policy: parent.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

        ColumnLayout {
            id: list
            width: parent.width
            spacing: Theme.space.lg

            Repeater {
                model: root.notes ? root.notes.items : []
                delegate: WhatsNewItem {
                    required property var modelData
                    Layout.fillWidth: true
                    iconName: modelData.icon
                    title: modelData.title
                    text: modelData.text
                }
            }
        }
    }
}
