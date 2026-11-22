// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Putting a cape on the account in `row`, or taking it off: wear() starts AccountsController.changeCape and
 * follows its TaskWatcher. `busy` while it runs, `error` when it did not work, `worn` once a cape is on. Another
 * account in the row (`accountId`) forgets the change that was running.
 */
QtObject {
    id: root

    // AccountsController.
    property var controller: null
    property int row: -1
    property string accountId: ""

    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running
    property string error: ""

    signal worn()

    onAccountIdChanged: { root.watcher = null; root.error = "" }

    // An empty @p capeId takes the cape off.
    function wear(capeId) {
        if (!root.controller || root.busy)
            return
        root.error = ""
        var w = root.controller.changeCape(root.row, capeId)
        if (!w) {
            root.error = qsTr("The cape could not be changed.")
            return
        }
        root.watcher = w
        w.finished.connect(function(ok) {
            if (root.watcher !== w)
                return
            if (!ok)
                root.error = w.error.length > 0 ? w.error : qsTr("The cape could not be changed.")
            else if (capeId.length > 0)
                root.worn()
        })
    }
}
