// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One row of PackList: logo (or, while selecting, a checkbox), name and
 * version, the on/off switch and the ⋯ menu; clicked, it opens the file's
 * details underneath -- description, authors, version, file, where it was
 * installed from -- the way ModFolderPage's info panel showed them.
 *
 * A row of a table, not a card of its own: rows share PackList's card and
 * are split by hairlines; hover and ticked rows tint an inset strip.
 */
Item {
    id: row

    // The PackList this row belongs to: selection, kind, lock state.
    required property var owner
    required property int index
    // Through `model`: one of the roles is called "enabled", which
    // as a delegate property would disable the row itself.
    required property var model
    readonly property string key: model.key || ""
    readonly property string name: model.name || ""
    readonly property var version: model.version
    readonly property bool itemEnabled: model.enabled
    readonly property string iconUrl: model.iconUrl || ""
    readonly property string homepage: model.homepage || ""
    // "Modrinth" / "CurseForge" when installed from one.
    readonly property string source: model.source || ""
    readonly property bool selected: owner.isSelected(key)
    readonly property bool expanded: owner.expandedKey === key
    // The last row draws no hairline under itself: the card's border ends it.
    property bool lastRow: false

    height: header.height + (expanded ? details.implicitHeight + Theme.space.md : 0)
    clip: true

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xs
        anchors.rightMargin: Theme.space.xs
        anchors.topMargin: Theme.space.xxs
        anchors.bottomMargin: Theme.space.xxs
        radius: Theme.radius.sm
        color: row.selected ? Theme.palette.accentSubtle
             : hover.hovered ? Theme.palette.surfaceRaised : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space.md
        anchors.rightMargin: Theme.space.md
        height: 1
        color: Theme.palette.divider
        visible: !row.lastRow
    }

    // One "label  value" line of the details.
    component FactLabel: Text {
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
    }
    component FactValue: Text {
        Layout.fillWidth: true
        elide: Text.ElideMiddle
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
    }

    HoverHandler { id: hover }

    // Behind the controls, which take their own clicks first.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                rowMenu.popup()
                return
            }
            if (mouse.modifiers & Qt.ShiftModifier) {
                owner.selectRange(row.key)
            } else if ((mouse.modifiers & Qt.ControlModifier) || owner.selecting) {
                owner.toggle(row.key)
            } else {
                owner.expandedKey = row.expanded ? "" : row.key
            }
        }
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.control.heightLg + Theme.space.md

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.sm
            spacing: Theme.space.md

            // The file's own logo when it has one; otherwise (and
            // while it loads) the kind's glyph. Hovering, or any
            // row being ticked, swaps it for the checkbox.
            Item {
                Layout.preferredWidth: owner.thumbSize
                Layout.preferredHeight: owner.thumbSize

                Rectangle {
                    anchors.fill: parent
                    visible: !check.visible
                    radius: Theme.radius.sm
                    color: Theme.palette.surfaceSunken
                    opacity: row.itemEnabled ? 1 : Theme.opacity.disabled
                    Image {
                        id: logo
                        anchors.fill: parent
                        anchors.margins: owner.thumbInset
                        source: row.iconUrl
                        // The provider scales to exactly this,
                        // keeping pixel-art logos crisp.
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: logo.status !== Image.Ready
                        iconName: owner.iconName
                        size: Theme.icon.sm
                        color: Theme.palette.textTertiary
                    }
                }

                CheckBox {
                    id: check
                    anchors.centerIn: parent
                    padding: 0
                    visible: owner.selecting || hover.hovered || activeFocus
                    checked: row.selected
                    Accessible.name: qsTr("Select %1").arg(row.name)
                    onToggled: {
                        owner.toggle(row.key)
                        checked = Qt.binding(() => row.selected)
                    }
                }
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 1
                Text {
                    width: parent.width
                    text: row.name
                    elide: Text.ElideRight
                    color: row.itemEnabled ? Theme.palette.textPrimary : Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.Medium
                }
                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: row.version ? String(row.version) : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }

            HoverReason {
                reason: owner.unlocked ? "" : owner.lockedReason
                Switch {
                    checked: row.itemEnabled
                    enabled: owner.unlocked
                    Accessible.name: qsTr("Enable %1").arg(row.name)
                    onToggled: {
                        owner.controller.setEnabled(owner.kind, [row.key], checked)
                        checked = Qt.binding(() => row.itemEnabled)
                    }
                }
            }

            IconButton {
                id: moreButton
                iconName: "more"
                tip: qsTr("More")
                onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)
            }
        }
    }

    // What the file says about itself -- ModFolderPage's info panel.
    ColumnLayout {
        id: details
        visible: row.expanded
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.leftMargin: owner.detailsIndent
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.sm

        Text {
            Layout.fillWidth: true
            text: row.model.description || qsTr("No description.")
            wrapMode: Text.Wrap
            maximumLineCount: 6
            elide: Text.ElideRight
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            lineHeight: Theme.type.label.lineHeight
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.space.lg
            rowSpacing: Theme.space.xxs


            FactLabel {
                visible: authorsValue.visible
                text: qsTr("Authors")
            }
            FactValue {
                id: authorsValue
                visible: text.length > 0
                text: (row.model.authors || []).join(", ")
            }
            FactLabel {
                visible: versionValue.visible
                text: qsTr("Version")
            }
            FactValue {
                id: versionValue
                visible: text.length > 0
                text: row.version ? String(row.version) : ""
            }
            FactLabel { text: qsTr("File") }
            FactValue { text: row.model.fileName || "" }
            FactLabel {
                visible: sourceValue.visible
                text: qsTr("Installed from")
            }
            FactValue {
                id: sourceValue
                visible: text.length > 0
                text: row.model.source || ""
            }
        }

        Row {
            visible: homepageButton.visible || configsButton.visible
            spacing: Theme.space.sm
            Button {
                id: homepageButton
                visible: row.homepage.length > 0
                text: qsTr("Homepage")
                icon.source: Icons.url("external-link")
                onClicked: Qt.openUrlExternally(row.homepage)
            }
            Button {
                id: configsButton
                visible: owner.kind === "mods" || owner.kind === "coremods"
                text: qsTr("Configs folder")
                icon.source: Icons.url("folder")
                onClicked: owner.openFolderRequested(owner.controller ? owner.controller.configsDir : "")
            }
        }
    }

    Menu {
        id: rowMenu
        MenuItem {
            enabled: owner.unlocked
            text: row.itemEnabled ? qsTr("Disable") : qsTr("Enable")
            onTriggered: owner.controller.setEnabled(owner.kind, [row.key], !row.itemEnabled)
        }
        MenuItem {
            text: row.expanded ? qsTr("Hide details") : qsTr("Details")
            icon.source: Icons.url("info")
            onTriggered: owner.expandedKey = row.expanded ? "" : row.key
        }
        MenuItem {
            text: row.selected ? qsTr("Deselect") : qsTr("Select")
            icon.source: Icons.url("check")
            onTriggered: owner.toggle(row.key)
        }
        MenuSeparator {}
        MenuItem {
            enabled: row.homepage.length > 0
            text: qsTr("View homepage")
            icon.source: Icons.url("external-link")
            onTriggered: Qt.openUrlExternally(row.homepage)
        }
        MenuItem {
            // A hidden item still takes its row in a Menu; zero it.
            visible: owner.kind === "mods" || owner.kind === "coremods"
            height: visible ? implicitHeight : 0
            text: qsTr("Open configs folder")
            icon.source: Icons.url("folder")
            onTriggered: owner.openFolderRequested(owner.controller ? owner.controller.configsDir : "")
        }
        MenuSeparator {
            visible: owner.updatable
            height: visible ? implicitHeight : 0
        }
        // Both need a recorded origin: where it was installed from.
        MenuItem {
            visible: owner.updatable
            height: visible ? implicitHeight : 0
            enabled: owner.unlocked && row.source.length > 0
            text: qsTr("Change version…")
            icon.source: Icons.url("layers")
            onTriggered: owner.changeVersionRequested(row.key)
        }
        MenuItem {
            visible: owner.updatable
            height: visible ? implicitHeight : 0
            enabled: row.source.length > 0
            text: qsTr("Reset metadata…")
            icon.source: Icons.url("refresh")
            onTriggered: owner.resetMetadataRequested(row.key, row.name)
        }
        MenuSeparator {}
        MenuItem {
            enabled: owner.unlocked
            text: qsTr("Remove…")
            icon.source: Icons.url("trash")
            onTriggered: owner.confirmRemove(row.key, row.name)
        }
    }
}
