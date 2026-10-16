// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme
import MeshMC.Components

ApplicationWindow {
    id: root

    required property var instanceModel
    required property var selection
    // QmlShell: actions (launchInstance, killInstance, ...), account summary
    // and the extra instance models the library needs.
    required property var shell

    readonly property string moduleName: "MeshMC"

    width: 1240
    height: 780
    minimumWidth: 860
    minimumHeight: 540
    title: "MeshMC"
    color: Theme.palette.canvas

    // Whether launcher/qml/Cat was built at all (MESHMC_HAS_CAT): the QML
    // shell has no other way to tell a Quick3D-less build apart from one
    // where the cat is merely turned off, and Settings needs that
    // distinction to hide the cat's rows instead of offering a switch that
    // would do nothing. Set from QmlShell::rootProperties(); false is the
    // right default for QmlModule_test, which never sets it.
    property bool catAvailable: false

    property string selectedId: ""
    // "home", "library", "discover", "settings", "instance" or "accounts".
    property string page: "home"
    // The instance the instance page shows.
    property string openedInstanceId: ""
    property var openedDetails: null

    function openInstance(id) {
        if (!root.shell || typeof root.shell.instanceDetails !== "function")
            return
        root.openedDetails = root.shell.instanceDetails(id)
        root.openedInstanceId = id
        root.selectedId = id
        root.page = "instance"
    }

    Component.onCompleted: {
        if (root.shell && root.shell.settings) {
            SettingsStore.adapter = root.shell.settings
            var mode = SettingsStore.string("UiThemeMode")
            if (mode === "dark" || mode === "light")
                Theme.mode = mode
            var scheme = SettingsStore.string("UiPalette")
            if (scheme === "amethyst" || scheme === "ember" || scheme === "diamond" || scheme === "grass")
                Theme.scheme = scheme
        }
        onboarding.start()
        if (root.devRoute.length > 0)
            Qt.callLater(root.applyDevRoute)
    }

    // Startup route for review snapshots (MESHMC_QML_ROUTE): ";"-separated
    // steps such as "theme=light;scheme=ember;size=1400x900;page=settings;section=java",
    // "instance=<id>;tab=mods", "page=discover;detail=0", "newinstance",
    // "gallery" or "page=home;crashed=<id>" (marks that instance as having
    // crashed last time, for the Home page's chip -- see QmlShell's
    // debugMarkInstanceCrashed()). "nav=<path>" goes through navigate()
    // below ("nav=settings/java", "nav=instance/fabric/settings"), and
    // "import=<path or url>" acts like `meshmc --import` (onImportRequested).
    property string devRoute: ""
    function applyDevRoute() {
        var steps = root.devRoute.split(";")
        for (var i = 0; i < steps.length; ++i) {
            var eq = steps[i].indexOf("=")
            var key = (eq < 0 ? steps[i] : steps[i].slice(0, eq)).trim()
            var value = eq < 0 ? "" : steps[i].slice(eq + 1).trim()
            if (key === "theme")
                Theme.mode = value
            else if (key === "scheme")
                Theme.scheme = value
            else if (key === "size") {
                var wh = value.split("x")
                root.width = parseInt(wh[0])
                root.height = parseInt(wh[1])
            } else if (key === "page")
                root.page = value
            else if (key === "instance")
                root.openInstance(value)
            else if (key === "tab")
                instancePage.tab = value
            else if (key === "section")
                settingsPage.section = value
            else if (key === "select")
                root.selectedId = value
            else if (key === "detail")
                devDetail.start()
            else if (key === "newinstance")
                root.openNewInstance(value)
            else if (key === "nav")
                root.navigate(value)
            else if (key === "import")
                root.openImport(value)
            else if (key === "gallery")
                galleryLoader.active = true
            else if (key === "profilemenu")
                Qt.callLater(() => topBar.openProfileMenu())
            else if (key === "picker")
                Qt.callLater(() => playDock.openPicker())
            else if (key === "sidebar")
                SettingsStore.setValue("UiSidebarCollapsed", value === "collapsed")
            else if (key === "crashed")
                root.call("debugMarkInstanceCrashed", value)
            // Deterministic cat pose for review snapshots -- see
            // CatOverlay.qml's own `demo` property.
            // The icon picker, as the library's "Change icon" opens it.
            else if (key === "iconpicker") {
                iconDialog.targetId = root.dockInstanceId
                iconDialog.current = value.length > 0 ? value : "grass"
                iconDialog.open()
            }
            // Pins Home's header scene to "day", "golden" or "night".
            else if (key === "mood")
                homePage.moodOverride = value
            else if (key === "cat" && catLoader.item)
                catLoader.item.demo = value
        }
    }
    Timer {
        id: devDetail
        interval: 3500
        onTriggered: {
            var parts = root.devRoute.match(/detail=(\d+)/)
            discoverPage.openResult(parts ? parseInt(parts[1]) : 0)
        }
    }
    onSelectedIdChanged: {
        if (selectedId.length > 0)
            root.selection.selectOnly(selectedId)
        else
            root.selection.clear()
    }

    // mode is "create" (default) or "import" -- see NewInstanceDialog.qml.
    // source pre-fills import mode's file/URL field; empty otherwise.
    function openNewInstance(mode, source) {
        if (!root.shell || !root.shell.newInstance)
            return
        newInstanceDialog.controller = root.shell.newInstance
        newInstanceDialog.mode = mode === "import" ? "import" : "create"
        newInstanceDialog.presetSource = source || ""
        newInstanceDialog.open()
    }

    // An import asked for from outside the window: `meshmc --import`, or a
    // second launcher passing a file on (QmlShell::requestImport()). With
    // the dialog already open it takes the new source in place, unless it
    // is still busy with the last one.
    function openImport(source) {
        if (!newInstanceDialog.opened) {
            root.openNewInstance("import", source)
            return
        }
        if (newInstanceDialog.creating) {
            toast.show(qsTr("Another instance is still being set up. Try again once it is in your library."))
            return
        }
        newInstanceDialog.mode = "import"
        newInstanceDialog.setImportSource(source)
    }

    /*
     * Cross-page links: any page can send the user somewhere else in the
     * app with one call (an instance's Java settings pointing at the
     * launcher-wide Java section, say). A page declares a signal such as
     * `signal navigateRequested(string target)` and its handler here
     * forwards it: `onNavigateRequested: (target) => root.navigate(target)`.
     *
     * Targets are paths, given whole or split in two (navigate("settings",
     * "java") is navigate("settings/java")):
     *   "home", "library", "discover", "accounts"
     *   "settings" or "settings/<section>"  a SettingsPage.qml section id
     *                                       ("java", "proxy", "about", ...)
     *   "instance/<id>" or "instance/<id>/<tab>"
     *                                       an InstancePage.qml tab id
     *                                       ("settings", "content", "log", ...)
     * Instance ids are folder names and never contain "/". Returns false,
     * leaving the current page alone, for a path it does not know or an
     * instance that does not exist.
     */
    function navigate(page, sub) {
        var path = sub === undefined || sub === null || String(sub).length === 0
                ? String(page) : page + "/" + sub
        var parts = path.split("/")
        switch (parts[0]) {
        case "home":
        case "library":
        case "discover":
        case "accounts":
            root.page = parts[0]
            return true
        case "settings":
            if (parts.length > 1 && parts[1].length > 0)
                settingsPage.section = parts[1]
            root.page = "settings"
            return true
        case "instance":
            if (parts.length < 2 || parts[1].length === 0 || !root.shell
                    || typeof root.shell.instanceDetails !== "function"
                    || !root.shell.instanceDetails(parts[1]))
                return false
            root.openInstance(parts[1])
            if (parts.length > 2 && parts[2].length > 0)
                instancePage.tab = parts[2]
            return true
        }
        return false
    }

    // Play, everywhere in the shell. With no account at all the classic
    // launch flow would pop a widget dialog; say what is missing instead
    // and go where it is fixed.
    function launch(id) {
        if (root.shell && root.shell.accountCount === 0) {
            root.page = "accounts"
            toast.show(qsTr("Sign in with the Microsoft account that owns Minecraft to play."))
            return
        }
        root.call("launchInstance", id)
    }

    // Home/Library/Discover/Instance keep the play bar; Settings/Accounts
    // hide it -- no "instance to play" on either, and the extra chrome would
    // just crowd two already form-heavy pages. Home needs it too: its own
    // "Jump back in" cards deliberately use a secondary Play, and the dock's
    // Play is what keeps that page down to one accent-filled control.
    function dockVisibleFor(page) {
        return page === "home" || page === "library" || page === "discover" || page === "instance"
    }

    function instanceGroups() {
        var groups = root.shell && root.shell.groups ? root.shell.groups : []
        return [""].concat(groups.filter(g => g.length > 0))
    }

    function call(name, arg) {
        if (root.shell && typeof root.shell[name] === "function")
            arg === undefined ? root.shell[name]() : root.shell[name](arg)
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: topBar.focusSearch()
    }
    // The game's console, asked for by the launch flow (ShowConsole, or a
    // crash): the instance page, on its Log tab.
    Connections {
        target: root.shell
        ignoreUnknownSignals: true
        function onOpenInstanceLog(id) {
            root.openInstance(id)
            instancePage.tab = "log"
        }
        function onImportRequested(source) {
            root.openImport(source)
        }
    }

    Binding {
        target: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
        property: "instanceId"
        value: root.openedInstanceId.length > 0 ? root.openedInstanceId : "/"
        when: !!root.shell && !!root.shell.instancePageModel
    }

    // The play bar's instance: the selection if there is one, otherwise the
    // most recently played instance, otherwise just the first one -- the
    // same fallback the library's old hero card used. heroModel is free for
    // this now that the hero card itself is gone (see LibraryPage.qml).
    readonly property string dockInstanceId: root.selectedId.length > 0 ? root.selectedId
            : (root.shell && root.shell.recentModel && root.shell.recentModel.firstId ? root.shell.recentModel.firstId
            : (root.instanceModel && root.instanceModel.firstId ? root.instanceModel.firstId : ""))

    Binding {
        target: root.shell && root.shell.heroModel ? root.shell.heroModel : null
        property: "instanceId"
        value: root.dockInstanceId
        when: !!root.shell && !!root.shell.heroModel
    }

    // Instance management, shared by the library's card menu and the
    // instance page's own ⋯ menu.
    function askRename(id, name) {
        renameDialog.targetId = id
        renameDialog.value = name
        renameDialog.open()
    }
    function askIcon(id, iconKey) {
        iconDialog.targetId = id
        iconDialog.current = iconKey
        iconDialog.open()
    }
    function askGroup(id, group) {
        groupDialog.targetId = id
        groupDialog.value = group
        groupDialog.suggestions = root.instanceGroups()
        groupDialog.open()
    }
    function askDuplicate(id, name, group) {
        duplicateDialog.targetId = id
        duplicateDialog.targetGroup = group
        duplicateDialog.value = qsTr("%1 (copy)").arg(name)
        duplicateDialog.open()
    }
    function askDelete(id, name) {
        deleteDialog.targetId = id
        deleteDialog.text = qsTr("Delete \u201c%1\u201d? Its folder \u2014 worlds, mods, screenshots \u2014 is removed for good.").arg(name)
        deleteDialog.open()
    }

    Shortcut {
        sequences: [StandardKey.New]
        onActivated: root.openNewInstance()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SidebarNav {
            Layout.fillHeight: true
            Layout.preferredWidth: implicitWidth
            // An icon rail below this saves real width for the content
            // pages on the launcher's own minimum-width window, rather than
            // squeezing the library grid down to one column behind it. ORed
            // with the persisted manual toggle (see SidebarNav's own
            // collapseToggle), so a small window still forces the rail
            // regardless of what was last chosen.
            collapsed: root.width < 1000 || SettingsStore.bool("UiSidebarCollapsed")
            items: [
                { id: "home", icon: "home", label: qsTr("Home") },
                { id: "library", icon: "library", label: qsTr("Library") },
                { id: "discover", icon: "compass", label: qsTr("Discover") }
            ]
            footerItems: [
                { id: "settings", icon: "settings", label: qsTr("Settings") }
            ]
            currentId: root.page === "instance" ? "library" : root.page === "accounts" ? "" : root.page
            barBeside: pageHost.dockVisible
            recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
            onItemActivated: (id) => root.page = id
            onRecentActivated: (id) => {
                root.page = "library"
                root.selectedId = id
            }
            onRecentPlayRequested: (id) => root.launch(id)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            TopBar {
                id: topBar
                Layout.fillWidth: true
                // The instance page has its own banner and way back.
                visible: root.page !== "instance"
                title: root.page === "home" ? qsTr("Home")
                     : root.page === "settings" ? qsTr("Settings")
                     : root.page === "discover" ? qsTr("Discover")
                     : root.page === "accounts" ? qsTr("Accounts") : qsTr("Library")
                count: root.page === "library" && root.instanceModel && root.instanceModel.count !== undefined
                       ? root.instanceModel.count : -1
                searchVisible: root.page === "library"
                searchPlaceholder: qsTr("Search instances")
                onSearchTextChanged: root.instanceModel.filterText = searchText

                accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
                accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
                accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
                accountsController: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                onOpenAccountsRequested: root.page = "accounts"

                ComboBox {
                    id: sortCombo
                    visible: root.page === "library"
                    implicitWidth: 152
                    model: [
                        { value: "Name", label: qsTr("Name") },
                        { value: "LastLaunch", label: qsTr("Last played") },
                        { value: "TotalTimePlayed", label: qsTr("Time played") }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    // InstSortMode is the same launcher-wide setting the
                    // classic Settings page's "Sort instances by" choice
                    // writes -- QmlShell already re-sorts every instance
                    // proxy when it changes, so this needs no plumbing of
                    // its own beyond reading and writing it. Deferred with
                    // callLater: this control completes before root's own
                    // Component.onCompleted has pointed SettingsStore at a
                    // live adapter, so reading the setting here directly
                    // would always see it empty.
                    Component.onCompleted: Qt.callLater(() => sortCombo.currentIndex = Math.max(0, sortCombo.indexOfValue(SettingsStore.string("InstSortMode") || "Name")))
                    onActivated: SettingsStore.setValue("InstSortMode", currentValue)

                    Accessible.name: qsTr("Sort instances by")
                }

                SegmentedControl {
                    visible: root.page === "library"
                    options: [
                        { value: "grid", label: qsTr("Grid") },
                        { value: "list", label: qsTr("List") }
                    ]
                    current: libraryPage.viewMode
                    onActivated: (value) => libraryPage.viewMode = value
                }

                Button {
                    visible: root.page === "library"
                    text: qsTr("New instance")
                    icon.source: Icons.url("plus")
                    onClicked: root.openNewInstance()
                }
            }

            // A page change fades the outgoing page out, swaps
            // StackLayout's currentIndex at the (invisible) midpoint, then
            // fades the incoming one back in with a small upward slide --
            // StackLayout still flips each page's own `visible` at exactly
            // that swap instant, same as a plain binding would, so
            // DiscoverPage's visible-based lazy search is untouched and
            // Layout sizing still comes from the StackLayout underneath.
            Item {
                id: pageHost
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0

                readonly property var pageOrder: ["home", "library", "settings", "discover", "instance", "accounts"]
                // Whether the play bar shows for the page currently on
                // screen -- updated at the same invisible midpoint as
                // pageStack's own currentIndex (see pageTransition below),
                // never straight from root.page, so the bar never appears or
                // disappears while the old page is still visibly fading.
                property bool dockVisible: root.dockVisibleFor(root.page)

                transform: Translate { id: pageSlide }

                SequentialAnimation {
                    id: pageTransition
                    property int nextIndex: 0
                    NumberAnimation { target: pageHost; property: "opacity"; to: 0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
                    PropertyAction { target: pageStack; property: "currentIndex"; value: pageTransition.nextIndex }
                    PropertyAction { target: pageHost; property: "dockVisible"; value: root.dockVisibleFor(pageHost.pageOrder[pageTransition.nextIndex]) }
                    ParallelAnimation {
                        NumberAnimation { target: pageHost; property: "opacity"; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                        NumberAnimation { target: pageSlide; property: "y"; from: Theme.space.sm; to: 0; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                    }
                }

                Connections {
                    target: root
                    function onPageChanged() {
                        pageTransition.nextIndex = pageHost.pageOrder.indexOf(root.page)
                        pageTransition.restart()
                    }
                }

                StackLayout {
                    id: pageStack
                    anchors.fill: parent
                    Component.onCompleted: currentIndex = pageHost.pageOrder.indexOf(root.page)

                    HomePage {
                        id: homePage
                        recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
                        instanceModel: root.instanceModel
                        recentWorldsModel: root.shell && root.shell.recentWorlds ? root.shell.recentWorlds : null
                        accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""

                        onLaunchRequested: (id) => root.launch(id)
                        onStopRequested: (id) => root.call("killInstance", id)
                        onOpenInstanceRequested: (id) => root.openInstance(id)
                        onOpenInstanceWorldsRequested: (id) => {
                            root.openInstance(id)
                            instancePage.tab = "worlds"
                        }
                        onCreateRequested: root.openNewInstance()
                        onDiscoverRequested: root.page = "discover"
                        onLibraryRequested: root.page = "library"
                    }

                    LibraryPage {
                        id: libraryPage
                        focus: true
                        instanceModel: root.instanceModel
                        sectionModelFor: function (group) {
                            return root.shell && typeof root.shell.sectionModel === "function"
                                    ? root.shell.sectionModel(group) : null
                        }
                        searchText: topBar.searchText
                        selectedId: root.selectedId

                        onSelectRequested: (id) => root.selectedId = id
                        onLaunchRequested: (id) => root.launch(id)
                        onStopRequested: (id) => root.call("killInstance", id)
                        onEditRequested: (id) => root.openInstance(id)
                        onFolderRequested: (id) => root.call("openInstanceFolder", id)
                        onCreateRequested: root.openNewInstance()
                        onRenameRequested: (id, name) => root.askRename(id, name)
                        onIconRequested: (id, iconKey) => root.askIcon(id, iconKey)
                        onGroupRequested: (id, group) => root.askGroup(id, group)
                        onDuplicateRequested: (id, name, group) => root.askDuplicate(id, name, group)
                        onDeleteRequested: (id, name) => root.askDelete(id, name)
                        onClearSearchRequested: topBar.searchText = ""
                    }

                    SettingsPage {
                        id: settingsPage
                        languages: root.shell && root.shell.languages ? root.shell.languages : null
                        selectLanguage: function (key) { root.shell.selectLanguage(key) }
                        pluginSurfaces: root.shell && typeof root.shell.pluginSurfaces === "function"
                                        ? root.shell.pluginSurfaces(0, "") : null
                        systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                        // Whether to show the Cat group at all -- see
                        // root.catAvailable's own comment.
                        catAvailable: root.catAvailable
                        onOpenPathRequested: (path) => root.call("openPath", path)
                    }

                    DiscoverPage {
                        id: discoverPage
                        model: root.shell && root.shell.modpackModel ? root.shell.modpackModel : null
                        installer: function (projectId, versionId, name, group) {
                            return root.shell.installModpack(projectId, versionId, name, group)
                        }
                        onShowInstanceRequested: (id) => {
                            root.page = "library"
                            if (id.length > 0)
                                root.selectedId = id
                        }
                        onOtherPlatformsRequested: root.openNewInstance("import")
                    }

                    InstancePage {
                        id: instancePage
                        headerModel: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
                        details: root.openedDetails
                        systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                        accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
                        accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
                        accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
                        accountsController: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                        onOpenAccountsRequested: root.page = "accounts"
                        onBackRequested: root.page = "library"
                        // The header's ⋯ menu manages the instance with the
                        // same dialogs as the library's own card menu.
                        manageable: true
                        onRenameRequested: (id, name) => root.askRename(id, name)
                        onIconRequested: (id, iconKey) => root.askIcon(id, iconKey)
                        onGroupRequested: (id, group) => root.askGroup(id, group)
                        onDuplicateRequested: (id, name, group) => root.askDuplicate(id, name, group)
                        onDeleteRequested: (id, name) => root.askDelete(id, name)
                        // Two arguments -- root.call() only ever forwards
                        // one -- so this goes straight to the shell instead.
                        onJoinServerRequested: (id, address) => { if (root.shell) root.shell.joinServer(id, address) }
                        pluginSurfacesFor: function (anchor, instanceId) {
                            return root.shell && typeof root.shell.pluginSurfaces === "function"
                                   ? root.shell.pluginSurfaces(anchor, instanceId) : null
                        }
                        contentInstaller: function (row, versionId) {
                            return root.shell.installContent(row, versionId)
                        }
                        onOpenPathRequested: (path) => root.call("openPath", path)
                    }

                    AccountsPage {
                        id: accountsPage
                        controller: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                    }
                }

                // The roaming 3D cat (launcher/qml/Cat/CatOverlay.qml), over
                // whichever page is showing. `active` only turns true when
                // the module actually exists (catAvailable, MESHMC_HAS_CAT)
                // and the setting is on, so a build without Qt Quick3D never
                // even resolves the qrc path below -- see the CMake option
                // MeshMC_ENABLE_CAT this all hangs off. Filling pageHost
                // rather than the whole window is what keeps its own
                // walkArea (CatOverlay.qml's default, unset here) right of
                // the sidebar, below the header and above the play bar
                // without this file needing to know any of their sizes.
                Loader {
                    id: catLoader
                    anchors.fill: parent
                    active: root.catAvailable && SettingsStore.bool("CatEnabled")
                    source: active ? "qrc:/qt/qml/MeshMC/Cat/CatOverlay.qml" : ""
                }
            }

            // A genuine row below the page area, not a floating overlay: the
            // StackLayout above shrinks by exactly this bar's height
            // whenever it is visible, so it can never cover a page's
            // content -- including Discover's, whose QML this change does
            // not own. visible follows pageHost.dockVisible rather than
            // root.page directly, so it appears/disappears at the same
            // invisible mid-fade instant the page itself swaps at, instead
            // of jumping the layout while the outgoing page is still
            // visible.
            PlayDock {
                id: playDock
                Layout.fillWidth: true
                visible: pageHost.dockVisible
                rowModel: root.shell && root.shell.heroModel ? root.shell.heroModel : null
                instanceModel: root.instanceModel
                recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
                selectedId: root.selectedId
                onSelectRequested: (id) => root.selectedId = id
                onLaunchRequested: (id) => root.launch(id)
                onStopRequested: (id) => root.call("killInstance", id)
                onCancelRequested: (id) => root.call("killInstance", id)
            }
        }
    }

    NewInstanceDialog {
        id: newInstanceDialog
        iconsModel: root.shell && root.shell.iconsModel ? root.shell.iconsModel : null
        onCreated: root.page = "library"
    }

    // Instance management, from the library's menu.
    PromptDialog {
        id: renameDialog
        property string targetId
        title: qsTr("Rename instance")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            if (root.shell.renameInstance(targetId, text))
                close()
            else
                error = qsTr("That name cannot be used.")
        }
    }

    PromptDialog {
        id: groupDialog
        property string targetId
        title: qsTr("Move to group")
        label: qsTr("Type a new group or pick an existing one.")
        placeholder: qsTr("No group")
        confirmText: qsTr("Move")
        allowEmpty: true
        onSubmitted: (text) => {
            root.shell.setInstanceGroup(targetId, text)
            close()
        }
    }

    PromptDialog {
        id: duplicateDialog
        property string targetId
        property string targetGroup
        property var watcher: null
        title: qsTr("Duplicate instance")
        label: qsTr("A full copy, worlds included, under a new name.")
        confirmText: qsTr("Duplicate")
        onSubmitted: (text) => {
            watcher = root.shell.duplicateInstance(targetId, text, targetGroup)
            close()
            if (watcher)
                toast.show(qsTr("Copying \u201c%1\u201d\u2026").arg(text))
            else
                toast.show(qsTr("The instance could not be copied."), "danger")
        }
        Connections {
            target: duplicateDialog.watcher
            ignoreUnknownSignals: true
            function onFinished(ok) {
                toast.show(ok ? qsTr("Copy ready in your library.")
                              : qsTr("Copy failed: %1").arg(duplicateDialog.watcher.error),
                           ok ? "success" : "danger")
            }
        }
    }

    ConfirmDialog {
        id: deleteDialog
        property string targetId
        title: qsTr("Delete instance")
        confirmText: qsTr("Delete instance")
        onConfirmed: {
            if (root.shell.deleteInstance(targetId)) {
                if (root.selectedId === targetId)
                    root.selectedId = ""
                // Deleted from its own page: there is no page left to show.
                if (root.page === "instance" && root.openedInstanceId === targetId)
                    root.page = "library"
                toast.show(qsTr("Instance deleted."), "success")
            } else {
                toast.show(qsTr("The instance could not be deleted. Is it running?"), "danger")
            }
        }
    }

    IconPickerDialog {
        id: iconDialog
        property string targetId
        iconsModel: root.shell && root.shell.iconsModel ? root.shell.iconsModel : null
        onPicked: (key) => root.shell.setInstanceIcon(targetId, key)
        onOpenFolderRequested: root.call("openPath", root.shell.iconsDir || "icons")
    }

    Toast {
        id: toast
    }

    // Questions the core asks while it works, and its "please wait".
    UiRequestDialog {
        id: uiRequests
        host: root.shell && root.shell.uiHost ? root.shell.uiHost : null
        // Until this exists, the core keeps asking through the classic
        // dialogs rather than waiting on a question nobody can see.
        Component.onCompleted: if (host) host.setPresenterReady(true)
        Component.onDestruction: if (host) host.setPresenterReady(false)
    }

    BusyOverlay {
        busy: root.shell && root.shell.uiHost ? root.shell.uiHost.busy : false
        text: root.shell && root.shell.uiHost ? root.shell.uiHost.busyText : ""
    }

    // First run: language, Java, account -- over everything else.
    OnboardingView {
        id: onboarding
        shell: root.shell
        onSignInRequested: {
            root.page = "accounts"
            accountsPage.startMicrosoftLogin()
        }
    }

    Loader {
        id: galleryLoader
        anchors.fill: parent
        z: 100
        active: false
        sourceComponent: Gallery {}
    }
}
