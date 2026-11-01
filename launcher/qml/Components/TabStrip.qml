// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Page tabs: a row of labels, the current one on a soft pill. Tabs that do not
 * fit move, in order, into a "More" menu at the end; which ones fit depends
 * only on the width, never on the selection. `more: true` tabs always live in
 * the menu; `attention: true` draws the count on an accent pill.
 */
Item {
    id: root

    // [{ id, label, count?, more?, attention? }]
    property var tabs: []
    property string current
    signal activated(string id)

    // Ids of the tabs that did not fit and live in the More menu; recomputed by relayout().
    property var overflowIds: []
    readonly property var overflowTabs: root.tabs.filter((t) => root.overflowIds.indexOf(t.id) >= 0)
    readonly property var currentOverflowTab: root.overflowTabs.find((t) => t.id === root.current) || null

    readonly property int pillPadding: Theme.space.md + Theme.space.xxs

    implicitHeight: Theme.control.height
    implicitWidth: row.implicitWidth

    Accessible.role: Accessible.PageTabList

    // Width the More button needs for any label it may hold, selected, so whichever gets picked fits.
    function moreWidthFor(hiddenIndexes) {
        let label = Math.ceil(moreMetrics.advanceWidth)
        for (let i = 0; i < root.tabs.length; ++i) {
            if (root.tabs[i].more === true || hiddenIndexes.indexOf(i) >= 0)
                label = Math.max(label, repeater.itemAt(i).selectedLabelWidth)
        }
        return label + moreRow.spacing + Theme.icon.sm + root.pillPadding * 2
    }

    // Fits as many tabs as the width allows, in order. The current tab plays no part, so selecting
    // one never moves another.
    function relayout() {
        const n = root.tabs.length
        if (root.width <= 0 || repeater.count !== n)
            return
        const gap = row.spacing
        const free = []
        for (let i = 0; i < n; ++i) {
            if (root.tabs[i].more !== true)
                free.push(i)
        }
        const anyPinned = free.length < n
        const ends = [0]
        for (let j = 0; j < free.length; ++j)
            ends.push(ends[j] + repeater.itemAt(free[j]).implicitWidth + (j > 0 ? gap : 0))

        let shown = free.length
        if (ends[shown] + (anyPinned ? gap + root.moreWidthFor([]) : 0) > root.width) {
            shown = 0
            for (let k = free.length - 1; k > 0; --k) {
                if (ends[k] + gap + root.moreWidthFor(free.slice(k)) <= root.width) {
                    shown = k
                    break
                }
            }
        }

        const hiddenFree = free.slice(shown)
        const hidden = root.tabs.filter((t, i) => t.more === true || hiddenFree.indexOf(i) >= 0)
                                .map((t) => t.id)
        if (JSON.stringify(hidden) !== JSON.stringify(root.overflowIds))
            root.overflowIds = hidden
    }

    // Wait for the repeater to settle before measuring.
    onTabsChanged: Qt.callLater(relayout)
    onWidthChanged: Qt.callLater(relayout)
    Component.onCompleted: Qt.callLater(relayout)

    TextMetrics {
        id: moreMetrics
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Font.DemiBold
        text: qsTr("More")
    }

    Row {
        id: row
        height: parent.height
        spacing: Theme.space.xs

        Repeater {
            id: repeater
            model: root.tabs
            delegate: AbstractButton {
                id: tab
                required property var modelData
                readonly property bool selected: modelData.id === root.current
                readonly property bool hasCount: modelData.count !== undefined && modelData.count >= 0
                readonly property bool attention: tab.hasCount && modelData.count > 0 && modelData.attention === true
                readonly property color inkColor: tab.selected ? Theme.palette.accentText
                                                : tab.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary

                // A tab's slot is its unselected label width, so selecting one never nudges the
                // tabs after it.
                readonly property real labelWidth: Math.ceil(labelMetrics.advanceWidth)
                readonly property real selectedLabelWidth: Math.ceil(selectedMetrics.advanceWidth)

                // A tab in the More menu takes no room, but relayout() still needs its width; a Row
                // measures only visible children, so it comes from the label metrics.
                visible: root.overflowIds.indexOf(modelData.id) < 0
                implicitWidth: labelWidth + (hasCount ? labelRow.spacing + countBox.width : 0)
                               + root.pillPadding * 2
                onImplicitWidthChanged: Qt.callLater(root.relayout)

                TextMetrics {
                    id: labelMetrics
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.Medium
                    text: tab.modelData.label
                }
                TextMetrics {
                    id: selectedMetrics
                    font: moreMetrics.font
                    text: tab.modelData.label
                }

                height: row.height
                hoverEnabled: true
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                onClicked: root.activated(modelData.id)

                contentItem: Row {
                    id: labelRow
                    spacing: Theme.space.sm
                    leftPadding: root.pillPadding
                    Copy {
                        id: label
                        anchors.verticalCenter: parent.verticalCenter
                        width: tab.labelWidth
                        text: tab.modelData.label
                        color: tab.inkColor
                        font.weight: tab.selected ? Font.DemiBold : Font.Medium
                    }
                    Item {
                        id: countBox
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.hasCount
                        width: tab.attention ? Math.max(height, countLabel.implicitWidth + Theme.space.sm) : countLabel.implicitWidth
                        height: tab.attention ? Math.ceil(countLabel.implicitHeight) + Theme.space.xs : countLabel.implicitHeight

                        Rectangle {
                            anchors.fill: parent
                            visible: tab.attention
                            radius: Theme.radius.pill
                            color: Theme.palette.accent
                        }
                        Copy {
                            id: countLabel
                            anchors.centerIn: parent
                            text: tab.hasCount ? tab.modelData.count : ""
                            role: "caption"
                            color: tab.attention ? Theme.palette.textOnAccent
                                 : tab.selected ? tab.inkColor : Theme.palette.textTertiary
                            font.weight: tab.attention ? Font.DemiBold : Font.Medium
                        }
                    }
                }

                background: StateFill {
                    radius: Theme.radius.pill
                    hovered: tab.hovered
                    down: tab.down
                    active: tab.selected

                    FocusFrame { around: Theme.focus.offset; cornerRadius: Theme.radius.pill; visible: tab.visualFocus }
                }
            }
        }

        // Styled as one more tab; while the current tab is in the menu it carries that tab's name
        // and the selected look.
        AbstractButton {
            id: moreButton
            readonly property bool selected: root.currentOverflowTab !== null
            readonly property color inkColor: selected ? Theme.palette.accentText
                                            : hovered || moreMenu.visible ? Theme.palette.textPrimary
                                            : Theme.palette.textSecondary
            visible: root.overflowIds.length > 0
            implicitWidth: moreLabel.implicitWidth + moreRow.spacing + Theme.icon.sm + root.pillPadding * 2
            height: row.height
            hoverEnabled: true
            Accessible.role: Accessible.ButtonMenu
            Accessible.name: selected ? qsTr("%1, more tabs").arg(root.currentOverflowTab.label) : qsTr("More tabs")
            onClicked: moreMenu.visible ? moreMenu.close() : moreMenu.open()

            contentItem: Row {
                id: moreRow
                spacing: Theme.space.xs
                leftPadding: root.pillPadding
                Copy {
                    id: moreLabel
                    anchors.verticalCenter: parent.verticalCenter
                    text: moreButton.selected ? root.currentOverflowTab.label : moreMetrics.text
                    color: moreButton.inkColor
                    font.weight: moreButton.selected ? Font.DemiBold : Font.Medium
                }
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    color: moreButton.inkColor
                }
            }

            background: StateFill {
                radius: Theme.radius.pill
                hovered: moreButton.hovered || moreMenu.visible
                down: moreButton.down
                active: moreButton.selected

                FocusFrame { around: Theme.focus.offset; cornerRadius: Theme.radius.pill; visible: moreButton.visualFocus }
            }

            Menu {
                id: moreMenu
                y: moreButton.height

                Instantiator {
                    model: root.overflowTabs
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.count !== undefined && modelData.count >= 0
                              ? qsTr("%1 (%2)").arg(modelData.label).arg(modelData.count)
                              : modelData.label
                        onTriggered: root.activated(modelData.id)
                    }
                    onObjectAdded: (index, object) => moreMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => moreMenu.removeItem(object)
                }
            }
        }
    }
}
