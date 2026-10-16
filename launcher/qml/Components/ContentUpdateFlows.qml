// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The Content tab's update flows, kept out of ContentTab itself: check
 * for updates (UpdateReviewDialog), change one file's version
 * (ChangeVersionDialog), verify dependencies and reset metadata, each on
 * top of ContentUpdateController. Whatever needs reviewing goes through
 * InstallReviewDialog; whatever downloads is `activity` until it ends,
 * and how it went comes out of notify() for the tab's toast.
 */
Item {
    id: root

    // InstanceContentController, and what the tab is showing.
    property var content: null
    property string kind: "mods"
    // Lower-case plural: "mods", "resource packs".
    property string noun: qsTr("mods")

    readonly property var updater: root.content ? root.content.updater : null
    // The download in progress (TaskWatcher), or null.
    property var activity: null
    // A review still working out what it needs, or null.
    property var resolving: null
    // What the tab's progress notice says; empty when nothing is going on.
    readonly property string busyText: root.activity ? (root.activity.status.length > 0 ? root.activity.status : root.activity.title)
                                       : root.resolving ? qsTr("Checking what it needs…") : ""
    readonly property real busyProgress: root.activity ? root.activity.progress : -1

    signal notify(string message, string tone)

    function checkUpdates(keys) {
        if (!root.updater)
            return
        const watcher = root.updater.checkForUpdates(root.kind, keys)
        if (!watcher) {
            root.notify(root.updater.lastError, "neutral")
            return
        }
        updateDialog.show(watcher)
    }

    function changeVersion(key) {
        if (!root.updater)
            return
        if (!root.updater.loadVersions(root.kind, key)) {
            root.notify(root.updater.lastError, "neutral")
            return
        }
        versionDialog.open()
    }

    function verifyDependencies() {
        if (!root.updater)
            return
        const review = root.updater.verifyDependencies()
        if (!review) {
            root.notify(root.updater.lastError, "neutral")
            return
        }
        root.settle(review, qsTr("Installed the missing dependencies."),
                    qsTr("Every dependency is installed."))
    }

    function resetMetadata(keys, name) {
        resetConfirm.keys = keys
        resetConfirm.text = keys.length === 1 && name.length > 0
            ? qsTr("Forget where “%1” came from? It stays installed, but can no longer be updated or changed to another version from here.").arg(name)
            : keys.length === 1
            ? qsTr("Forget where this file came from? It stays installed, but can no longer be updated or changed to another version from here.")
            : qsTr("Forget where %1 %2 came from? They stay installed, but can no longer be updated or changed to another version from here.").arg(keys.length).arg(root.noun)
        resetConfirm.open()
    }

    // Once @p review knows what it needs: straight to the download when
    // there is nothing to ask about, else to InstallReviewDialog.
    // @p nothingText, when set, is said instead of installing a review
    // that found nothing (a dependency check that came back clean).
    function settle(review, doneText, nothingText) {
        root.resolving = review
        const go = function () {
            if (!review.ready || root.resolving !== review)
                return
            root.resolving = null
            if (review.needsReview) {
                reviewDialog.doneText = doneText
                reviewDialog.show(review)
            } else if (nothingText && nothingText.length > 0) {
                review.cancel()
                root.notify(nothingText, "success")
            } else {
                root.track(review.install(), doneText)
            }
        }
        review.stateChanged.connect(go)
        go()
    }

    function track(watcher, doneText) {
        if (!watcher)
            return
        root.activity = watcher
        watcher.finished.connect(function (ok) {
            root.notify(ok ? doneText : (watcher.error.length > 0 ? watcher.error : qsTr("The download did not finish.")),
                        ok ? "success" : "danger")
            if (root.activity === watcher)
                root.activity = null
        })
    }

    UpdateReviewDialog {
        id: updateDialog
        controller: root.updater
        noun: root.noun
        onUpdateStarted: (watcher) => root.track(watcher, qsTr("Updates installed."))
    }

    ChangeVersionDialog {
        id: versionDialog
        controller: root.updater
        onReviewRequested: (review) => root.settle(review, qsTr("Installed %1.").arg(review.title), "")
    }

    InstallReviewDialog {
        id: reviewDialog
        property string doneText: ""
        onInstallStarted: (watcher) => root.track(watcher, reviewDialog.doneText)
    }

    ConfirmDialog {
        id: resetConfirm
        property var keys: []
        title: qsTr("Forget where it came from?")
        confirmText: qsTr("Forget")
        danger: false
        onConfirmed: {
            const n = root.updater ? root.updater.resetMetadata(root.kind, keys) : 0
            root.notify(n === 0 ? qsTr("There was nothing recorded to forget.")
                        : n === 1 ? qsTr("Forgot where that file came from.")
                        : qsTr("Forgot where %1 files came from.").arg(n),
                        n > 0 ? "success" : "neutral")
        }
    }
}
