// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Decides which rows of `view` (a ListView, GridView or Repeater) make an entrance (RiseIn): the ones built when
 * the list first fills, fills again after being empty, or is handed another model. It then stays open for as long
 * as the staggered entrance takes and closes, so rows built afterwards (scrolled into view, one more item added)
 * do not replay it.
 */
Timer {
    id: root

    property var view: null
    // False where the model is a JS array rebuilt on every change: only an empty list filling counts then.
    property bool watchModel: true

    property bool open: true
    property var seenModel: null
    readonly property int count: root.view ? root.view.count : 0

    // Only a model object has an identity worth comparing; a list or a number reads as a new value every time.
    function identity(model) {
        return model && typeof model === "object" && model.objectName !== undefined ? model : null
    }

    // Called by each RiseIn as its row is built.
    function admit() {
        const model = root.watchModel && root.view ? root.identity(root.view.model) : null
        if (model !== root.seenModel) {
            root.seenModel = model
            root.stop()
            root.open = true
        }
        if (root.open && !root.running)
            root.start()
        return root.open
    }

    interval: Theme.motion.stagger * Theme.motion.staggerRows + Theme.motion.slow
    onCountChanged: {
        if (root.count === 0) {
            root.stop()
            root.open = true
        }
    }
    onTriggered: root.open = false
}
