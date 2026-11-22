// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The arrow keys of the library's cards and rows. Left, Right, Up and Down move the keyboard focus to the
 * neighbouring card by the grid's column count; past a section's edge they go on in the next or previous section
 * (folded ones are stepped over). Home and End jump to the first and the last card. The card that takes the focus is
 * scrolled into the page's view. InstanceSection hands every key press on a card to navigate().
 */
QtObject {
    id: root

    // () -> the InstanceSections that show their cards, top to bottom.
    property var sectionList: null
    // 1 for the list view.
    property int columns: 1
    // The page's scrolling view.
    property Flickable flickable: null

    readonly property int revealMargin: Theme.space.lg

    function reveal(item) {
        var view = root.flickable
        if (!view || !item)
            return
        var top = item.mapToItem(view.contentItem, 0, 0).y
        var bottom = top + item.height
        var limit = Math.max(0, view.contentHeight - view.height)
        if (top - root.revealMargin < view.contentY)
            view.contentY = Math.max(0, top - root.revealMargin)
        else if (bottom + root.revealMargin > view.contentY + view.height)
            view.contentY = Math.min(limit, bottom + root.revealMargin - view.height)
    }

    function focusCard(section, index) {
        var card = section ? section.cardAt(index) : null
        if (!card)
            return false
        card.pointerFocus = false
        card.forceActiveFocus(Qt.TabFocusReason)
        root.reveal(card)
        return true
    }

    // Where a key leads from card @p index of the section at @p at: { at, index }, or null to stay.
    function target(lists, at, index, key) {
        var count = lists[at].itemCount
        var columns = Math.max(1, root.columns)
        var column = index % columns
        // The next section with cards in @p step's direction, -1 for none.
        var beside = (step) => {
            for (var i = at + step; i >= 0 && i < lists.length; i += step) {
                if (lists[i].itemCount > 0)
                    return i
            }
            return -1
        }
        var other
        switch (key) {
        case Qt.Key_Left:
            if (columns === 1)
                return null
            if (index > 0)
                return { at: at, index: index - 1 }
            other = beside(-1)
            return other < 0 ? null : { at: other, index: lists[other].itemCount - 1 }
        case Qt.Key_Right:
            if (columns === 1)
                return null
            if (index < count - 1)
                return { at: at, index: index + 1 }
            other = beside(1)
            return other < 0 ? null : { at: other, index: 0 }
        case Qt.Key_Up:
            if (index - columns >= 0)
                return { at: at, index: index - columns }
            other = beside(-1)
            if (other < 0)
                return null
            // The same column of the section's last row, or that row's last card.
            var last = lists[other].itemCount - 1
            return { at: other, index: Math.min(last, Math.floor(last / columns) * columns + column) }
        case Qt.Key_Down:
            if (index + columns < count)
                return { at: at, index: index + columns }
            // A short last row: its last card.
            if (Math.floor(index / columns) < Math.floor((count - 1) / columns))
                return { at: at, index: count - 1 }
            other = beside(1)
            return other < 0 ? null : { at: other, index: Math.min(column, lists[other].itemCount - 1) }
        case Qt.Key_Home:
            other = at
            for (var i = 0; i < at; ++i) {
                if (lists[i].itemCount > 0) {
                    other = i
                    break
                }
            }
            return { at: other, index: 0 }
        case Qt.Key_End:
            other = at
            for (var j = lists.length - 1; j > at; --j) {
                if (lists[j].itemCount > 0) {
                    other = j
                    break
                }
            }
            return { at: other, index: lists[other].itemCount - 1 }
        }
        return null
    }

    function navigate(section, index, event) {
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier))
            return
        var keys = [Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down, Qt.Key_Home, Qt.Key_End]
        if (keys.indexOf(event.key) < 0)
            return
        var lists = root.sectionList ? root.sectionList() : [section]
        var at = lists.indexOf(section)
        if (at < 0)
            return
        // The arrows are the cards' own from here on, a move or not: the page must not scroll under them.
        event.accepted = true
        var to = root.target(lists, at, index, event.key)
        if (to)
            root.focusCard(lists[to.at], to.index)
    }
}
