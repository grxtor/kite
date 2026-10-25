// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * What a new instance runs on, as a labelled pick-one list in the list-nav
 * grammar (NavSelectionIndicator slides behind the picked row): Kite
 * Performance first, over a hairline, then the unmodified game and each mod
 * loader with the logo the launcher ships for it. Kite Performance is Fabric
 * with the performance mods; picked, its row opens on the icons of the mods
 * picked for it and offers to edit them (so does a right click on the row),
 * and it is greyed out for a version it has no mods for.
 */
Item {
    id: root

    // NewInstanceDialog.loaders rows: {value, label, iconKey, fit}.
    property var loaders: []
    // The picked row: performanceKind, or a loader's value ("" is vanilla).
    property string current: ""
    // Whether the version picked beside the list has Kite Performance.
    property bool performanceAvailable: true
    // BundleCatalog::mods() rows of the mods picked for Kite Performance.
    property var pickedMods: []

    signal picked(string kind)
    // The performance row's "Edit mods", or a right click on that row.
    signal editModsRequested()

    readonly property string performanceKind: "performance"
    readonly property int logoSize: Theme.icon.md
    // As many icons as fit on the row's one line; the summary names the rest.
    readonly property int maxIcons: 8

    readonly property Item selectedItem: {
        if (root.current === root.performanceKind)
            return performanceRow
        for (var i = 0; i < loaderRepeater.count; ++i) {
            var item = loaderRepeater.itemAt(i)
            if (item && item.modelData.value === root.current)
                return item
        }
        return null
    }

    // "Sodium, Lithium and 6 more": the first names, then how many follow.
    function modSummary() {
        var names = root.pickedMods.map((mod) => mod.name)
        if (names.length <= 2)
            return names.join(", ")
        return qsTr("%1, %2 and %3 more").arg(names[0]).arg(names[1]).arg(names.length - 2)
    }

    implicitWidth: 260
    implicitHeight: rows.implicitHeight

    NavSelectionIndicator {
        target: root.selectedItem
    }

    Column {
        id: rows
        width: parent.width
        spacing: Theme.space.xxs

        KindRow {
            id: performanceRow
            width: rows.width
            label: qsTr("Kite Performance")
            selected: root.current === root.performanceKind
            enabled: root.performanceAvailable
            mods: performanceRow.selected ? root.pickedMods.slice(0, root.maxIcons) : []
            detail: !root.performanceAvailable ? qsTr("Not for this version")
                  : performanceRow.selected ? root.modSummary()
                  : qsTr("Fabric with the performance mods")
            actionText: performanceRow.selected ? qsTr("Edit mods") : ""
            onClicked: root.picked(root.performanceKind)
            onActionTriggered: root.editModsRequested()

            // The row's own menu, as it were: straight to its mods.
            TapHandler {
                enabled: performanceRow.enabled
                acceptedButtons: Qt.RightButton
                onTapped: {
                    root.picked(root.performanceKind)
                    root.editModsRequested()
                }
            }
        }

        // Kite's own pick above, the plain game and its loaders below.
        Item {
            width: rows.width
            height: Theme.space.sm * 2 + 1
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.space.md
                width: parent.width - Theme.space.md * 2
                height: 1
                color: Theme.palette.divider
            }
        }

        Repeater {
            id: loaderRepeater
            model: root.loaders

            delegate: KindRow {
                required property var modelData
                width: rows.width
                label: modelData.label
                iconKey: modelData.iconKey
                fit: modelData.fit
                selected: root.current === modelData.value
                onClicked: root.picked(modelData.value)
            }
        }
    }

    // One row: a logo (or the ⚡) and the label on the first line, then an
    // optional line under the label and the performance mods' icons.
    component KindRow: AbstractButton {
        id: row

        property bool selected: false
        // instanceicon key of the logo; empty for the ⚡.
        property string iconKey: ""
        // How much of the logo box the logo fills (NewInstanceDialog.loaders).
        property real fit: 1.0
        property string label
        property string detail: ""
        property var mods: []
        // A link under the rest, e.g. "Edit mods"; none when empty.
        property string actionText: ""

        signal actionTriggered()

        // The logo or the label, whichever is taller, centred in a
        // control's height on a one-line row.
        readonly property int lineHeight: Math.max(root.logoSize, Math.ceil(Theme.type.body.lineHeightPx))
        readonly property int textX: root.logoSize + Theme.space.md
        readonly property color ink: !row.enabled ? Theme.palette.textTertiary
                                   : row.selected ? Theme.palette.accentText
                                   : row.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary

        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        leftPadding: Theme.space.md
        rightPadding: Theme.space.md
        topPadding: (Theme.control.height - row.lineHeight) / 2
        bottomPadding: row.topPadding
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

        Accessible.role: Accessible.RadioButton
        Accessible.name: row.label
        Accessible.description: row.detail
        Accessible.checked: row.selected

        // Hover and press only; the selection is the indicator behind.
        background: Rectangle {
            radius: Theme.radius.md
            color: row.down ? Theme.palette.pressedOverlay
                 : row.hovered ? Theme.palette.hoverOverlay : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }

        contentItem: Column {
            spacing: Theme.space.xs

            Row {
                height: row.lineHeight
                spacing: Theme.space.md

                Item {
                    width: root.logoSize
                    height: parent.height

                    Image {
                        anchors.centerIn: parent
                        visible: row.iconKey.length > 0
                        width: Math.round(root.logoSize * row.fit)
                        height: width
                        source: row.iconKey.length > 0 ? Icons.instanceIcon(row.iconKey) : ""
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: row.enabled ? 1 : Theme.opacity.disabled
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: row.iconKey.length === 0
                        iconName: "zap"
                        size: root.logoSize
                        color: row.enabled ? Theme.palette.accentText : Theme.palette.textTertiary
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.label
                    color: row.ink
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: row.selected ? Font.DemiBold : Font.Medium
                }
            }

            PerformanceModIcons {
                x: row.textX
                width: parent.width - x
                visible: row.mods.length > 0
                iconSize: Theme.icon.lg
                spacing: Theme.space.xs
                mods: row.mods
            }

            Text {
                x: row.textX
                width: parent.width - x
                visible: row.detail.length > 0
                text: row.detail
                wrapMode: Text.Wrap
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }

            // Its text, not the box it pulls out for the chevron, on the
            // other lines' edge.
            LinkButton {
                id: link
                x: row.textX - link.chevronBearing
                visible: row.actionText.length > 0
                text: row.actionText
                onClicked: row.actionTriggered()
            }
        }
    }
}
