// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Runs the crash doctor's fixes (CrashDoctorModel.causes[i].fixes[j]). Fixer actions go to the
 * model, after the fix's own question when it has one ("Turn off Sodium? The file is not
 * deleted..."). Installs and updates go through the Mods tab's flows, which show what they will
 * download first. Pages and folders are the instance page's to open. Every outcome is a toast; a
 * fix that worked offers Play.
 */
Item {
    id: root

    property var doctor: null
    // InstanceDetails, for its content controller, folders and log.
    property var details: null
    property var shell: null
    property var toast: null
    // The cause whose fix is running, -1 for none, and what its button says meanwhile.
    property int workingCause: -1
    property string workingText: ""
    signal navigateTab(string tab)
    signal openFolder(string path)
    signal uploadRequested()
    signal crashReportRequested(string name)
    signal playRequested()

    readonly property string instanceId: root.details ? root.details.instanceId : ""

    function notify(message, tone) {
        if (root.toast && message.length > 0)
            root.toast.show(message, tone || "neutral")
    }
    function played(message) {
        if (root.toast)
            root.toast.show(message, "success", qsTr("Play"), () => root.playRequested())
    }

    // Fix @p fixIndex of cause @p causeIndex.
    function run(causeIndex, fixIndex) {
        const cause = root.doctor ? root.doctor.causes[causeIndex] : null
        const fix = cause ? cause.fixes[fixIndex] : null
        if (!fix)
            return
        if (fix.runner === "fixer") {
            if (fix.confirm.length > 0) {
                confirm.causeIndex = causeIndex
                confirm.fixIndex = fixIndex
                confirm.text = fix.confirm
                confirm.confirmText = fix.label
                confirm.open()
            } else {
                root.applyNow(causeIndex, fixIndex)
            }
            return
        }
        if (fix.runner === "flow")
            root.runFlow(causeIndex, fix)
        else
            root.navigate(fix)
    }

    function applyNow(causeIndex, fixIndex) {
        const result = root.doctor.applyFix(causeIndex, fixIndex)
        if (result.ok)
            root.played(result.message)
        else
            root.notify(result.message, "danger")
    }

    // A fix parameter as a JS array: one id, or a list of them (a QStringList is no Array).
    function ids(value) {
        if (typeof value === "string")
            return [value]
        const list = []
        for (let i = 0; i < value.length; ++i)
            list.push(String(value[i]))
        return list
    }

    function runFlow(causeIndex, fix) {
        const action = fix.action
        if (action === "installMissingDependency" || action === "updateMod") {
            const wanted = root.ids(action === "updateMod" ? fix.params.mods : fix.params.mod)
            // An update replaces the file there; an install has none to replace.
            const replaces = action === "updateMod" ? root.doctor.fileKeysFor(wanted) : wanted.map(() => "")
            root.workingCause = causeIndex
            root.workingText = qsTr("Looking…")
            installs.installMissing(replaces, wanted, replaces)
        } else if (action === "rematchPerformanceMods") {
            const watcher = root.shell ? root.shell.speedUp(root.instanceId) : null
            if (watcher)
                installs.track(watcher, "")
            else
                root.notify(qsTr("Kite Performance could not be set up for this instance."))
        } else if (action === "updateLoader") {
            root.navigateTab("version")
        } else if (action === "uploadLog") {
            root.upload()
        }
    }

    function navigate(fix) {
        switch (fix.action) {
        case "openModsFolder":
            root.openFolder(root.details.modsDir)
            break
        case "openInstanceFolder":
            root.openFolder(root.details.instanceRoot)
            break
        case "openWorldsFolder":
            root.openFolder(root.details.worldsDir)
            break
        case "openBackups":
            root.navigateTab("backups")
            break
        case "openContentTab":
            root.navigateTab("content")
            break
        case "openJavaSettings":
        case "openMemorySettings":
            root.navigateTab("settings")
            break
        }
    }

    // Asks first: the log leaves the computer.
    function upload() {
        uploadConfirm.open()
    }
    function openCrashReport() {
        if (root.doctor && root.doctor.crashReport.length > 0)
            root.crashReportRequested(root.doctor.crashReportName())
    }
    function undo() {
        const result = root.doctor.undo()
        root.notify(result.message, result.ok ? "success" : "danger")
    }

    ContentUpdateFlows {
        id: installs
        content: root.details ? root.details.content : null
        kind: "mods"
        onNotify: (message, tone) => {
            root.workingCause = -1
            if (tone === "success")
                root.played(message)
            else
                root.notify(message, tone)
        }
        onActivityChanged: if (!installs.activity && !installs.resolving) root.workingCause = -1
        onResolvingChanged: if (!installs.activity && !installs.resolving) root.workingCause = -1
    }

    ConfirmDialog {
        id: confirm
        property int causeIndex: -1
        property int fixIndex: -1
        onConfirmed: root.applyNow(confirm.causeIndex, confirm.fixIndex)
    }

    ConfirmDialog {
        id: uploadConfirm
        danger: false
        title: qsTr("Share the log?")
        text: qsTr("The game's log goes to paste.ee and Kite copies the link for you. Your account details and your user folder are hidden.")
        confirmText: qsTr("Share")
        onConfirmed: root.uploadRequested()
    }
}
