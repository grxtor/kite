// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * An old-format (legacy) instance cannot be played or edited, only upgraded
 * into a new copy -- the widget's "Upgrade is required" page, as one notice
 * over the instance page with the upgrade as its action and its progress
 * along the foot. Shows nothing for any other instance.
 */
NoticeBar {
    id: root

    // InstanceDetails: isLegacy, contentChangesAllowed, upgradeLegacy().
    property var details: null
    // The upgrade in progress (a TaskWatcher), once started here.
    property var upgrade: null
    readonly property bool upgrading: !!root.upgrade && root.upgrade.running
    readonly property bool upgraded: !!root.upgrade && root.upgrade.succeeded
    readonly property bool failed: !!root.upgrade && root.upgrade.failed
    readonly property bool gameRunning: !!root.details && !root.details.contentChangesAllowed

    onDetailsChanged: root.upgrade = null

    visible: !!root.details && root.details.isLegacy
    tone: root.upgraded ? "info" : "warning"
    text: root.upgrading ? qsTr("Upgrading…")
        : root.upgraded ? qsTr("Done. The upgraded copy is in your library.")
        : root.failed ? (root.upgrade.error || qsTr("The upgrade did not work."))
        : root.gameRunning ? qsTr("This instance uses the old format and has to be upgraded before it can be played. Close the game first.")
        : qsTr("This instance uses the old format and has to be upgraded before it can be played. The upgrade makes a new copy and leaves this one as it is.")
    actionText: root.upgrading || root.upgraded || root.gameRunning ? "" : qsTr("Upgrade")
    progress: root.upgrading ? root.upgrade.progress : -2
    onActionTriggered: root.upgrade = root.details.upgradeLegacy()
}
