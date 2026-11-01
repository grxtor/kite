// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * A MenuItem that can be hidden: a hidden item still takes its row in a Menu unless its height is zeroed, so
 * `visible` here gives the row back.
 */
MenuItem {
    height: visible ? implicitHeight : 0
}
