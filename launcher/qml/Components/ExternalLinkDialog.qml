// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * ExternalLinks' question before a link from someone else leaves the launcher: where it leads in large type (the
 * host, or the address of a mail link), the whole link small under it, and for a web link a box to stop asking.
 */
AppDialog {
    id: root

    // The host or mail address the link leads to, and the whole link.
    property string destination
    property string address
    property bool web: true

    signal confirmed(bool stopAsking)

    objectName: "externalLinkDialog"
    size: "sm"
    closeOnOutsidePress: true
    title: root.web ? qsTr("Open this link?") : qsTr("Write an email?")
    primaryText: root.web ? qsTr("Open link") : qsTr("Write email")
    closeOnPrimary: false
    onPrimaryClicked: {
        root.close()
        root.confirmed(stopAsking.checked)
    }

    contentItem: Column {
        spacing: Theme.space.sm

        Copy {
            width: parent.width
            text: root.web ? qsTr("It opens in your browser, at:") : qsTr("Your mail app opens a new email to:")
            tone: "secondary"
            wrapMode: Text.Wrap
        }
        Copy {
            objectName: "linkDestination"
            width: parent.width
            text: root.destination
            role: "bodyStrong"
            wrapMode: Text.WrapAnywhere
        }
        Copy {
            width: parent.width
            text: root.address
            role: "caption"
            tone: "tertiary"
            elide: Text.ElideMiddle
        }
    }

    footerLeading: [
        CheckBox {
            id: stopAsking
            visible: root.web
            text: qsTr("Don't ask again")
        }
    ]
}
