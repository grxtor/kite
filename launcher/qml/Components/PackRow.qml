// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One row of PackList: tick box, logo, name and source, version and file (when
 * the table has room), the on/off switch and the ⋯ menu; clicking opens the
 * file's details underneath. A mod the health check flags carries a "Problem"
 * or "Warning" badge before its switch, with the reasons in a tooltip and in
 * the details.
 */
Item {
    id: row

    required property var owner
    required property int index
    // Through `model`: one role is called "enabled", which as a delegate property would disable the
    // row itself.
    required property var model
    readonly property string key: model.key || ""
    readonly property string name: model.name || ""
    readonly property var version: model.version
    readonly property bool itemEnabled: model.enabled
    readonly property string iconUrl: model.iconUrl || ""
    readonly property string homepage: model.homepage || ""
    readonly property string source: model.source || ""
    readonly property bool selected: owner.isSelected(key)
    // The version the last update check found for this file; empty when it found none.
    readonly property string newVersion: {
        const found = owner.updates
        if (!found)
            return ""
        found.count
        return found.newVersionFor(row.key)
    }
    readonly property bool expanded: owner.expandedKey === key
    // The last row draws no hairline under itself: the card's border ends it.
    property bool lastRow: false

    readonly property var health: owner.kind === "mods" && owner.controller ? owner.controller.health : null
    // What the health check found about this file, blocking ones first; re-read when `revision`
    // changes.
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
    readonly property string authorsText: (model.authors || []).join(", ")
    // The description's first line, for the row itself; the whole of it is in the details.
    readonly property string summary: String(model.description || "").split("\n")[0].trim()
    readonly property string byline: {
        const authors = (model.authors || []).join(", ")
        return authors.length > 0 && source.length > 0 ? qsTr("%1 · %2").arg(authors).arg(source)
             : authors.length > 0 ? authors : source
    }
    clip: true

    StateFill {
        id: rowFill
        anchors.fill: parent
        card: true
        rest: "transparent"
        hovered: hover.hovered
        active: row.selected
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.palette.divider
        visible: !row.lastRow
    }

    component FactLabel: Copy {
        role: "label"
        tone: "tertiary"
    }
    component FactValue: Copy {
        Layout.fillWidth: true
        elide: Text.ElideMiddle
        role: "label"
    }

    HoverHandler { id: hover }

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

            RowThumb {
                Layout.preferredWidth: owner.thumbSize
                Layout.preferredHeight: owner.thumbSize
                opacity: row.itemEnabled ? 1 : Theme.opacity.disabled
                source: row.iconUrl
                glyph: owner.iconName
                rounded: true
                // The row's fill is see-through at rest, over the list's card.
                matte: Qt.tint(Theme.palette.surface, rowFill.color)
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs
                opacity: row.itemEnabled ? 1 : Theme.opacity.disabled + 0.2
                // Name, then who made it; under them what it does (or where it came from).
                RowLayout {
                    width: parent.width
                    spacing: Theme.space.sm
                    Copy {
                        Layout.maximumWidth: parent.width
                        text: row.name
                        elide: Text.ElideRight
                        role: "title"
                        font.family: Theme.font.family
                    }
                    Copy {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBaseline
                        visible: owner.showVersionColumn && row.authorsText.length > 0
                        text: qsTr("by %1").arg(row.authorsText)
                        elide: Text.ElideRight
                        role: "label"
                        tone: "tertiary"
                    }
                }
                Copy {
                    id: summaryLine
                    width: parent.width
                    visible: text.length > 0
                    text: owner.showVersionColumn ? (row.summary.length > 0 ? row.summary : row.source)
                          : row.version && row.newVersion.length > 0 ? qsTr("%1 → %2").arg(row.version).arg(row.newVersion)
                          : (row.version ? String(row.version) : row.byline)
                    elide: Text.ElideRight
                    role: "label"
                    tone: "secondary"
                }
            }

            Column {
                visible: owner.showVersionColumn
                Layout.preferredWidth: owner.versionColumnWidth
                Layout.maximumWidth: owner.versionColumnWidth
                spacing: Theme.space.xxs
                opacity: row.itemEnabled ? 1 : Theme.opacity.disabled + 0.2
                Row {
                    width: parent.width
                    // A file with no version shows its file name alone, centred in the row.
                    visible: versionText.visible || updateBadge.visible
                    // As tall as the version alone: the badge is taller and must not push the file name down.
                    height: versionText.implicitHeight
                    spacing: Theme.space.sm
                    Copy {
                        id: versionText
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, parent.width - (updateBadge.visible ? updateBadge.width + parent.spacing : 0))
                        visible: text.length > 0
                        text: row.version ? String(row.version) : ""
                        elide: Text.ElideRight
                        font.weight: Font.Medium
                    }
                    StatusBadge {
                        id: updateBadge
                        anchors.verticalCenter: parent.verticalCenter
                        // An update found while the row is on screen pops in.
                        readonly property bool shown: row.newVersion.length > 0
                        visible: updateBadge.shown
                        opacity: updateBadge.shown ? 1 : 0
                        scale: updateBadge.shown ? 1 : Theme.motion.appearScale
                        Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                        Behavior on scale { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot } }
                        tone: "success"
                        text: qsTr("→ %1").arg(row.newVersion)
                    }
                }
                Copy {
                    width: parent.width
                    text: row.model.fileName || ""
                    elide: Text.ElideMiddle
                    role: "label"
                    tone: "tertiary"
                }
            }

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
                visible: owner.updateSlotWidth > 0
                reason: owner.unlocked ? "" : owner.lockedReason
                Layout.preferredWidth: owner.updateSlotWidth - Theme.space.md
                Layout.alignment: Qt.AlignVCenter
                Button {
                    anchors.right: parent.right
                    visible: row.newVersion.length > 0
                    enabled: owner.unlocked
                    buttonSize: "sm"
                    text: qsTr("Update")
                    Accessible.name: qsTr("Update %1").arg(row.name)
                    onClicked: owner.updateRequested(row.key)
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

    ColumnLayout {
        id: details
        visible: row.expanded
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.leftMargin: owner.detailsIndent
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.sm

        Copy {
            Layout.fillWidth: true
            // Not when the row's own line already says all of it.
            visible: text.length > 0 && (text !== summaryLine.text || summaryLine.truncated)
            text: row.model.description || ""
            wrapMode: Text.Wrap
            maximumLineCount: 6
            elide: Text.ElideRight
            role: "label"
            tone: "secondary"
            lineHeight: Theme.type.label.lineHeight
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: row.problems.length > 0
            spacing: Theme.space.xxs

            Repeater {
                model: row.problems
                Copy {
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    wrapMode: Text.Wrap
                    role: "label"
                    tone: row.problemBlocking ? "danger" : "warning"
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
                onClicked: ExternalLinks.open(row, row.homepage)
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
            onTriggered: ExternalLinks.open(row, row.homepage)
        }
        MenuEntry {
            visible: owner.kind === "mods" || owner.kind === "coremods"
            text: qsTr("Open configs folder")
            icon.source: Icons.url("folder")
            onTriggered: owner.openFolderRequested(owner.controller ? owner.controller.configsDir : "")
        }
        MenuDivider {
            visible: owner.updatable
        }
        MenuEntry {
            visible: owner.updatable
            enabled: owner.unlocked && row.source.length > 0
            text: qsTr("Change version…")
            icon.source: Icons.url("layers")
            onTriggered: owner.changeVersionRequested(row.key)
        }
        MenuEntry {
            visible: owner.updatable
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
