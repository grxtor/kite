// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One row of PackList: a tick box, the logo, name and where it is from, its
 * version and file (when the table has room for that column), the on/off
 * switch and the ⋯ menu; clicked, it opens the file's details
 * underneath -- description, authors, version, file, where it was
 * installed from -- the way ModFolderPage's info panel showed them.
 *
 * A row of a table, not a card of its own: rows share PackList's card and
 * are split by hairlines; hover and ticked rows tint their whole width.
 *
 * A mod the health check has something to say about (missing or
 * switched-off dependencies, clashes, doubled files) carries a "Problem"
 * or "Warning" badge before its switch, with the reasons in a tooltip and
 * again, written out, at the top of its details.
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

    // The mods folder's health check (ModHealthModel); mods only.
    readonly property var health: owner.kind === "mods" && owner.controller ? owner.controller.health : null
    // What it found about this file, asked again whenever the check is
    // redone (`revision` changes), blocking ones first.
    readonly property var problems: {
        const h = row.health
        if (!h)
            return []
        h.revision
        return h.problemsFor(row.key)
    }
    readonly property bool problemBlocking: {
        const h = row.health
        if (!h)
            return false
        h.revision
        return h.blockingFor(row.key)
    }

    height: header.height + (expanded ? details.implicitHeight + Theme.space.md : 0)
    // "Mod Menu · Modrinth": who made it and where it came from.
    readonly property string byline: {
        const authors = (model.authors || []).join(", ")
        return authors.length > 0 && source.length > 0 ? qsTr("%1 · %2").arg(authors).arg(source)
             : authors.length > 0 ? authors : source
    }
    clip: true

    Rectangle {
        anchors.fill: parent
        color: row.selected ? Theme.palette.accentSubtle
             : hover.hovered ? Theme.palette.surfaceHover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.palette.divider
        visible: !row.lastRow
    }

    // One "label  value" line of the details.
    component FactLabel: Text {
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
    }
    component FactValue: Text {
        Layout.fillWidth: true
        elide: Text.ElideMiddle
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
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
        height: owner.thumbSize + Theme.space.md * 2

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: owner.rowPadding
            anchors.rightMargin: owner.rowPadding
            spacing: Theme.space.md

            CheckBox {
                id: check
                Layout.preferredWidth: owner.checkSize
                padding: 0
                checked: row.selected
                Accessible.name: qsTr("Select %1").arg(row.name)
                onToggled: {
                    owner.toggle(row.key)
                    checked = Qt.binding(() => row.selected)
                }
            }

            // The file's own logo when it has one; otherwise (and
            // while it loads) the kind's glyph.
            RowThumb {
                Layout.preferredWidth: owner.thumbSize
                Layout.preferredHeight: owner.thumbSize
                opacity: row.itemEnabled ? 1 : Theme.opacity.disabled
                source: row.iconUrl
                glyph: owner.iconName
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs
                opacity: row.itemEnabled ? 1 : Theme.opacity.disabled + 0.2
                Text {
                    width: parent.width
                    text: row.name
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }
                // Who made it and where from -- or, with no Version column
                // (a narrow table), its version.
                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: owner.showVersionColumn ? row.byline
                          : (row.version ? String(row.version) : row.byline)
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }

            // Version, over the file it comes from.
            Column {
                visible: owner.showVersionColumn
                Layout.preferredWidth: owner.versionColumnWidth
                Layout.maximumWidth: owner.versionColumnWidth
                spacing: Theme.space.xxs
                opacity: row.itemEnabled ? 1 : Theme.opacity.disabled + 0.2
                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: row.version ? String(row.version) : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.Medium
                }
                Text {
                    width: parent.width
                    text: row.model.fileName || ""
                    elide: Text.ElideMiddle
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }

            // What the health check found; the tooltip lists it. Its slot
            // is as wide as the widest badge (PackList), the badge against
            // the switch.
            Item {
                visible: owner.healthBadgeWidth > 0
                Layout.preferredWidth: owner.healthBadgeWidth
                Layout.preferredHeight: healthBadge.implicitHeight
                Layout.alignment: Qt.AlignVCenter

                StatusBadge {
                    id: healthBadge
                    anchors.right: parent.right
                    visible: row.problems.length > 0
                    text: row.problemBlocking ? qsTr("Problem") : qsTr("Warning")
                    tone: row.problemBlocking ? "danger" : "warning"

                    HoverHandler { id: badgeHover }
                    ToolTip.visible: badgeHover.hovered
                    ToolTip.delay: 400
                    ToolTip.text: row.problems.join("\n")
                }
            }

            HoverReason {
                reason: owner.unlocked ? "" : owner.lockedReason
                Layout.preferredWidth: 40
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

        // What is wrong with it, each in the colour of its severity.
        ColumnLayout {
            Layout.fillWidth: true
            visible: row.problems.length > 0
            spacing: Theme.space.xxs

            Repeater {
                model: row.problems
                Text {
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    wrapMode: Text.Wrap
                    color: row.problemBlocking ? Theme.palette.danger : Theme.palette.warning
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.Medium
                }
            }
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
