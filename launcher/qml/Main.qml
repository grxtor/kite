// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Placeholder root of the QML user interface.
 *
 * At this stage the launcher still runs the QtWidgets MainWindow; this exists
 * so that the QML module, its resource prefix and the qmlcachegen step are
 * built and covered by a test from the first commit onwards, rather than
 * appearing all at once later. QmlModule_test instantiates it headlessly.
 */
Item {
    id: root

    // Read by QmlModule_test to prove the component really instantiated
    // rather than silently resolving to a default-constructed Item.
    readonly property string moduleName: "MeshMC"

    implicitWidth: 960
    implicitHeight: 600
}
