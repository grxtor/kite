// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The Friends tab with nobody on it: not a blank box but what to do next.
 * The add field is on the page above, so this does not point at it again;
 * friends find each other by Minecraft name -- and have to use Kite too --
 * so it says so and shows yours, ready to copy and send (CopyName's chip).
 * It is an EmptyState like the other tabs', so the three share one type
 * scale and place.
 */
EmptyState {
    id: root

    // Your Minecraft name, "" while it is not known.
    property string myName

    upperThird: true
    extraShown: root.myName.length > 0
    title: qsTr("No friends yet")
    body: root.myName.length > 0
          ? qsTr("Your friends need Kite too. Add them by their Minecraft name, or send them yours so they can add you:")
          : qsTr("Your friends need Kite too. Add them by their Minecraft name.")

    SoftIconCircle { iconName: "users" }

    extra: CopyName {
        chip: true
        value: root.myName
        pixelSize: Theme.type.title.pixelSize
        weight: Theme.type.title.weight
    }
}
