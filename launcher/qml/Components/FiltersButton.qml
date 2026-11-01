// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * The button that opens a filter panel, with how many filters are narrowing the list: "Filters (2)".
 */
Button {
    property int count: 0

    text: count > 0 ? qsTr("Filters (%1)").arg(count) : qsTr("Filters")
    icon.source: Icons.url("filter")
}
