// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Files and links dropped on a page that is no instance's (Home, Discover, Settings). read() sorts them
 * (QmlShell.sortDrop) into what the drop will do, which is also what the overlay says while they are dragged:
 *   "content"       mods, packs, worlds: asked which instance takes them (the only one is not asked), then
 *                   ContentDropFlow adds them there. Wins over a modpack in the same drop.
 *   "needsInstance" the same with an empty library: nothing to add them to, a toast says so.
 *   "import"        a modpack file or a link: Import takes the first, a toast counts the ones left out.
 *   "none"          nothing Kite knows.
 * Not visible itself; its one dialog opens over the window.
 */
Item {
    id: root

    visible: false

    // QmlShell: sortDrop(), dropTargets(), contentDrop().
    property var shell: null
    // The window's ContentDropFlow.
    property var contentFlow: null

    signal notify(string text, string tone)
    signal importRequested(string source)

    // { mode, title, iconName, rows, unknown, content, imports, targets }
    function read(urls) {
        // A drag that cannot be sorted (nothing readable yet, no shell) goes to Import as it is.
        const sortable = !!root.shell && typeof root.shell.sortDrop === "function" && urls.length > 0
        const found = sortable ? root.shell.sortDrop(urls) : ({ rows: [], unknown: "", content: [], imports: [] })
        const targets = root.shell && typeof root.shell.dropTargets === "function" ? root.shell.dropTargets() : []
        const plan = { rows: found.rows, unknown: found.unknown, content: found.content, imports: found.imports,
                       targets: targets, iconName: "download" }
        if (found.content.length > 0 && targets.length > 0) {
            plan.mode = "content"
            // A modpack dropped along is left out; so is its line.
            plan.rows = found.rows.filter((row) => row.kind !== "modpacks")
            plan.title = targets.length === 1 ? qsTr("Drop to add to %1").arg(targets[0].name) : qsTr("Drop to add to…")
        } else if (found.imports.length > 0 || !sortable) {
            plan.mode = "import"
            plan.rows = found.rows.filter((row) => row.kind === "modpacks")
            plan.unknown = ""
            plan.title = qsTr("Drop to import as a new instance")
        } else if (found.content.length > 0) {
            plan.mode = "needsInstance"
            plan.iconName = "cube"
            plan.title = qsTr("Create an instance first")
        } else {
            plan.mode = "none"
            plan.iconName = "x"
            plan.title = qsTr("Nothing here to add")
        }
        return plan
    }

    function drop(urls) {
        const plan = root.read(urls)
        switch (plan.mode) {
        case "content":
            if (plan.targets.length === 1) {
                root.addTo(plan.targets[0].id, plan.content)
                return
            }
            pickerLoader.active = true
            pickerLoader.item.plan = plan
            pickerLoader.item.open()
            return
        case "needsInstance":
            root.notify(qsTr("Create an instance first, then drop these on it."), "warning")
            return
        case "import":
            if (plan.imports.length === 0 && urls.length === 0)
                return
            root.importRequested(plan.imports.length > 0 ? plan.imports[0] : urls[0])
            if (plan.imports.length === 2)
                root.notify(qsTr("Importing the first pack. 1 other was skipped."), "neutral")
            else if (plan.imports.length > 2)
                root.notify(qsTr("Importing the first pack. %1 others were skipped.").arg(plan.imports.length - 1), "neutral")
            return
        }
        root.notify(plan.unknown.length > 0 ? plan.unknown + "." : qsTr("Nothing here to add"), "warning")
    }

    function addTo(id, urls) {
        if (root.contentFlow && root.shell)
            root.contentFlow.handle(root.shell.contentDrop(id), urls, null)
    }

    Loader {
        id: pickerLoader
        active: false
        sourceComponent: AppDialog {
            id: picker
            property var plan: ({ rows: [], targets: [], content: [] })

            title: qsTr("Add to which instance?")
            size: "sm"
            closeOnOutsidePress: true
            primaryText: qsTr("Add")
            primaryEnabled: list.canJoin
            onAboutToShow: list.currentIndex = 0
            onOpened: list.focusList()
            onPrimaryClicked: list.pickCurrent()

            contentItem: ColumnLayout {
                spacing: Theme.space.md

                Copy {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: picker.plan.rows.map((row) => row.text).join(", ")
                    tone: "secondary"
                    wrapMode: Text.Wrap
                }
                JoinInstanceList {
                    id: list
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    // Every instance takes files, a running one too (ContentDropFlow asks where it matters).
                    rows: picker.plan.targets.map((one) => Object.assign({}, one, { canLaunch: true }))
                    maxRows: 6
                    onPicked: (id, name) => {
                        picker.close()
                        root.addTo(id, picker.plan.content)
                    }
                }
            }
        }
    }
}
