// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The title and one quiet line of explanation over a card or list of settings: the one heading style of a
 * settings group, "Downloaded Java", "More instance folders" and the first-run lists. Hides itself when both are empty.
 */
Column {
    id: root

    property string title
    property string description

    visible: root.title.length > 0 || root.description.length > 0
    spacing: Theme.space.sm

    Copy {
        visible: root.title.length > 0
        text: root.title
        role: "title"
        font.weight: Font.Bold
    }

    Copy {
        visible: root.description.length > 0
        width: parent.width
        text: root.description
        role: "label"
        tone: "tertiary"
        wrapMode: Text.Wrap
    }
}
