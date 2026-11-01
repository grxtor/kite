// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The Friends tab with nobody on it: what to do next. The add field is on the page above; friends find each other by
 * Minecraft name (and have to use Kite too), so this says so and shows yours, ready to copy (CopyName's chip). An
 * EmptyState like the other tabs', so the three share one type scale and place.
 */
EmptyState {
    id: root

    property string myName

    upperThird: true
    iconName: "users"
    extraShown: root.myName.length > 0
    title: qsTr("No friends yet")
    body: root.myName.length > 0
          ? qsTr("Your friends need Kite too. Add them by their Minecraft name, or send them yours so they can add you:")
          : qsTr("Your friends need Kite too. Add them by their Minecraft name.")

    extra: CopyName {
        chip: true
        value: root.myName
        pixelSize: Theme.type.title.pixelSize
        weight: Theme.type.title.weight
    }
}
