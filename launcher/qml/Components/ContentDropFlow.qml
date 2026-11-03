// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * What happens after files or links are dropped on an instance (a ContentDrop): the drop is read, the few questions it
 * raises are asked one after another - a mod loader first, the other version of a mod that is already there, the world
 * a data pack goes into, a running game - and then everything is installed and one toast says what was added.
 * Modpacks and other links are handed to Import (importRequested); project links on Modrinth and CurseForge are
 * installed from the platform. Not visible itself: its dialogs open over the window.
 */
Item {
    id: root

    visible: false

    // text, tone ("success", "warning", "danger", "neutral").
    signal notify(string text, string tone)
    signal importRequested(string source)

    // The ContentDrop being worked on, the answers of prepare() and the check for a missing mod loader (a function
    // taking the action to resume with; true when it took over, false to go on at once).
    property var drop: null
    property var plan: null
    property var loaderGate: null

    // Links being installed, one at a time; what has come back so far.
    property var linkQueue: []
    property var linkResults: []

    function handle(target, urls, gate) {
        if (!target)
            return
        root.drop = target
        root.loaderGate = gate || null
        const found = target.prepare(urls)
        root.plan = found

        const source = found.modpack.length > 0 ? found.modpack : (found.otherLinks.length > 0 ? found.otherLinks[0] : "")
        if (source.length > 0)
            root.importRequested(source)
        if (found.links.length > 0)
            root.installLinks(found.links)

        if (!found.empty || found.unknown.length > 0)
            root.askLoader()
    }

    function dialogOf(loader) {
        loader.active = true
        return loader.item
    }

    function askLoader() {
        if (root.plan.needsLoader && root.loaderGate && root.loaderGate(() => root.askConflicts()))
            return
        root.askConflicts()
    }

    function askConflicts() {
        const conflicts = root.plan.conflicts
        if (conflicts.length === 0) {
            root.askWorld()
            return
        }
        const dialog = root.dialogOf(conflictLoader)
        dialog.text = conflicts.length === 1
            ? qsTr("“%1” is already here as %2. Replace it with the one you dropped, or keep both?")
                  .arg(conflicts[0].name).arg(conflicts[0].existing)
            : qsTr("%1 mods you dropped are already here in another version. Replace them, or keep both?")
                  .arg(conflicts.length)
        dialog.open()
    }

    function askWorld() {
        if (!root.plan.needsWorld) {
            root.askRunning()
            return
        }
        const choices = root.plan.worldChoices
        if (choices.length === 0) {
            root.drop.chooseWorld("")
            root.askRunning()
            return
        }
        const dialog = root.dialogOf(worldLoader)
        dialog.choices = choices
        dialog.open()
    }

    function askRunning() {
        if (!(root.plan.hasWorld && root.plan.running)) {
            root.finish()
            return
        }
        const dialog = root.dialogOf(runningLoader)
        dialog.open()
    }

    function giveUp() {
        if (root.drop)
            root.drop.cancel()
    }

    function finish() {
        const done = root.drop.commit()
        let text = done.text
        let tone = done.tone
        if (root.plan.needsLoader && !root.loaderGate) {
            text += " " + qsTr("This instance has no mod loader yet, so its mods will not load.")
            tone = "warning"
        }
        const task = done.worldTask
        if (!task) {
            root.notify(text, tone)
            return
        }
        // Worlds unpack in the background; the sentence waits for them.
        root.notify(done.worlds === 1 ? qsTr("Adding 1 world…") : qsTr("Adding %1 worlds…").arg(done.worlds), "neutral")
        task.finished.connect((ok) => {
            root.notify(ok ? text : qsTr("The world could not be added."), ok ? tone : "danger")
        })
    }

    function installLinks(links) {
        const idle = root.linkQueue.length === 0
        root.linkQueue = root.linkQueue.concat(links)
        if (idle) {
            root.linkResults = []
            root.notify(qsTr("Looking it up…"), "neutral")
            root.nextLink()
        }
    }

    function nextLink() {
        if (root.linkQueue.length === 0) {
            root.reportLinks()
            return
        }
        const url = root.linkQueue[0]
        root.linkQueue = root.linkQueue.slice(1)
        const install = root.drop ? root.drop.installLink(url) : null
        if (!install) {
            root.linkResults = root.linkResults.concat([{ ok: false, text: qsTr("That link cannot be installed here.") }])
            root.nextLink()
            return
        }
        install.finished.connect((ok, text) => {
            root.linkResults = root.linkResults.concat([{ ok: ok, text: text }])
            root.nextLink()
        })
    }

    function reportLinks() {
        const results = root.linkResults
        if (results.length === 1) {
            root.notify(results[0].text, results[0].ok ? "success" : "danger")
            return
        }
        const added = results.filter((r) => r.ok).length
        const problems = results.filter((r) => !r.ok).map((r) => r.text)
        const head = added === 1 ? qsTr("Added 1 project from the links.")
            : added > 1 ? qsTr("Added %1 projects from the links.").arg(added) : ""
        root.notify([head].concat(problems).filter((t) => t.length > 0).join(" "), problems.length > 0 ? "warning" : "success")
    }

    // A dialog closed any way but through its answer ends the drop.
    Loader {
        id: conflictLoader
        active: false
        sourceComponent: AppDialog {
            id: conflictDialog
            property string text
            property bool answered: false

            title: qsTr("Already installed")
            size: "sm"
            closeOnOutsidePress: true
            primaryText: qsTr("Replace")
            onOpened: conflictDialog.answered = false
            onPrimaryClicked: conflictDialog.answer("replace")
            onClosed: if (!conflictDialog.answered) root.giveUp()

            function answer(mode) {
                conflictDialog.answered = true
                root.drop.resolveConflicts(mode)
                conflictDialog.close()
                root.askWorld()
            }

            footerLeading: [
                Button {
                    text: qsTr("Keep both")
                    onClicked: conflictDialog.answer("keep")
                }
            ]
            contentItem: Copy {
                text: conflictDialog.text
                tone: "secondary"
                wrapMode: Text.Wrap
                lineHeight: 1.3
            }
        }
    }

    Loader {
        id: worldLoader
        active: false
        sourceComponent: AppDialog {
            id: worldDialog
            property var choices: []
            property bool answered: false

            title: qsTr("Add data pack")
            size: "sm"
            closeOnOutsidePress: true
            primaryText: qsTr("Add")
            primaryEnabled: picker.currentIndex >= 0
            onOpened: {
                worldDialog.answered = false
                picker.currentIndex = 0
            }
            onPrimaryClicked: {
                worldDialog.answered = true
                root.drop.chooseWorld(worldDialog.choices[picker.currentIndex].folder)
                root.askRunning()
            }
            onClosed: if (!worldDialog.answered) root.giveUp()

            contentItem: Column {
                spacing: Theme.space.sm

                Copy {
                    width: parent.width
                    text: qsTr("A data pack belongs to a world. Which one should it go into?")
                    tone: "secondary"
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                }
                ComboBox {
                    id: picker
                    width: parent.width
                    model: worldDialog.choices
                    textRole: "name"
                }
            }
        }
    }

    Loader {
        id: runningLoader
        active: false
        sourceComponent: ConfirmDialog {
            id: runningDialog
            property bool answered: false

            title: qsTr("Minecraft is running")
            text: qsTr("Changing a world while Minecraft is running can damage it. Continue anyway?")
            confirmText: qsTr("Continue anyway")
            onOpened: runningDialog.answered = false
            onConfirmed: {
                runningDialog.answered = true
                root.finish()
            }
            onClosed: if (!runningDialog.answered) root.giveUp()
        }
    }
}
