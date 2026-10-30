// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What the other looks keep in Home's side panel, as sections of the
 * Sahne page, after "Continue playing": "Set up Kite" (GettingStartedCard,
 * until both its steps are done) and Kite's news (HomeNews). Side by side
 * on a wide page, one over the other on a narrow one; each leaves itself out
 * when it has nothing to say, and so does this.
 */
Grid {
    id: root

    property bool hasInstances: false
    property bool signedIn: false
    // QmlShell.news (NewsModel), or null.
    property var news: null

    signal createRequested()
    signal signInRequested()

    readonly property int setupWidth: 340
    // The news cards are never wider than this when they have the row to
    // themselves.
    readonly property int newsMaxWidth: 620
    readonly property int sideBySideMinWidth: 760
    // Whether there is anything to show; not the children's `visible`,
    // which follows this item's own.
    readonly property bool hasContent: setup.shown || newsList.hasNews
    readonly property bool sideBySide: setup.shown && newsList.hasNews && root.width >= root.sideBySideMinWidth

    columns: root.sideBySide ? 2 : 1
    spacing: Theme.space.xl

    GettingStartedCard {
        id: setup
        width: root.sideBySide ? root.setupWidth : Math.min(root.width, root.setupWidth)
        visible: setup.shown
        hasInstances: root.hasInstances
        signedIn: root.signedIn
        onCreateRequested: root.createRequested()
        onSignInRequested: root.signInRequested()
    }

    HomeNews {
        id: newsList
        width: root.sideBySide ? root.width - root.setupWidth - root.spacing
                               : Math.min(root.width, root.newsMaxWidth)
        news: root.news
    }
}
