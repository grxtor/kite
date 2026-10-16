// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One line of a "here is what will be downloaded" list -- InstallReviewDialog
 * (a project and what it needs) and UpdateReviewDialog (updates found):
 * a checkbox, what it is, one line about it, small tags, and an optional
 * link at its end. A row that cannot be left out (`checkable: false`, the
 * project being installed) shows a download mark where the checkbox goes,
 * so the columns stay lined up.
 */
Rectangle {
    id: root

    property bool checkable: true
    property bool checked: false
    property string title: ""
    property string subtitle: ""
    // The subtitle is a caution ("Another version is already installed").
    property bool subtitleWarning: false
    property var tags: []
    property string linkText: ""
    property string linkUrl: ""
    signal toggled(bool checked)

    implicitHeight: Theme.control.heightLg + Theme.space.sm
    radius: Theme.radius.sm
    color: Theme.palette.surfaceSunken
    border.width: 1
    border.color: Theme.palette.border

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
            Text {
                width: parent.width
                text: root.title
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: Font.Medium
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: root.subtitle
                elide: Text.ElideRight
                color: root.subtitleWarning ? Theme.palette.warning : Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
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
