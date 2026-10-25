// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Which catalog mods a Kite Performance instance gets, picked before it is
 * created: a grid of cards in the manner of Lunar Client's mod menu, each
 * switched on or off by a click and saying so in the bar along its foot,
 * with the catalog's categories as filter chips and a search over names and
 * summaries. Turning a mod on turns on what it requires (the catalog's
 * `requires`); turning one off turns off what needs it.
 * Nothing is installed here: Done hands the picked projects back
 * (`picked`), in catalog order, and the caller installs them once the
 * instance exists.
 */
Dialog {
    id: root

    // BundleCatalog::mods() rows to pick from.
    property var mods: []
    // Project ids switched on; a working copy until Done.
    property var picks: []
    // The category chip that is on; "" shows every category.
    property string category: ""

    signal picked(var projects)

    readonly property int preferredWidth: 900
    readonly property int preferredHeight: 680
    readonly property int gap: Theme.space.sm
    // Cards are never narrower than this; the grid takes as many columns
    // as fit.
    readonly property int minCardWidth: 190
    readonly property int cardHeight: 156
    readonly property int barHeight: Theme.control.heightSm - Theme.space.xs
    readonly property int columns: Math.max(1, Math.floor((cards.width + root.gap) / (root.minCardWidth + root.gap)))
    readonly property real cardWidth: (cards.width - root.gap * (root.columns - 1)) / root.columns

    readonly property var defaults: root.mods.filter((mod) => mod.isDefault === true).map((mod) => mod.project)
    readonly property bool atDefaults: root.picks.length === root.defaults.length
                                       && root.defaults.every((project) => root.picks.indexOf(project) >= 0)
    // "All", then each category the rows name, in catalog order.
    readonly property var categoryChips: {
        const chips = [{ value: "", label: qsTr("All") }]
        for (const mod of root.mods) {
            if (!chips.some((chip) => chip.value === mod.category))
                chips.push({ value: mod.category, label: mod.categoryName || mod.category })
        }
        return chips
    }
    readonly property int shownCount: root.mods.filter((mod) => root.matches(mod, search.text)).length

    // Starts on @p projects, every category and an empty search.
    function openWith(projects) {
        root.picks = projects.slice()
        root.category = ""
        search.text = ""
        root.open()
    }

    function modById(project) {
        return root.mods.find((mod) => mod.project === project)
    }
    function requiresOf(project) {
        const mod = root.modById(project)
        return mod && mod.requires ? mod.requires : []
    }
    function matches(mod, text) {
        if (root.category.length > 0 && mod.category !== root.category)
            return false
        const needle = text.trim().toLowerCase()
        return needle.length === 0 || mod.name.toLowerCase().indexOf(needle) >= 0
               || (mod.summary || "").toLowerCase().indexOf(needle) >= 0
    }
    // On: the mod and what it requires. Off: the mod and what needs it.
    function toggle(project) {
        if (root.picks.indexOf(project) >= 0) {
            root.picks = root.picks.filter((id) => id !== project && root.requiresOf(id).indexOf(project) < 0)
            return
        }
        const next = root.picks.concat([project])
        for (const needed of root.requiresOf(project)) {
            if (next.indexOf(needed) < 0)
                next.push(needed)
        }
        root.picks = next
    }
    function namesOf(projects) {
        return projects.map((project) => {
            const mod = root.modById(project)
            return mod ? mod.name : project
        }).join(", ")
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(root.preferredHeight, parent ? parent.height - Theme.space.xxl * 2 : root.preferredHeight)
    modal: true
    topPadding: 0
    bottomPadding: Theme.space.sm
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Kite Performance mods")

    header: DialogHeader {
        title: root.title
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            ChipGroup {
                Layout.fillWidth: true
                options: root.categoryChips
                current: root.category
                onActivated: (value) => root.category = value
            }
            SearchBox {
                id: search
                Layout.preferredWidth: 220
                placeholderText: qsTr("Search mods")
            }
        }

        Flickable {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: cards.height
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Flow {
                id: cards
                width: scroller.width
                spacing: root.gap

                Repeater {
                    model: root.mods
                    delegate: ModCard {}
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Theme.space.xxl
                visible: root.shownCount === 0
                text: qsTr("No mods match.")
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }
    }

    footer: RowLayout {
        spacing: Theme.space.sm

        Text {
            Layout.leftMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            text: qsTr("%1 of %2 on").arg(root.picks.length).arg(root.mods.length)
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
        Button {
            Layout.bottomMargin: Theme.space.xl
            flat: true
            visible: !root.atDefaults
            text: qsTr("Reset to defaults")
            icon.source: Icons.url("refresh")
            onClicked: root.picks = root.defaults.slice()
        }
        Item { Layout.fillWidth: true }
        Button {
            Layout.bottomMargin: Theme.space.xl
            text: qsTr("Cancel")
            onClicked: root.close()
        }
        Button {
            Layout.rightMargin: Theme.space.xl
            Layout.bottomMargin: Theme.space.xl
            highlighted: true
            text: qsTr("Done")
            icon.source: Icons.url("check")
            onClicked: {
                root.picked(root.mods.filter((mod) => root.picks.indexOf(mod.project) >= 0)
                                     .map((mod) => mod.project))
                root.close()
            }
        }
    }

    // One mod: its icon and a Default tag, the name, what it does and what
    // it needs; the bar along the foot says On or Off. A click anywhere
    // switches it.
    component ModCard: AbstractButton {
        id: card

        required property var modelData
        readonly property bool on: root.picks.indexOf(card.modelData.project) >= 0
        readonly property var needs: card.modelData.requires || []

        width: root.cardWidth
        height: root.cardHeight
        visible: root.matches(card.modelData, search.text)
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        padding: Theme.space.md
        bottomPadding: root.barHeight + Theme.space.sm

        Accessible.role: Accessible.CheckBox
        Accessible.name: card.modelData.name
        Accessible.description: card.modelData.summary || ""
        Accessible.checked: card.on

        onClicked: root.toggle(card.modelData.project)

        background: Rectangle {
            radius: Theme.radius.md
            color: card.hovered ? Theme.palette.surfaceHover : Theme.palette.surfaceRaised
            border.width: 1
            border.color: card.visualFocus ? Theme.palette.focusRing
                        : card.hovered ? Theme.palette.borderStrong : Theme.palette.border
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

            // The On/Off bar: one rounded strip inside the border, its top
            // corners clipped off (two strips would double a see-through
            // fill where they overlap).
            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: parent.border.width
                height: root.barHeight
                clip: true

                Rectangle {
                    y: -radius
                    width: parent.width
                    height: parent.height + radius
                    radius: Theme.radius.md - 1
                    color: card.on ? Theme.palette.successSubtle : Theme.palette.surfaceSunken
                }
                Row {
                    anchors.centerIn: parent
                    spacing: Theme.space.xs
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: card.on ? "check" : "x"
                        size: Theme.icon.sm - 4
                        color: card.on ? Theme.palette.success : Theme.palette.textTertiary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: card.on ? qsTr("On") : qsTr("Off")
                        color: card.on ? Theme.palette.success : Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: Theme.space.xs

            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.space.xs
                RowThumb {
                    Layout.preferredWidth: Theme.control.height
                    Layout.preferredHeight: Theme.control.height
                    source: card.modelData.icon || ""
                    matte: Theme.palette.surfaceRaised
                }
                Item { Layout.fillWidth: true }
                Tag {
                    Layout.alignment: Qt.AlignTop
                    visible: card.modelData.isDefault === true
                    text: qsTr("Default")
                }
            }
            Text {
                Layout.fillWidth: true
                text: card.modelData.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: card.modelData.summary || ""
                wrapMode: Text.Wrap
                maximumLineCount: card.needs.length > 0 ? 1 : 2
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
            Text {
                Layout.fillWidth: true
                visible: card.needs.length > 0
                text: qsTr("Needs %1").arg(root.namesOf(card.needs))
                elide: Text.ElideRight
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
            Item { Layout.fillHeight: true }
        }
    }
}
