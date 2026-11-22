// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The facts line of a search result (ContentResultRow): download count, how long ago it was updated, then its
 * categories as tags. One line, clipped at the row's edge; whatever the provider did not say is left out, and
 * with nothing to say the line keeps its height so rows stay level.
 */
Item {
    id: root

    property var downloads: 0
    property string updatedText: ""
    property var categories: []

    implicitHeight: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.space.xs * 2
    clip: true

    component Fact: Row {
        anchors.verticalCenter: parent.verticalCenter
        property string iconName
        property string text
        visible: text.length > 0
        spacing: Theme.space.xs
        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: parent.iconName.length > 0
            iconName: parent.iconName
            size: Theme.icon.xxs
            color: Theme.palette.textTertiary
        }
        Copy {
            anchors.verticalCenter: parent.verticalCenter
            text: parent.text
            role: "caption"
            tone: "tertiary"
            font.weight: Font.Medium
        }
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space.md

        Fact {
            iconName: "download"
            text: (root.downloads || 0) > 0 ? Format.compactNumber(root.downloads) : ""
        }
        Fact {
            iconName: "clock"
            text: root.updatedText
        }
        Repeater {
            model: root.categories || []
            delegate: Tag {
                required property var modelData
                anchors.verticalCenter: parent.verticalCenter
                text: Format.titleCase(String(modelData).replace(/-/g, " "))
            }
        }
    }
}
