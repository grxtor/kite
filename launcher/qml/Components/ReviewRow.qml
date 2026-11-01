// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One line of a "here is what will be downloaded" list (InstallReviewDialog,
 * UpdateReviewDialog): a checkbox, what it is, one line about it, small tags and
 * an optional link. A row that cannot be left out (`checkable: false`) shows a
 * download mark where the checkbox goes, so the columns stay lined up.
 */
BoxSurface {
    id: root

    property bool checkable: true
    property bool checked: false
    property string title: ""
    property string subtitle: ""
    property bool subtitleWarning: false
    property var tags: []
    property string linkText: ""
    property string linkUrl: ""
    signal toggled(bool checked)

    implicitHeight: Theme.control.heightLg + Theme.space.sm
    sunken: true
    radius: Theme.radius.sm

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.sm
        anchors.rightMargin: Theme.space.md
        spacing: Theme.space.sm

        Item {
            Layout.preferredWidth: check.implicitWidth
            Layout.preferredHeight: check.implicitHeight
            CheckBox {
                id: check
                visible: root.checkable
                checked: root.checked
                Accessible.name: root.title
                onToggled: {
                    root.toggled(checked)
                    checked = Qt.binding(() => root.checked)
                }
            }
            MeshIcon {
                anchors.centerIn: parent
                visible: !root.checkable
                iconName: "download"
                size: Theme.icon.sm
                color: Theme.palette.accentText
            }
        }

        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 1
            Copy {
                width: parent.width
                text: root.title
                elide: Text.ElideRight
                font.weight: Font.Medium
            }
            Copy {
                width: parent.width
                visible: text.length > 0
                text: root.subtitle
                role: "caption"
                tone: root.subtitleWarning ? "warning" : "tertiary"
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: root.tags
            Tag {
                required property var modelData
                text: modelData
            }
        }

        Button {
            visible: root.linkUrl.length > 0
            flat: true
            text: root.linkText
            icon.source: Icons.url("external-link")
            onClicked: Qt.openUrlExternally(root.linkUrl)
        }
    }
}
